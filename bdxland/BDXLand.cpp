// BDXLand.cpp : 定义 DLL 的导出函数。
//

#include "pch.h"
#include "BDXLand.h"
typedef int s32;
typedef int64_t s64;
typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
static Logger LOG(stdio_commit{ "[LAND] " });
static LangPack LP("langpack/land.json");
#define to_lpos(x) ((x) ^ 0x80000000)
namespace LandImpl {
	using std::list;
	using std::swap;
	using std::unordered_map;
	static std::unique_ptr<KVDBImpl> db;
	typedef u32 lpos_t;
	typedef u64 xuid_t;
	enum LandPerm : u16 {
		NOTHING = 0,   //no perm specified , must be owner to operate
		PERM_INTERWITHACTOR = 1,
		PERM_USE = 2,  //null hand place
		PERM_ATK = 4,  //attack entity
		PERM_BUILD = 8, //nonnull hand place
		PERM_DESTROY=16
	};

	struct FastLand {
		lpos_t x, z, dx, dz; //4*int
		u32 lid; //5*int
		u32 refcount; //6*int
		u16 dim;
		LandPerm perm; //7*int
		u32 owner_sz; //8*int
		xuid_t owner[0];
		inline int getOPerm(xuid_t x) const {
			u32 owner_sz2 = owner_sz >> 3;
			for (u32 i = 0; i < owner_sz2; ++i) {
				if (owner[i] == x) return 1 + (i == 0);
			}
			return 0;
		}
		inline bool hasPerm(xuid_t x, LandPerm PERM) const {
			return ((perm & PERM) != 0) || (getOPerm(x) > 0);
		}
		inline string_view getOwnerView() const {
			return { (char*)owner,owner_sz };
		}
		inline array_view<xuid_t> OWNER() const {
			return { owner,owner_sz>>3 };
		}
		inline string ownStr() const{
			string rv;
			u32 owner_sz2 = owner_sz >> 3;
			for (u32 i = 0; i < owner_sz2; ++i) {
				auto name=XIDREG::id2str(owner[i]);
				if (name.set) {
					rv += name.val();
					rv += ',';
				}
			}
			if (rv.size()) rv.pop_back();
			return rv;
		}
	};
	static_assert(sizeof(FastLand) == sizeof(u32) * 8);
	static_assert(offsetof(FastLand, owner) == sizeof(u32) * 8);
	static_assert(offsetof(FastLand, perm) == sizeof(u32) * 6 + sizeof(u16));
	struct DataLand {
		lpos_t x, z, dx, dz; //4*int
		u32 lid; //5*int
		u32 spec; //6*int
		u16 dim;
		LandPerm perm; //7*int
		string owner;
		DataLand() {}
		DataLand(FastLand const& fastland) {
			x = fastland.x;
			z = fastland.z;
			dx = fastland.dx;
			dz = fastland.dz;
			lid = fastland.lid;
			spec = fastland.refcount;
			dim = fastland.dim;
			perm = fastland.perm;
			owner = fastland.getOwnerView();
		}
		void addOwner(xuid_t x, bool SuperOwner = false) {
			if (SuperOwner) owner.insert(0, to_view(x)); else owner.append(to_view(x));
		}
		void delOwner(xuid_t x) {
			size_t pos = owner.find(to_view(x));
			if (pos != owner.npos)
				owner.erase(pos, sizeof(x));
		}
		string serialize() const {
			WBStream ws;
			ws.apply(x, z, dx, dz, lid, spec, dim, perm, owner);
			return ws.data;
		}
	};
	static inline void __fail(const string& c) {
		printf(c.c_str());
		exit(1);
	}
	namespace LandCacheManager {
		static unordered_map<u32, FastLand*> cache;
		static void reset() {
			for (auto [k, v] : cache) {
				free(v);
			}
			cache.clear();
		}
		static void noticeFree(u32 id) {
			auto it = cache.find(id);
			if (it == cache.end()) {
				__fail("double free " + S(id));
			}
			it->second->refcount--;
			if (it->second->refcount == 0) {
				free(it->second);
				cache.erase(it);
			}
		}
		static FastLand* requestLand(u32 id) {
			auto it = cache.find(id);
			if (it == cache.end()) {
				string landstr;
				db->get(to_view(id), landstr);
				FastLand* res = (FastLand*)malloc(landstr.size());
				if (!res) {
					__fail("bad_alloc");
				}
				memcpy(res, landstr.data(), landstr.size());
				res->refcount = 1;
				cache[id] = res;
				return res;
			}
			else {
				it->second->refcount++;
				return it->second;
			}
		}
	};
	struct ChunkLandManager {
		FastLand* lands[16][16];
		u32 mLandsCnt;
		u32 managed_lands[256];
		~ChunkLandManager() {
			for (u32 i = 0; i < mLandsCnt; ++i)
				LandCacheManager::noticeFree(managed_lands[i]);
		}
		ChunkLandManager(u32* landlist, u32 siz, lpos_t xx, lpos_t zz) {
			memset(lands, 0, sizeof(lands));
			mLandsCnt = 0;
			for (u32 I = 0; I < siz; ++I) {
				auto fl = LandCacheManager::requestLand(landlist[I]);
				managed_lands[mLandsCnt++] = landlist[I];
				lpos_t sx, dx, sz, dz;
				if ((fl->x >> 4) == xx)
					sx = fl->x & 15;
				else
					sx = 0;
				if ((fl->z >> 4) == zz)
					sz = fl->z & 15;
				else
					sz = 0;
				if ((fl->dx >> 4) == xx)
					dx = fl->dx & 15;
				else
					dx = 15;
				if ((fl->dz >> 4) == zz)
					dz = fl->dz & 15;
				else
					dz = 15;
				for (lpos_t i = sx; i <= dx; ++i) {
					for (lpos_t j = sz; j <= dz; ++j) { lands[i][j] = fl; }
				}
			}
		}
	};

