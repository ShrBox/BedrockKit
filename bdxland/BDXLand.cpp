// BDXLand.cpp : 定义 DLL 的导出函数。
//

#include "pch.h"
#include "BDXLand.h"
#include "shared.h"
static Logger LOG(stdio_commit{ "[LAND] " });
LangPack LP("langpack/land.json");
#define to_lpos(x) ((x) ^ 0x80000000)
namespace LandImpl {
	using std::list;
	using std::swap;
	using std::unordered_map;
	static std::unique_ptr<KVDBImpl> db;
	typedef u64 xuid_t;

	//FastLand in shared.h
	static_assert(sizeof(FastLand) == sizeof(u32) * 8);
	static_assert(offsetof(FastLand, owner) == sizeof(u32) * 8);
	static_assert(offsetof(FastLand, perm_others) == sizeof(u32) * 6 + sizeof(u16));
	struct DataLand {
		lpos_t x, z, dx, dz; //4*int
		u32 lid; //5*int
		u16 spec;  //refcount
		LandPerm perm_group;//6*int
		u16 dim;
		LandPerm perm_others; //7*int
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
			perm_group = fastland.perm_group;
			perm_others = fastland.perm_others;
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
			ws.apply(x, z, dx, dz, lid, spec, perm_group, dim, perm_others, owner);
			return ws.data;
		}
		void deserialize(RBStream rs) {
			rs.apply(x, z, dx, dz, lid, spec, perm_group, dim, perm_others, owner);
		}
	};
	static inline int __fail(const string& c,int line) {
		printf("[LAND/FATAL :: %d] %s\n",line,c.c_str());
		exit(1);
		return 1;
	}
