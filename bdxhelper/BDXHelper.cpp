// BDXHelper.cpp : 定义 DLL 的导出函数。
//

#include "pch.h"
static Logger LOG(stacked{ stdio_commit{"[BH] "},file_commit{"bdx.log"} });
bool EXP_PLAY;
/*from codehz/element-0*/

THook(
	void,
	"?registerCommand@CommandRegistry@@QEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@"
	"PEBDW4CommandPermissionLevel@@UCommandFlag@@3@Z",
	uintptr_t a0, uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5) {
	original(a0, a1, a2, a3, a4, 0x40);
}
THook(
	void**,
	"?getEncryptedPeerForUser@NetworkHandler@@QEAA?AV?$weak_ptr@VEncryptedNetworkPeer@@@std@@AEBVNetworkIdentifier@@@Z",
	void* a, void** b, void* c) {
	b[0] = b[1] = NULL;
	return b;
}

#pragma region expplay
THook(
	void,
	"?setStack@ResourcePackManager@@QEAAXV?$unique_ptr@VResourcePackStack@@U?$default_delete@VResourcePackStack@@@std@@"
	"@std@@W4ResourcePackStackType@@_N@Z",void* thi,
	void* ptr, int type, bool flag) {
	if(EXP_PLAY)
		((char*)thi)[227] = EXP_PLAY;
	original(thi, ptr, type, flag);
}

THook(void*, "??0MinecraftEventing@@QEAA@AEBVPath@Core@@@Z", void* a, void* b) {
	// HACK FOR LevelSettings
	char* access = (char*)b + 2800;
	access[76] = EXP_PLAY;
	//access[36] = settings.education_feature;
	return original(a, b);
}

THook(bool, "?isEnabled@FeatureToggles@@QEBA_NW4FeatureOptionID@@@Z",void* thi, int id) {
	if (EXP_PLAY) return true;
	return original(thi, id);
}
THook(void, "??0LevelSettings@@QEAA@AEBV0@@Z", char* lhs, char* rhs) {
	rhs[76] = EXP_PLAY;
	//rhs[36] = settings.education_feature;
	original(lhs, rhs);
}
#pragma endregion
unordered_map<int, string> CMDMAP;
bool regABILITY;
bool fix_crash_bed_explode, FIX_PUSH_CHEST;
int FAKE_SEED;
vector<taskid_t> TASKS;
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
		jr.bind("enable_ability", regABILITY);
		jr.bind("fix_crash_bed_explode", fix_crash_bed_explode, false);
		jr.bind("FIX_PUSH_CHEST", FIX_PUSH_CHEST, false);
		jr.bind("FAKE_SEED", FAKE_SEED, 114514);
		vector<string> Timers;
		jr.bind("Timers", Timers, {});
		if (EXP_PLAY) {
			LOG("EXP Play mode is BUGGY!!!");
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
void entry() {
	loadCfg();
	loadCNAME();
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
		if (!ev.exp.cause && EXP_PLAY) {
			ev.setCancelled();
		}
		if (fix_crash_bed_explode) {
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
		if (ev.maySpam) return;
		auto id = ev.getItemInHand()->getId();
		auto it = CMDMAP.find(id);
		if (it != CMDMAP.end()) ev.getPlayer().runcmd(it->second),ev.setCancelled();
		});
	LOG("server started");
	/*Handler::schedule(RepeatingTask([]() {
		for (auto i : LocateS<WLevel>()->getUsers()) {
			Vec3 pos = i->getPos();
			i.sendText(S(pos.x) + " " + S(pos.z) + " " + S(int(pos.x)) + " " + S(int(pos.z)) + " " + S(int(round(pos.x))) +" " + S(int(round(pos.z))));
		}
	}, 3));*/
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
/*
THook(void*, "?_sortAndPacketizeEvents@NetworkHandler@@AEAA_NAEAVConnection@1@V?$time_point@Usteady_clock@chrono@std@@V?$duration@_JU?$ratio@$00$0DLJKMKAA@@std@@@23@@chrono@std@@@Z", void* a, void* b, void* c) {
	WatchDog dog("_sortAndPacketizeEvents");
	return original(a, b, c);
}
*/
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
#ifdef TRACING_ENABLED
THook(void*, "?tickWorld@ServerPlayer@@UEAAHAEBUTick@@@Z", void* a, void* b) {
	WatchDog dog("tickWorld@ServerPlayer");
	return original(a, b);
}
THook(void*, "?CompactRange@DBImpl@leveldb@@UEAAXPEBVSlice@2@0@Z", void* a, void* b, void* c) {
	WatchDog dog("DB_COMPACT");
	return original(a, b, c);
}
#endif
THook(void*, "?onFertilized@GrassBlock@@UEBA_NAEAVBlockSource@@AEBVBlockPos@@PEAVActor@@W4FertilizerType@@@Z", void* thi, void* a1, void* a2, void* a3_bds_bugs_here, void* a4) {
	if (a3_bds_bugs_here) return original(thi, a1, a2, a3_bds_bugs_here, a4);
	return nullptr;
}
THook(void*, "?handle@ServerNetworkHandler@@UEAAXAEBVNetworkIdentifier@@AEBVCommandBlockUpdatePacket@@@Z", ServerNetworkHandler* thi, NetworkIdentifier& a2, unsigned char* pk) {
	auto sp = thi->_getServerPlayer(a2, pk[16]);
	if (sp) {
		WPlayer wp{ *sp };
		if (wp.getPermLvl() == 0) {
			LOG("CMDBLOCK CHEAT!!", wp.getName());
			return nullptr;
		}
		return original(thi, a2, pk);
	}
	return nullptr;
}