	static inline u64 ChunkID(lpos_t x, lpos_t z, int dim) {
		u32 ids[2];
		ids[0] = x;
		ids[1] = z << 4;
		return (*(u64*)(&ids)) | dim;
	}
	static ChunkLandManager EMPTYC(nullptr, 0, 0, 0);
	template<typename TP,size_t CAP=16>
	struct pool_allocator {
		static_queue<TP*, CAP> q;
		inline TP* _get() {
			if (!q.empty()) {
				auto rv = q.back();
				q.pop_back();
				return rv;
			}
			return (TP*)malloc(sizeof(TP));
		}
		inline void _put(TP* p) {
			if (!q.full()) q.push_back(p);
			else free(p);
		}
		template<typename... T>
		TP* alloc(T&&... args) {
			TP* r = _get();
			new (r)TP(std::forward<T>(args)...);
			return r;
		}
		void dealloc(TP* t) {
			t->~TP();
			_put(t);
		}
	};
	static pool_allocator<ChunkLandManager> CLDP;
	struct ChunkDataWrapped
	{
		ChunkLandManager* v;
		ChunkDataWrapped() {
			v = &EMPTYC;
		}
		ChunkDataWrapped(lpos_t x, lpos_t z, int dim) {
			string LList;
			u64 CID = ChunkID(x, z, dim);
			if (db->get(to_view(CID), LList)) {
				v = CLDP.alloc((u32*)LList.data(), (u32)LList.size() / sizeof(u32), x, z);//new ChunkLandManager((u32*)LList.data(), (u32)LList.size() / sizeof(u32), x, z);
			}
			else {
				v = &EMPTYC;
			}
		}
		~ChunkDataWrapped() {
			if (v != &EMPTYC) {
				//delete v;
				CLDP.dealloc(v);
				v = &EMPTYC;
			}
		}
	};
	static U64LRUmap<ChunkDataWrapped,67> LRUM_CHUNK(256);
	static inline void purge_cache() {
		//LandCacheManager::reset();
		//CLMan.purge();
		LRUM_CHUNK.clear();
	}
	static inline ChunkLandManager* getChunkMan(lpos_t x, lpos_t z, int dim) {
		u64 CID = ChunkID(x, z, dim);
		auto res = LRUM_CHUNK.find(CID);
		if (res) {
			return res->v;
		}
		else {
			return LRUM_CHUNK.insert(CID, x, z, dim)->v;
		}
	}
	static inline ChunkLandManager* getChunkMan_TEST(lpos_t x, lpos_t z, int dim) {
		string LList;
		u64 CID = ChunkID(x, z, dim);
		db->get(to_view(CID), LList);
		auto C = new ChunkLandManager((u32*)LList.data(), (u32)LList.size() / sizeof(u32), x, z);
		return C;
	}
	static inline FastLand* getFastLand(int x, int z, int dim) {
		auto cm = getChunkMan(to_lpos(x) >> 4, to_lpos(z) >> 4, dim);
		return cm->lands[x & 15][z & 15];
	}

