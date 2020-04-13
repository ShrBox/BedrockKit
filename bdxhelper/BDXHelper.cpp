// BDXHelper.cpp : 定义 DLL 的导出函数。
//

#include "pch.h"

void InitExplodeProtect();
bool NOFIXBDS_BONEMEAL_BUG;
Logger<stacked<stdio_commit, file_commit>> LOG(stacked{ stdio_commit{"[BH] "},file_commit("bdx.log",6,2*1024*1024) });
bool EXP_PLAY;
unordered_map<int, string> CMDMAP;
bool regABILITY;
bool fix_crash_bed_explode, FIX_PUSH_CHEST;
int FAKE_SEED;
vector<taskid_t> TASKS;
std::unordered_set<short> logItems,banItems;
int explosion_land_dist;
bool NO_EXPLOSION;
static bool StartGamePacketMod;
void loadCfg() {
	try {
		CMDMAP.clear();
		for (auto i : TASKS) {
			Handler::cancel(i);
		}
		TASKS.clear();
		ConfigJReader jr("config/helper.json");
		jr.bind("force_enable_expplay", EXP_PLAY,false);
		jr.bind("CMDMAP", CMDMAP);
		jr.bind("force_enable_ability", regABILITY,false);
		jr.bind("fix_crash_bed_explode", fix_crash_bed_explode, false);
		jr.bind("FIX_PUSH_CHEST", FIX_PUSH_CHEST, false);
		jr.bind("FAKE_SEED", FAKE_SEED, 114514);
		jr.bind("explosion_land_dist", explosion_land_dist, -1);
		jr.bind("NO_EXPLOSION", NO_EXPLOSION, false);
		//jr.bind("StartGamePacketMod", StartGamePacketMod, false);
		jr.bind("NOFIXBDS_BONEMEAL_BUG",NOFIXBDS_BONEMEAL_BUG,false);
		vector<string> Timers;
		jr.bind("Timers", Timers, {});
		vector<int> items;
		logItems.clear();
		banItems.clear();
		jr.bind("logItems", items, {});
		for (auto i : items) logItems.insert(i);
		items.clear();
		jr.bind("banItems", items, {});
		for (auto i : items) banItems.insert(i);
		if (EXP_PLAY) {
			LOG("\n\nEXP Play mode is BUGGY!!!\n\n");
		}
		for (auto& i : Timers) {
			char _luabuf[256];
			int interval=-1, delay=-1;
			sscanf_s(i.c_str(), "%d;%d;%s",&interval,&delay,_luabuf,256);
			if (delay == -1) continue;
			string lua = _luabuf;
			LOG("enabled timer", lua, interval, delay);
			TASKS.push_back(Handler::schedule(DelayedRepeatingTask([lua]() {
				call_lua(lua.c_str(), {});
			}, delay, interval)));
		}
	}
	catch (string e) {
		printf("[BDXHelper] json error %s\n", e.c_str());
		throw 0;
	}
}
bool onRunAS(CommandOrigin const& ori, CommandOutput& outp, CommandSelector<Player>& p, CommandMessage& cm) {
	auto res = p.results(ori);
	if (!Command::checkHasTargets(res, outp)) return false;
	string cmd = cm.get(ori);
	for (auto i : res) {
		WPlayer wp{ *i };
		wp.runcmd(cmd);
	}
	return true;
}
unordered_map<string, string> CNAME;
unique_ptr<KVDBImpl> db;
void loadCNAME() {
	db = MakeKVDB(GetDataPath("custname"),false);
	db->iter([](string_view k, string_view v) {
		if(!k._Starts_with("b_"))
			CNAME.emplace(k, v),LOG(k,v);
		return true;
	});
}
enum class CNAMEOP :int {
	set=1,
	remove=2
};
bool onCMD_CNAME(CommandOrigin const& ori, CommandOutput& p,MyEnum<CNAMEOP> op,string& src,optional<string>& name) {
	if (op == CNAMEOP::set) {
		CNAME[src] = name.val();
		db->put(src, name.val());
	}
	else {
		CNAME.erase(src);
		db->del(src);
	}
	return true;
}
bool onCMD_Trans(CommandOrigin const& ori, CommandOutput& outp, CommandSelector<Player>& p, string& host, optional<int> port) {
	int P = port.set ? port.val() : 19132;
	auto res = p.results(ori);
	if (!Command::checkHasTargets(res, outp)) return false;
	WBStream ws;
	ws.apply(MCString(host), (unsigned short)P);
	MyPkt<0x55, false> trpk(ws);
	for (auto i : res) {
		((ServerPlayer*)i)->sendNetworkPacket(trpk);
	}
	return true;
}
enum class BANOP :int {
	ban=1,
	unban=2,
	banip = 3
};
enum class BANOP_LIST :int {
	list=1
};
void LOWERSTRING(string& S) {
	for (auto& i : S) {
		i = tolower(i);
	}
}
void addBanEntry(string const& entry, time_t timediff) {
	time_t next = timediff == 0 ? 0 : (time(0) + timediff);
	db->put("b_" + entry, to_view(next));
}
optional<time_t> getBanEntry(string const& entry) {
	string val;
	if (db->get("b_" + entry, val)) return { *(time_t*)val.data() };
	return {};
}
void removeBanEntry(string const& entry) {
	db->del("b_" + entry);
}
bool onCMD_BanList(CommandOrigin const& ori, CommandOutput& outp, MyEnum<BANOP_LIST>) {
	db->iter([&](string_view key, string_view val)->bool {
		if (key._Starts_with("b_")) {
			string banned{ key.substr(2) };
			if (banned.find('.') != banned.npos) {
				outp.addMessage(banned + " " + std::to_string(*(time_t*)val.data()));
			}
			else {
				xuid_t xid;
				std::stringstream ss{ banned };
				ss >> xid;
				auto strname = XIDREG::id2str(xid);
				outp.addMessage(banned + " (" + (strname.set ? strname.val() : "") + ") " + std::to_string(*(time_t*)val.data()));
			}
		}
		return true;
		});
	return true;
}
bool onCMD_Ban(CommandOrigin const& ori, CommandOutput& outp, MyEnum<BANOP> op, string& entry, optional<int>& time) {
	LOWERSTRING(entry);
	switch (op.val)
	{
	case BANOP::banip: {
		addBanEntry(entry, time.set ? time.val() : 0);
		return true;
	}
	case BANOP::ban: {
		addBanEntry(S(XIDREG::str2id(entry).val()), time.set ? time.val() : 0);
		BDX::runcmdA("skick", QUOTE(entry));
		return true;
	}
		break;
	case BANOP::unban: {
		if (getBanEntry(entry).set) {
			removeBanEntry(entry);
			return true;
		}
		else {
			auto XID = XIDREG::str2id(entry);
			if (!XID.set) {
				outp.error("not banned");
				return false;
			}
			else {
				removeBanEntry(S(XID.val()));
				return true;
			}
		}
	}
		break;
	default:
		break;
	}
	return false;
}
bool onCMD_skick(CommandOrigin const& ori, CommandOutput& outp,string& target) {
	LOWERSTRING(target);
	vector<WPlayer> to_kick;
	for (auto i : LocateS<WLevel>()->getUsers()) {
		auto name = i.getName();
		LOWERSTRING(name);
		if (name._Starts_with(target)) {
			if (name == target) {
				i.forceKick();
				return true;
			}
			to_kick.push_back(i);
		}
	}
	for (auto i : to_kick) {
		i.forceKick();
	}
	return true;
}
static bool onReload(CommandOrigin const& ori, CommandOutput& outp) {
	loadCfg();
	return true;
}
static bool oncmd_toggle_debug(CommandOrigin const& ori, CommandOutput& outp) {
	static LInfo<PlayerUseItemOnEvent> id;
	static LInfo<MobDeathEvent> id2;
	if (id.id != -1) {
		removeListener(id);
		removeListener(id2);
		outp.success("disabled");
	}
	else {
		id = addListener([](PlayerUseItemOnEvent& ev) {
			ev.getPlayer().sendText(ev.getItemInHand()->toString());
			ev.getPlayer().sendText(S(ev.getItemInHand()->getId()) + ":" + S(ev.getItemInHand()->getAuxValue()));
		}, EvPrio::HIGH);
		id2 = addListener([](MobDeathEvent& ev) {
			LocateS<WLevel>()->broadcastText(S(ev.getMob()->getEntityTypeId())+" died");
		}, EvPrio::HIGH);
		outp.success("enabled");
	}
	return true;
}
enum GameType:int{};
static bool oncmd_gamemode(CommandOrigin const& ori, CommandOutput& outp,CommandSelector<Player>& s,int mode) {
	auto res=s.results(ori);
	if (!Command::checkHasTargets(res, outp)) return false;
	for (auto i : res) {
		((ServerPlayer*)i)->setPlayerGameType((GameType)mode);
	}
	return true;
}
void entry() {
	loadCfg();
	loadCNAME();
	InitExplodeProtect();
	addListener([](RegisterCommandEvent& ev) {
		CEnum<CNAMEOP> _1("cnameop", { "set","rm" });
		CEnum<BANOP> _2("banop", {"ban","unban","banip"});
		CEnum<BANOP_LIST> _3("banoplist", { "list" });
		MakeCommand("runas", "runas player", 1);
		CmdOverload(runas, onRunAS, "target", "command");
		MakeCommand("cname", "custom name", 1);
		CmdOverload(cname,onCMD_CNAME,"op","target","name");
		MakeCommand("transfer", "transfer player to another server", 1);
		CmdOverload(transfer, onCMD_Trans, "target", "host", "port");
		MakeCommand("ban", "blacklist", 1);
		CmdOverload(ban, onCMD_Ban, "op", "target","time");
		CmdOverload(ban, onCMD_BanList, "list");
		MakeCommand("skick", "force kick", 1);
		CmdOverload(skick, onCMD_skick, "target");
		MakeCommand("hreload", "reload cmdhelper", 1);
		CmdOverload(hreload, onReload);
		MakeCommand("idbg", "toggle debug mode", 1);
		CmdOverload(idbg, oncmd_toggle_debug);
		MakeCommand("gmode","set gamemode",1);
		CmdOverload(gmode, oncmd_gamemode, "target", "mode");
		if (regABILITY) {
			SymCall("?setup@AbilityCommand@@SAXAEAVCommandRegistry@@@Z", void, CommandRegistry&)(LocateS<CommandRegistry>());
		}
	});
	addListener([](PlayerJoinEvent& ev) {
		LOG(ev.getPlayer().getName(), "joined server,IP", ev.getPlayer().getIP());
		});
	addListener([](PlayerPreJoinEvent& ev) {
		auto xuid=ExtendedCertificate::getXuid(ev.cert);
		auto be1 = getBanEntry(xuid);
		auto IP = BDX::getIP(ev.neti);
		auto be2 = getBanEntry(IP);
		if (be1.set) {
			if (be1.val()!=0 && be1.val() < time(0)) {
				removeBanEntry(xuid);
			}
			else {
				//prevent
				LocateS<ServerNetworkHandler>()->onDisconnect(ev.neti);
			}
		}
		if (be2.set) {
			if (be2.val() != 0 && be2.val() < time(0)) {
				removeBanEntry(IP);
			}
			else {
				//prevent
				LocateS<ServerNetworkHandler>()->onDisconnect(ev.neti);
			}
		}
	});
	addListener([](LevelExplodeEvent& ev) {
		if ((fix_crash_bed_explode || EXP_PLAY) && ev.exp.bs.getDim().getID()==2 /*ender*/) {
			ev.setCancelled();
		}
	});
	addListener([](PlayerLeftEvent& ev) {
		LOG(ev.getPlayer().getName(), "left server");
		});
	addListener([](PlayerCMDEvent& ev) {
		LOG(ev.getPlayer().getName(), "CMD", ev.getCMD());
		});
	addListener([](PlayerChatEvent& ev) {
		LOG.l('<', ev.getPlayer().getName(), '>', ' ', ev.getChat());
		});
	addListener([](PlayerUseItemOnEvent& ev) {
		auto id = ev.getItemInHand()->getId();
		if (ev.getPlayer().getPermLvl() == 0) {
			if (logItems.count(id)) {
				LOG("player", ev.getPlayer().getName(), "used warning item", ev.getItemInHand()->toString());
				return;
			}
			else {
				if (banItems.count(id)) {
					LOG("player", ev.getPlayer().getName(), "used banned item", ev.getItemInHand()->toString());
					ev.getPlayer().sendText("banned item");
					ev.setAborted();
					ev.setCancelled();
					return;
				}
			}
		}
		if (ev.maySpam) return;
		auto it = CMDMAP.find(id);
		if (it != CMDMAP.end()) ev.getPlayer().runcmd(it->second),ev.setCancelled();
		},EvPrio::HIGH);
	LOG("server started");
}
THook(void, "?write@TextPacket@@UEBAXAEAVBinaryStream@@@Z", void* a, void* b) {
	//+40 +48
	if (dAccess<char, 40>(a) == 1) {
		auto& name = dAccess<string, 48>(a);
		auto it = CNAME.find(name);
		if (it != CNAME.end()) name = it->second;
	}
	original(a, b);
}
static inline bool __isContainer(void* blk) {
	void** vtbl = *(void***)blk;
	bool (*call)(void);
	call = (decltype(call))vtbl[0xc8 / 8];
	return call();
}
THook(
	void*,
	"?createWeakPtr@BlockLegacy@@QEAA?AV?$WeakPtr@VBlockLegacy@@@@XZ",
	void* thi, void* a1) {
	if(__isContainer(thi) && FIX_PUSH_CHEST)
		dAccess<unsigned long, 152>(thi) |= 0x1000000LL;
	auto ret = original(thi, a1);
	return ret;
}
THook(void*, "?write@StartGamePacket@@UEBAXAEAVBinaryStream@@@Z", void* a, void* b) {
	if (FAKE_SEED) {
		dAccess<int, 40>(a) = FAKE_SEED;
	}
	return original(a, b);
}
class InventorySource {
	char filler[12];
};
class InventoryAction {
	char filler[152 + 152 - 16];
};