#define LAssert(x,y) (x)&&__fail(y,__LINE__)
	namespace LandCacheManager {
		static unordered_map<u32, FastLand*> cache;
		static void noticeFree(u32 id) {
			auto it = cache.find(id);
			LAssert(it == cache.end(), "double free land "+S(id));
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
				LAssert(!db->get(to_view(id), landstr),"bad land "+S(id));
				FastLand* res = (FastLand*)malloc(landstr.size());
				if (!res) {
					LOG.l<LOGLVL::Fatal>("bad_alloc");
					exit(1);
				}
				memcpy(res, landstr.data(), landstr.size());
				LAssert(landstr.size() < sizeof(FastLand),"bad land "+S(id)+" "+S(landstr.size()));
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
			for (u32 i = 0; i < mLandsCnt; ++i) {
				LandCacheManager::noticeFree(managed_lands[i]);
			}
		}
		ChunkLandManager(u32* landlist, u32 siz, lpos_t xx, lpos_t zz) {
			memset(lands, 0, sizeof(lands));
			mLandsCnt = siz;
			memcpy(managed_lands, landlist, siz * sizeof(u32));
			LAssert(mLandsCnt >= 256, "mlandscnt " + S(mLandsCnt));
			for (u32 I = 0; I < siz; ++I) {
				auto fl = LandCacheManager::requestLand(landlist[I]);
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

	static inline u64 ChunkID(lpos_t x, lpos_t z, u32 dim) {
		u32 ids[2];
		ids[0] = x;
		ids[1] = z;
		ids[1] |=dim<<29;
		return (*(u64*)(&ids));
		/*u64 rv = x;
		rv *= 0x100000000ull;
		rv += ((u64)z) * 16;
		rv += dim&3;
		return rv;*/
	}
	template<typename TP,size_t CAP=24>
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
#if 1
	struct ChunkDataWrapped
	{
		ChunkLandManager* v;
		ChunkDataWrapped(ChunkDataWrapped const&) = delete;
		ChunkDataWrapped& operator=(ChunkDataWrapped const&) = delete;
		ChunkDataWrapped() {
			v = nullptr;
		}
		ChunkDataWrapped(lpos_t x, lpos_t z, int dim) {
			string LList;
			u64 CID = ChunkID(x, z, dim);
			if (db->get(to_view(CID), LList)) {
				v = CLDP.alloc((u32*)LList.data(), (u32)(LList.size() / sizeof(u32)), x, z);
				//v=new ChunkLandManager((u32*)LList.data(), (u32)(LList.size() / sizeof(u32)), x, z);
			}
			else {
				v = nullptr;
			}
		}
		~ChunkDataWrapped() {
			if (v) {
				//delete v;
				CLDP.dealloc(v);
				v = nullptr;
			}
		}
		FastLand const* get(int x, int z) {
			if (!v) return nullptr;
			return v->lands[x][z];
		}
	};
#else
	struct ChunkDataWrapped
	{
		ChunkLandManager v;
		ChunkDataWrapped():v(EMPTYC) {
		}
		ChunkDataWrapped(lpos_t x, lpos_t z, int dim) :v(EMPTYC) {
			string LList;
			u64 CID = ChunkID(x, z, dim);
			if (db->get(to_view(CID), LList)) {
				//v = CLDP.alloc((u32*)LList.data(), (u32)(LList.size() / sizeof(u32)), x, z);
				//LOG(LList.size());
				v = ChunkLandManager((u32*)LList.data(), (u32)(LList.size() / sizeof(u32)), x, z);
			}
			else {
				v = EMPTYC;
			}
		}
	};
#endif
	static U64LRUmap<ChunkDataWrapped,73> LRUM_CHUNK(256);
	static inline void purge_cache() {
		LRUM_CHUNK.clear();
		LAssert(LandCacheManager::cache.size(), "LandCacheManager::cache.size()");
	}
#if 1
	static inline ChunkDataWrapped& getChunkMan(lpos_t x, lpos_t z, int dim) {
		u64 CID = ChunkID(x, z, dim);
		auto res = LRUM_CHUNK.find(CID);
		if (res) {
			return *res;
		}
		else {
			return *LRUM_CHUNK.insert(CID, x, z, dim);
		}
	}
#endif
#if 0
	static inline ChunkLandManager* getChunkMan(lpos_t x, lpos_t z, int dim) {
		glb = ChunkDataWrapped(x, z, dim);
		return &glb.v;
	}
#endif
	static inline FastLand const* getFastLand(int x, int z, int dim) {
		ChunkDataWrapped& cm = getChunkMan(to_lpos(x) >> 4, to_lpos(z) >> 4, dim);
		return cm.get(x & 15,z & 15);
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
		ld.perm_others = perm;
		ld.perm_group = PERM_FULL;
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

static playerMap<PointSelector> SELECT_POINT;
int BUY_PRICE, SELL_PRICE;
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
	perm_o = 1, //others
	perm_g=2 //group
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
		outp.addMessage("click ground to select point A");
	}
		break;
	case LANDPOP::B: {
		SELECT_POINT[wp].mode=2;
		outp.addMessage("click ground to select point B");
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

enum class LandFetchRes {
	success=1,
	noland=2,
	noperm=3
};

using LandImpl::getFastLand,LandImpl::DataLand;
static std::pair<LandFetchRes, FastLand const*> FetchLandForOwner(WPlayer wp) {
	IVec2 pos(wp->getPos());
	auto fl = getFastLand(pos.x, pos.z, wp.getDimID());
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
	if (fl->hasPerm(wp.getXuid(),pm)) return nullptr;
	return fl;
}

BDXLAND_API u32 getLandIDAt(IVec2 vc, int dim) {
	auto fl = getFastLand(vc.x, vc.z, dim);
	if (fl) return fl->lid; else return 0;
}
BDXLAND_API u32 checkLandRange(IVec2 vc, IVec2 vc2, int dim) {
	for(int i=vc.x;i<=vc2.x;++i)
		for (int j = vc.z; j <= vc2.z; ++j)
		{
			auto fl = getFastLand(i, j,dim);
			if (fl) return fl->lid;
		}
	return 0;
}
BDXLAND_API bool checkLandOwnerRange(IVec2 vc, IVec2 vc2, int dim, unsigned long long xuid) {
	for (int i = vc.x; i <= vc2.x; ++i)
		for (int j = vc.z; j <= vc2.z; ++j)
		{
			auto fl = getFastLand(i, j, dim);
			if (fl && fl->getOPerm(xuid)==0) return false;
		}
	return true;
}

static inline FastLand const* genericPerm(BlockPos const& pos, WPlayer wp, LandPerm pm=LandPerm::NOTHING) {	
	return genericPerm(pos.x, pos.z, wp, pm);
}
static void LandGUIFor(WPlayer wp) {
	using namespace GUI;
	shared_ptr<FullForm> sf = make_shared<FullForm>();
	sf->title = "LAND GUI";
	sf->addWidget(GUIDropdown(string(I18N::S_OPERATION), { "give","trust" }));
	sf->addWidget(GUIDropdown(string(I18N::S_TARGET), getPlayerList()));
	sendForm(wp, FullFormBinder(sf, [](WPlayer w, FullFormBinder::DType d) {
		if (d.set) {
			auto [val, data] = d.val();
			w.runcmdA("land", data[0], QUOTE(data[1]));
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
			if (!land_buy_first_step(wp, outp, point)) return false;
			LOG("land buy step1", x, dx, z, dz);
			for (int i = x; i <= dx; ++i) {
				for (int j = z; j <= dz; ++j) {
					if (getFastLand(i, j, dim)) {
						OERR("land.overlap");
						outp.addMessage("Hint: "s + std::to_string(i) + " " + std::to_string(j));
						return false;
					}
				}
			}
			if (!land_buy_next_step(wp, outp, point)) return false;
			LOG("land buy step2", x, dx, z, dz);
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
			outp.addMessage("landid " + S(fl->lid));
			outp.addMessage("owners "+fl->ownStr());
			outp.addMessage("perm_group " + S((u16)fl->perm_group)); //TODO:human readable perm
			outp.addMessage("perm_others " + S((u16)fl->perm_others));
			return true;
		}
		else {
			if (res == LandFetchRes::noperm) {
				OERR("land.no.perm");
				return false;
			}
			//sell or gui
			if (op == LANDOP::sell) {
				land_sell_helper(wp, outp, fl);
				LandImpl::removeLand(fl);
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
	if(op.val==LANDOP_PERM::perm_o)
		dl.perm_others = (decltype(dl.perm_others))perm;
	else
		dl.perm_group = (decltype(dl.perm_others))perm;
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
		throw 0;
	}
}

static void TEST() {
	using namespace LandImpl;
	std::random_device rdv;
	int X = 10;
	while (X-- > 0)
		for (int i = 0; i <= 1000; ++i, i % 10 == 0 && printf("%d\n", i)) {
			//for (int j = -1000; j <= 1000; ++j) {
			//	LandImpl::getFastLand(i, j, 0);
			//	LandImpl::getFastLand(i, j, 1);
			//	LandImpl::getFastLand(i, j, 2);
			for (int Z = 0; Z < 100; ++Z)
				LandImpl::getFastLand((int)rdv(), (int)rdv(), rdv() & 3);
			//}
			if ((rdv() & 3) == 0) LandImpl::purge_cache();
			//DBG_FLAG = 1;
		}
	printf("enter test\n");
	for (int i = -1000; i <= 100; ++i)
		for (int j = -1000; j <= 100; ++j) {
			LandImpl::addLand(i, i, j, j, ((u32)i*233+(u32)j)&3, i * 1000 + j);
		}
	printf("->1\n");
	for (int i = -1000; i <= 1000; ++i)
		for (int j = -1000; j <= 1000; ++j) {
			auto fl = LandImpl::getFastLand(i, j, ((u32)i*233+(u32)j)&3);
			LAssert((!fl && i >= -1000 && i <= 100 && j >= -1000 && j <= 100) || (fl && fl->owner[0] != i * 1000 + j),"test failed");
			fl = LandImpl::getFastLand(i, j, ((u32)i * 233 + (u32)j+1)&3);
			LAssert(fl, "test2 failed");
		}
	printf("->2\n");
	X = 10;
	while (X-- > 0)
		for (int i = 0; i <= 1000; ++i, i % 10 == 0 && printf("%d\n", i)) {
			//for (int j = -1000; j <= 1000; ++j) {
			//	LandImpl::getFastLand(i, j, 0);
			//	LandImpl::getFastLand(i, j, 1);
			//	LandImpl::getFastLand(i, j, 2);
			for (int Z = 0; Z < 1000; ++Z)
				LandImpl::getFastLand((int)rdv(), (int)rdv(), rdv() & 3);
			//}
			if ((rdv() & 3) == 0) LandImpl::purge_cache();
			//DBG_FLAG = 1;
		}

	printf("TEST DONE\n");
}
static bool oncmd_dumpall(CommandOrigin const& ori, CommandOutput& outp) {
	LandImpl::db->iter([&](string_view k, string_view v) {
		if (k.size() == 4) {
			FastLand* fl = (FastLand*)v.data();
			outp.addMessage("land id " + S(fl->lid)+" perm "+S(fl->perm_group)+" : "+S(fl->perm_others));
			outp.addMessage("land pos (" + S(int(to_lpos(fl->x))) + " " + S(int(to_lpos(fl->z))) + ") -> (" + S(int(to_lpos(fl->dx))) + " " + S(int(to_lpos(fl->dz))) + ") dim " + S(fl->dim));
			outp.addMessage("land owners " + fl->ownStr());
			outp.addMessage("----------------------");
		}
		return true;
	});
	return true;
}
static void FIX_BUG_0401() {
	using LandImpl::db;
	string val;
	if (!db->get("BUG_0402_FIXED", val)) {
		LOG("\n\nstart to fix BUG_0402\n\n");
		vector<std::tuple<u32,u32,u32,u32,int,u32>> lands;
		vector<string> to_delete;
		db->iter([&](string_view k, string_view v) {
			if (k.size() == 4) {
				FastLand* fl = (FastLand*)v.data();
				if (fl->dx >= fl->x && fl->dz >= fl->z && v.size()>=sizeof(FastLand)) {
					lands.emplace_back(fl->x, fl->dx, fl->z, fl->dz,fl->dim, fl->lid);
				}
				else {
					LOG.p<LOGLVL::Error>("found buggy land", fl->lid, fl->x, fl->dx, fl->z, fl->dz);
					to_delete.emplace_back(k);
				}
			}
			else {
				if(k!= "FIX_LAND_PERM_GROUP_0405")
					to_delete.emplace_back(k);
			}
			return true;
		});
		for (auto& i : to_delete) {
			db->del(i);
		}
		for (auto& [x, dx, z, dz, dim, lid] : lands)
			LandImpl::proc_chunk_add(x>>4, dx>>4, z>>4, dz>>4, dim, lid);
		db->put("BUG_0402_FIXED", "fixed");
	}
}
void CHECK_MEMORY_LAYOUT() {
	DataLand ld;
	xuid_t xid = 114514;
	ld.owner.append(to_view(xid));
	auto sz = ld.serialize().size();
	using LandImpl::__fail;
	LAssert(sz != sizeof(FastLand) + sizeof(xuid_t), "MEM_LAYOUT_ERR");
}
void FIX_LAND_PERM_GROUP_0405() {
	using LandImpl::db;
	string val;
	if (!db->get("FIX_LAND_PERM_GROUP_0405", val)) {
		LOG("\n\nstart to fix FIX_LAND_PERM_GROUP_0405\n\n");
		std::vector<pair<u32, string>> lands;
		db->iter([&](string_view k, string_view v) {
			if (k.size() == 4) {
				string vv{ v };
				FastLand* fl = (FastLand*)vv.data();
				static_assert(offsetof(FastLand, perm_group) <= 36);
				fl->perm_group = PERM_FULL;
				lands.emplace_back(pair{ *(u32*)k.data(), std::move(vv) });
			}
			return true;
		});
		for (auto& [k, v] : lands) {
			db->put(to_view(k), v);
		}
		db->put("FIX_LAND_PERM_GROUP_0405", "fixed");
	}
}
void entry() {
	LandImpl::INITDB();
	CHECK_MEMORY_LAYOUT();
	FIX_BUG_0401();
	FIX_LAND_PERM_GROUP_0405();
	//WaitForDebugger();
	//TEST();
	addListener([](RegisterCommandEvent&) {
		CEnum<LANDPOP> _1("landpoint", {"a","b","exit"});
		CEnum<LANDOP> _2("landop", {"buy","sell","info","gui"});
		CEnum<LANDOP2> _3("landop2", { "trust","untrust","give"});
		CEnum<LANDOP_PERM> _4("landperm", { "permo","permg" });
		MakeCommand("land", "land command", 0);
		CmdOverload(land, oncmd,"point-op");
		CmdOverload(land, oncmd_2, "land-op");
		CmdOverload(land, oncmd_3, "land-op2","target");
		CmdOverload(land, oncmd_perm, "perm","land_perm");
		MakeCommand("landdump", "dump all lands", 1);
		CmdOverload(landdump, oncmd_dumpall);
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
		if (ev.maySpam) return;
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
		IVec2 vc(ev.victim->getPos());
		LandPerm pm = ev.type == PlayerUseItemOnEntityEvent::TransType::ATTACK ? LandPerm::PERM_ATK : LandPerm::PERM_INTERWITHACTOR;
		auto fl = genericPerm(vc.x, vc.z, ev.getPlayer(), pm);
		if (fl) {
			ev.setCancelled();
			ev.setAborted();
			NoticePerm(ev.getPlayer(), fl);
		}
	},EvPrio::HIGH);
	addListener([](MobHurtedEvent& ev) {
		IVec2 pos (ev.getMob()->getPos());
		FastLand const* fl = LandImpl::getFastLand(pos.x, pos.z, ev.getMob().actor()->getDimID());
		if (fl) {
			if (/*ev.getSource().isEntitySource() || We already filtered ent source in useitemOnEnt event*/ev.getSource().isChildEntitySource())
			{
				auto id = ev.getSource().getEntityUniqueID();
				Actor* ac = LocateS<ServerLevel>()->fetchEntity(id, false);
				ServerPlayer* sp = MakeSP(ac);
				if (!sp) return;
				WPlayer wp{ *sp };
				if (fl->getOPerm(wp.getXuid()) == 0 && (fl->dim & LandPerm::PERM_ATK) == 0) {
					ev.setAborted();
					ev.setCancelled();
					NoticePerm(wp, fl);
					return;
				}
			}
		}
		}, EvPrio::HIGH);
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
		auto fl = genericPerm(x, z, wp,LandPerm::PERM_ITEMFRAME);
		if (fl) {
			NoticePerm(wp, fl);
			return;
		}
	}
	return original(thi, a2, pk);
}
#if 0
addListener([](MobHurtedEvent& ev) {
	auto& pos = ev.getMob()->getPos();
	FastLand* fl = LandImpl::getFastLand(pos.x, pos.z, ev.getMob().actor()->getDimID());
	if (fl) {
		if (/*ev.getSource().isEntitySource() || We already filtered ent source in useitemOnEnt event*/ev.getSource().isChildEntitySource())
		{
			auto id = ev.getSource().getEntityUniqueID();
			Actor* ac = LocateS<ServerLevel>()->fetchEntity(id, false);
			ServerPlayer* sp = MakeSP(ac);
			if (!sp) return;
			WPlayer wp{ *sp };
			if (fl->getOPerm(wp.getXuid()) == 0 && (fl->dim & LandPerm::PERM_ATK) == 0) {
				ev.setAborted();
				ev.setCancelled();
				NoticePerm(wp, fl);
				return;
			}
		}
	}
	}, EvPrio::HIGH);
#endif
#if 0
THook(bool, "?hurt@Actor@@QEAA_NAEBVActorDamageSource@@H_N1@Z", Actor& act,ActorDamageSource const& src,int dam_val,bool unk2,bool unk3 ) {
	auto& pos = act.getPos();
	FastLand* fl = LandImpl::getFastLand(pos.x, pos.z, WActor{ act }.getDimID());
	if (fl) {
		if (/*ev.getSource().isEntitySource() || We already filtered ent source in useitemOnEnt event*/src.isChildEntitySource())
		{
			auto id = src.getEntityUniqueID();
			Actor* ac = LocateS<ServerLevel>()->fetchEntity(id, false);
			ServerPlayer* sp = MakeSP(ac);
			if (!sp) return original(act, src, dam_val, unk2, unk3);
			WPlayer wp{ *sp };
			if (fl->getOPerm(wp.getXuid()) == 0 && (fl->dim & LandPerm::PERM_ATK) == 0 && wp.getPermLvl()==0) {
				NoticePerm(wp, fl);
				return false;
			}
		}
	}
	return original(act, src, dam_val, unk2, unk3);
}
#endif