	static u32 getLandUniqid() {
		string val;
		std::random_device rd;
		while (1) {
			u32 lid = rd();
			if (!lid) continue;
			if (!db->get(to_view(lid), val)) return lid;
		}
	}
	static void proc_chunk_add(lpos_t x, lpos_t dx, lpos_t z, lpos_t dz, int dim, u32 lid) {
		for (auto i = x; i <= dx; ++i) {
			for (auto j = z; j <= dz; ++j) {
				u64 key = ChunkID(i, j, dim);
				string val;
				db->get(to_view(key), val);
				val.append(to_view(lid));
				db->put(to_view(key), val);
			}
		}
	}
	static void proc_chunk_del(lpos_t x, lpos_t dx, lpos_t z, lpos_t dz, int dim, u32 lid) {
		for (auto i = x; i <= dx; ++i) {
			for (auto j = z; j <= dz; ++j) {
				u64 key = ChunkID(i, j, dim);
				string val;
				db->get(to_view(key), val);
				for (size_t i = 0; i < val.size(); i += sizeof(u32)) {
					if (*(u32*)(val.data() + i) == lid) {
						val.erase(i, 4);
						break;
					}
				}
				if (val.size())
					db->put(to_view(key), val);
				else
					db->del(to_view(key));
			}
		}
	}
	static void addLand(int _x, int _dx, int _z, int _dz, int dim, xuid_t owner, LandPerm perm = NOTHING) {
		lpos_t x(to_lpos(_x)), z(to_lpos(_z)), dx(to_lpos(_dx)), dz(to_lpos(_dz));
		DataLand ld;
		ld.x = x, ld.z = z, ld.dx = dx, ld.dz = dz, ld.dim = dim;
		ld.owner = to_view(owner);
		ld.perm = perm;
		auto lid = getLandUniqid();
		ld.lid = lid;
		db->put(to_view(lid), ld.serialize());
		proc_chunk_add(x >> 4, dx >> 4, z >> 4, dz >> 4, dim, lid);
		purge_cache();
	}
	static void updLand(DataLand const& ld) {
		db->put(to_view(ld.lid), ld.serialize());
		purge_cache();
	}
	static void removeLand(FastLand const* land) {
		db->del(to_view(land->lid));
		proc_chunk_del(land->x >> 4, land->dx >> 4, land->z >> 4, land->dz >> 4, land->dim, land->lid);
		purge_cache();
	}
	static inline void INITDB() {
		db = MakeKVDB(GetDataPath("land_v2"), true, 4, 10);
	}
};


struct PointSelector {
	int mode;
	int dim;
	bool selectedA;
	bool selectedB;
	BlockPos A;
	BlockPos B;
	std::tuple<int,int,int,int> pos() {
		return { std::min(A.x,B.x),std::max(A.x,B.x),std::min(A.z,B.z),std::max(A.z,B.z) };
	}
	bool click(BlockPos pos,int _dim) {
		if (mode == 1) { //A
			selectedA = true;
			A = pos;
			return true;
		}
		else {
			if (dim != _dim || !selectedA) return false;
			B = pos;
			selectedB = true;
			return true;
		}
	}
	size_t size() {
		auto [x, dx, z, dz] = pos();
		return ((size_t)dx - x + 1) * ((size_t)dz - z + 1);
	}
	bool selected() {
		return selectedA && selectedB;
	}
};
static playerMap<PointSelector> SELECT_POINT;
static int BUY_PRICE, SELL_PRICE;
static bool PROTECT_FARM,PROTECT_IFRAME;