bool CheckItemTrans(unordered_map<class InventorySource, vector<InventoryAction>>* mp,const string& plyname) {
	for (auto& [k, v] : *mp) {
		for (auto& action : v) {
			/*
			auto maysource = dAccess<u32, 0>(&action);
			auto maysource1 = dAccess<u32, 4>(&action); //???
			auto maypadding = dAccess<u32, 8>(&action); //may padding
			auto slot = dAccess<u32, 12>(&action);
			auto from = dAccess<ItemStackBase, 16>(&action).toString();
			auto to = dAccess<ItemStackBase, 152>(&action).toString();
			LOG(maysource, maysource1, maypadding, slot, from, to);
			*/
			short id1 = dAccess<ItemStackBase, 16>(&action).getId();
			short id2 = dAccess<ItemStackBase, 152>(&action).getId();
			auto& item1 = dAccess<ItemStackBase, 16>(&action);
			auto& item2 = dAccess<ItemStackBase, 152>(&action);
#define LOGITEM(set,id,type,item) if(set.count(id)) LOG("player",plyname," used " type,"item",item.toString())
#define LOGITEM2(set,id,type,item) if(set.count(id)) {LOG("player",plyname," used " type,"item",item.toString());return true;}
			LOGITEM(logItems, id1, "warning", item1);
			LOGITEM(logItems, id2, "warning", item2);
			LOGITEM2(banItems, id1, "banned", item1);
			LOGITEM2(banItems, id2, "banned", item2);
		}
	}
	return false;
}
THook(void*, "?executeFull@InventoryTransaction@@QEBA?AW4InventoryTransactionError@@AEAVPlayer@@_N@Z", unordered_map<class InventorySource, vector<class InventoryAction>>* thi, ServerPlayer& sp, bool unk) {
	if (sp.getCommandPermissionLevel() == 0) {
		if (CheckItemTrans(thi, WPlayer{ sp }.getName())) return nullptr;
	}
	return original(thi, sp, unk);
}
THook(bool,"?isSelectorExpansionAllowed@ActorCommandOrigin@@UEBA_NXZ",CommandOrigin& ori){
	auto p=MakeWP(ori);
	if(!p.set) return original(ori);
	return p.val().getPermLvl()>0;
}