#pragma region CMDENUM
enum class LANDOP :int {
	buy = 1,
	sell = 2,
	info = 3,
	gui=4
};
enum class LANDOP2 :int {
	trust = 1,
	untrust = 2,
	give = 3
};
enum class LANDOP_PERM :int {
	perm = 1
};
enum class LANDPOP :int {
	A = 1,
	B = 2,
	exit = 3
};
#pragma endregion 
static bool oncmd(CommandOrigin const& ori, CommandOutput& outp,MyEnum<LANDPOP> op) {
	auto wp = MakeSP(ori);
	if (!wp) return false;
	switch (op.val)
	{
	case LANDPOP::A: {
		SELECT_POINT[wp].mode=1;
	}
		break;
	case LANDPOP::B: {
		SELECT_POINT[wp].mode=2;
	}
		break;
	case LANDPOP::exit: {
		SELECT_POINT._map.erase(wp);
	}
		break;
	default:
		break;
	}
	return true;
}
#define OERR(x) outp.error(_TRS(x))
enum class LandFetchRes {
	success=1,
	noland=2,
	noperm=3
};

using LandImpl::FastLand,LandImpl::getFastLand,LandImpl::LandPerm,LandImpl::DataLand;
static std::pair<LandFetchRes, FastLand const*> FetchLandForOwner(WPlayer wp) {
	std::pair<int,int> pos = {int(round(wp->getPos().x)), int(round(wp->getPos().z))};
	auto fl = getFastLand(pos.first, pos.second, wp.getDimID());
	if (!fl) {
		return { LandFetchRes::noland,nullptr };
	}
	return { (fl->getOPerm(wp.getXuid()) == 2 || wp.getPermLvl()>0) ? LandFetchRes::success : LandFetchRes::noperm,fl };
}
static FastLand const* genericPerm(int x,int z,WPlayer wp,LandPerm pm=LandPerm::NOTHING) {
	//WHY return FastLand*,for further land perm tips
	if (wp.getPermLvl() > 0) return nullptr;
	auto fl = getFastLand(x, z, wp.getDimID());
	if (!fl) return nullptr;
	if (fl->getOPerm(wp.getXuid()) == 0 && (fl->perm & (unsigned short)pm) == 0) return fl;
	return nullptr;
}

BDXLAND_API u32 getLandIDAt(int x, int z, int dim) {
	auto fl = getFastLand(x, z, dim);
	if (fl) return fl->lid; else return 0;
}
BDXLAND_API u32 checkLandRange(int x, int z, int dx, int dz, int dim) {
	for(int i=x;i<=dx;++i)
		for (int j = z; j <= dz; ++j)
		{
			auto fl = getFastLand(i, j,dim);
			if (fl) return fl->lid;
		}
	return 0;
}
static inline FastLand const* genericPerm(BlockPos const& pos, WPlayer wp, LandPerm pm=LandPerm::NOTHING) {
	return genericPerm(pos.x, pos.z, wp, pm);
}
static void LandGUIFor(WPlayer wp) {
	using namespace GUI;
	shared_ptr<FullForm> sf = make_shared<FullForm>();
	sf->title = "LAND GUI";
	sf->addWidget(GUIDropdown("operation", { "give","trust" }));
	sf->addWidget(GUIDropdown("target", getPlayerList()));
	sendForm(wp, FullFormBinder(sf, [](WPlayer w, FullFormBinder::DType d) {
		if (d.set) {
			auto [val, data] = d.val();
			w.runcmd("land " + data[0] + " \"" + data[1] + '"');
		}
	}));
}
static bool oncmd_2(CommandOrigin const& ori, CommandOutput& outp, MyEnum<LANDOP> op) {
	auto sp = MakeSP(ori);
	if (!sp) return false;
	WPlayer wp{ *sp };
	switch (op.val)
	{
	case LANDOP::buy: {
		auto it = SELECT_POINT._map.find(sp);
		if (it != SELECT_POINT._map.end()) {
			auto& point = it->second;
			if (!point.selected()) {
				OERR("land.need.select");
				return false;
			}
			auto [x, dx, z, dz] = point.pos();
			auto dim = point.dim;
			if (wp.getPermLvl() < 1) {
				if (point.size() > 100000) {
					OERR("land.too.big");
					return false;
				}
				size_t lsz = point.size();
				if (lsz * BUY_PRICE > 2100000000) {
					OERR("land.too.big");
					return false;
				}
				auto mone = Money::getMoney(wp.getXuid());
				if ((size_t)mone < (lsz * BUY_PRICE)) {
					OERR("land.no.money");
					return false;
				}
			}
			for (int i = x; i <= dx; ++i) {
				for (int j = z; j <= dz; ++j) {
					if (getFastLand(i, j, dim)) {
						OERR("land.overlap");
						outp.addMessage("Hint: "s + std::to_string(i) + " " + std::to_string(j));
						return false;
					}
				}
			}
			if (wp.getPermLvl() < 1) {
				if (!Money::createTrans(wp.getXuid(), 0, BUY_PRICE * point.size(), "buy land")) {
					OERR("land.no.money");
					return false;
				}
			}
			LandImpl::addLand(x, dx, z, dz, dim, wp.getXuid());
			LOG("buy land", x, z, dx, dz, dim);
			SELECT_POINT._map.erase(wp);
			return true;
		}
		else {
			return false;
		}
	}
		break;
	default: {
		auto [res, fl] = FetchLandForOwner(wp);
		if (res == LandFetchRes::noland) {
			OERR("land.no.land.here");
			return false;
		}
		if (op == LANDOP::info) {
			outp.addMessage("owners "+fl->ownStr());
			outp.addMessage("perm " + S((u16)fl->perm));
			return true;
		}
		else {
			if (res == LandFetchRes::noperm) {
				OERR("land.no.perm");
				return false;
			}
			//sell or gui
			if (op == LANDOP::sell) {
				size_t siz = fl->dz - fl->z + 1;
				siz = siz * (fl->dx - fl->x + 1);
				siz *= SELL_PRICE;
				Money::createTrans(0, wp.getXuid(), money_t(siz), "sell land " + S(fl->lid));
				LandImpl::removeLand(fl);
				outp.addMessage("you get $" + S(siz) + " by selling this land");
				return true;
			}
			else {
				//GUI
				LandGUIFor(wp);
				return true;
			}
		}
	}
		break;
	}
	return true;
}
static bool oncmd_3(CommandOrigin const& ori, CommandOutput& outp, MyEnum<LANDOP2> op,string& target) {
	auto sp = MakeSP(ori);
	if (!sp) return false;
	WPlayer wp{ *sp };
	auto [res, fl] = FetchLandForOwner(wp);
	if (res == LandFetchRes::noland) {
		OERR("land.no.land.here");
		return false;
	}
	if (res == LandFetchRes::noperm) {
		OERR("land.no.perm");
		return false;
	}
	DataLand dl(*fl);
		auto tar = XIDREG::str2id(target);
		if (!tar.set) {
			OERR("land.no.target");
			return false;
		}
		switch (op.val)
		{
		case LANDOP2::trust: {
			dl.addOwner(tar.val());
		}
		break;
		case LANDOP2::untrust: {
			dl.delOwner(tar.val());
		}
		break;
		case LANDOP2::give: {
			memcpy(dl.owner.data(), &tar.val(), sizeof(xuid_t));
		}
		break;
		};
		updLand(dl);
		return true;
}
static bool oncmd_perm(CommandOrigin const& ori, CommandOutput& outp, MyEnum<LANDOP_PERM> op, int perm) {
	auto wp = MakeWP(ori);
	auto [res, fl] = FetchLandForOwner(wp.val());
	if (res == LandFetchRes::noland) {
		OERR("land.no.land.here");
		return false;
	}
	if (res == LandFetchRes::noperm) {
		OERR("land.no.perm");
		return false;
	}
	DataLand dl(*fl);
	dl.perm = (decltype(dl.perm))perm;
	updLand(dl);
	return true;
}
static inline void NoticePerm(WPlayer wp, FastLand const*) {
	wp.sendText(_TRS("land.no.perm"));
}
static void loadConfig() {
	try {
		ConfigJReader jr("config/land.json");
		jr.bind("BUY_PRICE", BUY_PRICE);
		jr.bind("SELL_PRICE", SELL_PRICE);
		jr.bind("PROTECT_FARM", PROTECT_FARM);
		jr.bind("PROTECT_IFRAME", PROTECT_IFRAME);
	}
	catch (string e) {
		LOG.p<LOGLVL::Fatal>("json error", e);
		exit(1);
	}
}

static void TEST() {
	for (int i = -100; i <= 100; ++i)
		for (int j = -100; j <= 100; ++j) {
			LandImpl::addLand(i, i, j, j, 0, i * 1000 + j);
		}
	printf("1\n");
	for (int i = -1000; i <= 1000; ++i)
		for (int j = -1000; j <= 1000; ++j) {
			auto fl = LandImpl::getFastLand(i, j, 0);
			if ((!fl && i >= -100 && i <= 100 && j >= -100 && j <= 100) || (fl && fl->owner[0] != i * 1000 + j)) {
				LandImpl::__fail("aaa");
			}
			//LandImpl::removeLand(fl);
		}
	printf("2\n");
}
void entry() {
	LandImpl::INITDB();
	//TEST();
	addListener([](RegisterCommandEvent&) {
		CEnum<LANDPOP> _1("landpoint", {"a","b","exit"});
		CEnum<LANDOP> _2("landop", {"buy","sell","info","gui"});
		CEnum<LANDOP2> _3("landop2", { "trust","untrust","give"});
		CEnum<LANDOP_PERM> _4("landperm", { "perm" });
		MakeCommand("land", "land command", 0);
		CmdOverload(land, oncmd,"point-op");
		CmdOverload(land, oncmd_2, "land-op");
		CmdOverload(land, oncmd_3, "land-op2","target");
		CmdOverload(land, oncmd_perm, "perm","land_perm");
		});
	addListener([](PlayerDestroyEvent& ev) {
		auto fl = genericPerm(ev.getPos(), ev.getPlayer(),LandPerm::PERM_DESTROY);
		if (fl) {
			ev.setCancelled();
			ev.setAborted();
			NoticePerm(ev.getPlayer(), fl);
		}
	},EvPrio::HIGH);
	addListener([](PlayerUseItemOnEvent& ev) {
		auto wp = ev.getPlayer();
		LandPerm pm=wp->getCarriedItem().isNull()?LandPerm::PERM_USE:LandPerm::PERM_BUILD;
		auto fl = genericPerm(ev.getPos(), wp, pm);
		if (fl) {
			ev.setCancelled();
			ev.setAborted();
			NoticePerm(ev.getPlayer(), fl);
			return;
		}
		fl = genericPerm(ev.getPlacePos(), wp, pm);
		if (fl) {
			ev.setCancelled();
			ev.setAborted();
			NoticePerm(ev.getPlayer(), fl);
			return;
		}
		//check select
		auto it = SELECT_POINT._map.find(wp.v);
		if (it == SELECT_POINT._map.end()) return;
		if (it->second.click(ev.getPos(), wp.getDimID())) {
			if (it->second.mode == 1) {
				//A
				wp.sendText(_TRS("land.select.A"));
			}
			else {
				wp.sendText(_TRS("land.select.B"));
				wp.sendText("size " + S(it->second.size()) + " price " + S(it->second.size() * BUY_PRICE));
			}
		}
		else {
			wp.sendText(_TRS("land.select.err"));
		}
		ev.setAborted();
		ev.setCancelled();
	}, EvPrio::HIGH);
	addListener([](PlayerUseItemOnEntityEvent& ev) {
		if (!ev.victim) return; //maybe player hit a npc?
		auto& pos = ev.victim->getPos();
		LandPerm pm = ev.type == PlayerUseItemOnEntityEvent::TransType::ATTACK ? LandPerm::PERM_ATK : LandPerm::PERM_INTERWITHACTOR;
		auto fl = genericPerm(int(pos.x), int(pos.z), ev.getPlayer(), pm);
		if (fl) {
			ev.setCancelled();
			ev.setAborted();
			NoticePerm(ev.getPlayer(), fl);
		}
	},EvPrio::HIGH);
	loadConfig();
}
THook(void*, "?attack@ItemFrameBlock@@UEBA_NPEAVPlayer@@AEBVBlockPos@@@Z", void* a, void* b, void* c) {
	return nullptr;
}
THook(void*, "?transformOnFall@FarmBlock@@UEBAXAEAVBlockSource@@AEBVBlockPos@@PEAVActor@@M@Z", void* t, class BlockSource& x, class BlockPos const& y, class Actor* z, float p) {
	return nullptr;
}
THook(void, "?handle@ServerNetworkHandler@@UEAAXAEBVNetworkIdentifier@@AEBVItemFrameDropItemPacket@@@Z", ServerNetworkHandler* thi, NetworkIdentifier& a2, unsigned char* pk) {
	auto sp=thi->_getServerPlayer(a2, pk[16]);
	if (sp) {
		int x = dAccess<int, 40>(pk);
		int z = dAccess<int, 48>(pk);
		WPlayer wp{ *sp };
		auto fl = genericPerm(x, z, wp);
		if (fl) {
			NoticePerm(wp, fl);
			return;
		}
	}
	return original(thi, a2, pk);
}