﻿// BDXTPA.cpp : 定义 DLL 的导出函数。
//

#include "pch.h"
#include <bdxland.h>
#include "homeStorage.h"
std::unique_ptr<KVDBImpl> db;
static Logger LOG(stdio_commit{"[TPA] "});
bool checkLandOwnerRange_stub(IVec2 vc, IVec2 vc2, int dim, unsigned long long xuid);
#pragma region structs
enum direction :int {
	A_B = 1,
	B_A = 2
};
struct TPReq {
	direction dir;
	string A, B;
	clock_t time;
	TPReq(){}
	TPReq(direction a,string_view b,string_view c,clock_t d):dir(a),A(b),B(c),time(d){}
};
struct TPASet {
	clock_t lastReq=0;
	bool Accept=true;
};


#pragma endregion
#pragma region gvals
static LangPack LP("langpack/tpa.json");
#include<unordered_map>
using std::unordered_map;
static std::list<TPReq> reqs;
static unordered_map<CHash,TPASet> tpaSetting;
static unordered_map<string, Vec4> warps;

static clock_t TPexpire=CLOCKS_PER_SEC*10;
static clock_t TPratelimit=CLOCKS_PER_SEC*2;
static int MAX_HOMES=5;
static int HOME_DISTANCE_LAND = 0;
static bool BACK_ENABLED, SUICIDE_ENABLED;


playerMap<Vec4> deathPos;
#pragma endregion
#pragma region TPA
optional<decltype(reqs.begin())> DoFetchReq(string_view a) {
	for (auto it = reqs.begin(); it != reqs.end();++it) {
		if (it->A == a || it->B == a) return { it };
	}
	return {};
}
optional<decltype(reqs.begin())> DoFetchReq_sender(string_view a) {
	auto rv = DoFetchReq(a);
	if (rv.set && rv.value()->A == a) return rv;
	return {};
}
optional<decltype(reqs.begin())> DoFetchReq_receiver(string_view a) {
	auto rv = DoFetchReq(a);
	if (rv.set && rv.value()->B == a) return rv;
	return {};
}
enum class TPFailReason :int {
	success=0,
	ratelimit=1,
	inreq=2,
	blocked=3
};
TPFailReason CanMakeReq(string_view a, string_view b) {
	CHash A = do_hash(a),B=do_hash(b);
	if (tpaSetting[A].lastReq >= clock() - TPratelimit) {
		return TPFailReason::ratelimit;
	}
	if (DoFetchReq(a).set || DoFetchReq(b).set) return TPFailReason::inreq;
	if (!tpaSetting[B].Accept) return TPFailReason::blocked;
	return TPFailReason::success;
}
enum class TPCloseReason :int {
	timeout = 1,
	deny = 2,
	accept = 3,
	cancel=4
};
bool DoCloseReq(decltype(reqs.begin()) rq, TPCloseReason res) {
	auto A = LocateS<WLevel>()->getPlayer(rq->A);
	auto B = LocateS<WLevel>()->getPlayer(rq->B);
	if (res == TPCloseReason::cancel) {
		reqs.erase(rq);
		return true;
	}
	if (res == TPCloseReason::accept) {
		if (A.set && B.set) {
			Vec4 AP{ (rq->dir == A_B ? B : A).value()};
			AP.teleport((rq->dir == A_B ? A : B).value());
			reqs.erase(rq);
			A.value().sendText(_TRS("tpa.reason.accept"));
			return true;
		}
		reqs.erase(rq);
		return false;
	}
	//deny timeout
	if (A.set) {
		A.value().sendText(res == TPCloseReason::deny ? _TRS("tpa.reason.deny") : _TRS("tpa.reason.timeout"));
	}
	if (B.set) {
		B.value().sendText(res == TPCloseReason::deny ? _TRS("tpa.reason.deny") : _TRS("tpa.reason.timeout"));
	}
	reqs.erase(rq);
	return true;
}
void DoMakeReq(WPlayer _a, WPlayer _b, direction dir) {
	auto& a = _a.getName();
	auto& b = _b.getName();
	CHash A = do_hash(a), B = do_hash(b);
	tpaSetting[A].lastReq = clock();
	reqs.emplace_back(dir,a,b,clock());
	string prompt = a + (dir == A_B ? _TRS("tpa.req.A_B") : _TRS("tpa.req.B_A"));
	_b.sendText(prompt);
	using namespace GUI;
	shared_ptr<RawFormBinder> x;
	char buf[1024];
	string FM{ buf,(size_t)snprintf(buf,1024,_TR("tpa.form"), prompt.c_str()) };
	sendForm(_b, RawFormBinder{ FM,[](WPlayer wp,RawFormBinder::DType i) {
		auto [clicked,res,list] = i;
		if (clicked) {
			int idx = atoi(res);
			wp.runcmdA("tpa",(idx == 0 ? "ac" : "de"));
		}
	} ,{} });
}
shared_ptr<GUI::SimpleForm> WARPGUI;
void reinitWARPGUI() {
	using namespace GUI;
	if (!WARPGUI) WARPGUI = make_shared<SimpleForm>();
	WARPGUI->title="Warp System";
	WARPGUI->content = "choose a warp point";
	WARPGUI->reset();
	for (auto& [k, v] : warps) {
		WARPGUI->addButton(GUIButton(string(k)));
	}
}
void sendWARPGUI(WPlayer wp) {
	using namespace GUI;
	sendForm(wp, SimpleFormBinder(WARPGUI, [](WPlayer wp, SimpleFormBinder::DType d) {
		if (d.set) {
			wp.runcmdA("warp", "go",  QUOTE(d.val().second));
		}
	}));
}
void schTask() {
	Handler::schedule(RepeatingTask([] {
		clock_t expire = clock() - TPexpire;
		for (auto it = reqs.begin(); it != reqs.end();) {
			if (it->time <= expire) {
				auto oldit = it;
				++it;
				DoCloseReq(oldit, TPCloseReason::timeout);
			}
			else break;
		}
		}, 10)); //5sec
}

bool oncmd_tpa(CommandOrigin const& ori, CommandOutput& outp, MyEnum<direction> dir, CommandSelector<Player>& target) {
	auto res = target.results(ori);
	if (!Command::checkHasTargets(res, outp) || (res.count()!=1)) return false;
	WPlayer t{ **res.begin() };
	auto reqres=CanMakeReq(ori.getName(), t.getName());
	switch (reqres) {
	case TPFailReason::success:
	{
		DoMakeReq({ *(ServerPlayer*)ori.getEntity() }, t, dir);
		return true;
	}
		break;
	case TPFailReason::ratelimit: {
		outp.error(_TRS("tp.fail.rate"));
		return false;
	}
		break;
	case TPFailReason::inreq: {
		outp.error(_TRS("tp.fail.inreq"));
		return false;
	}
		break;
	case TPFailReason::blocked: {
		outp.error(_TRS("tp.fail.blocked"));
		return false;
	}
		break;
	}	
	return false;
}
enum class TPAOP :int {
	ac=1,de=2,cancel=3,toggle=4
};
bool oncmd_tpa2(CommandOrigin const& ori, CommandOutput& outp, MyEnum<TPAOP> op){
	switch (op) {
	case TPAOP::ac: {
		auto it = DoFetchReq_receiver(ori.getName());
		if (!it.set) {
			return false;
		}
		DoCloseReq(it.val(), TPCloseReason::accept);
		break;
	}
	case TPAOP::de: {
		auto it = DoFetchReq_receiver(ori.getName());
		if (!it.set) {
			return false;
		}
		DoCloseReq(it.val(), TPCloseReason::deny);
		break;
	}
	case TPAOP::cancel: {
		auto it = DoFetchReq_sender(ori.getName());
		if (!it.set) {
			return false;
		}
		DoCloseReq(it.val(), TPCloseReason::cancel);
		
		break;
	}
	case TPAOP::toggle: {
		CHash hs = do_hash(ori.getName());
		auto state = !tpaSetting[hs].Accept;
		tpaSetting[hs].Accept = state;
		outp.addMessage("new state " + std::to_string(state));
		
		break;
	}
	}
	return true;
}
#pragma endregion
#pragma region WARP
void saveWarps() {
	WBStream ws;
	ws.apply(warps);
	db->put("warps", ws);
	reinitWARPGUI();
}
enum WARPOP :int {
	go=1,add=2,ls=3,del=4,gui=5
};
enum class HOMEOP :int {
	go = 1, add = 2, ls = 3, del = 4
};
bool oncmd_warp(CommandOrigin const& ori, CommandOutput& outp, MyEnum<WARPOP> op, optional<string>& val) {
	switch (op)
	{
	case gui: {
		sendWARPGUI(MakeWP(ori).val());
		return true;
		break;
	}
	case add: {
		if (ori.getPermissionsLevel() < 1) return false;
		WActor wa{ MakeWP(ori).val() };
		warps.emplace(val.value(), wa);
		saveWarps();
		
		break;
	}
	case del: {
		if (ori.getPermissionsLevel() < 1) return false;
		warps.erase(val.value());
		saveWarps();
		
		break;
	}
	case ls: {
		for (auto& i : warps) {
			outp.addMessage(i.first + " " + i.second.toStr());
		}
		
		break;
	}
	case go: {
		auto it = warps.find(val.value());
		if (it == warps.end()) {
			outp.error(_TRS("home.not.found"));
			return false;
		}
		it->second.teleport(MakeWP(ori).val());
		
		break;
	}
	default:
		break;
	}
	return true;
}
#pragma endregion
#pragma region HOME
bool generic_home(CommandOrigin const& ori, CommandOutput& outp, Homes& hm, MyEnum<HOMEOP> op, optional<string>& val) {
	switch (op)
	{
	case HOMEOP::add: {
		if (ori.getPermissionsLevel() == 0 && hm.data.size() >= MAX_HOMES) {
			outp.error(_TRS("home.is.full"));
			return false;
		}
		WPlayer wp = MakeWP(ori).val();
		Vec4 vc{ wp };
		IVec2 startVc{ wp->getPos() };
		IVec2 endVc{ wp->getPos() };
		startVc += -HOME_DISTANCE_LAND;
		endVc += HOME_DISTANCE_LAND;
		if (!checkLandOwnerRange_stub(startVc,endVc,vc.dimid,wp.getXuid())) {
			outp.error(_TRS("home.near.others.land"));
			return false;
		}
		hm.data.emplace_back(val.value(), vc);
		hm.save();
		outp.success("home point `" + val.value() + "` was set successfully!!");
		
	break;
	}
	case HOMEOP::del: {
		hm.data.remove_if([&](auto& x) {
			return x.name == val.value();
			});
		hm.save();
		
		break;
	}
	case HOMEOP::ls: {
		for (auto& i : hm.data) {
			outp.addMessage(i.name + " " + i.pos.toStr());
		}
		
		break;
	}
	case HOMEOP::go: {
		for (auto& i : hm.data) {
			if (i.name == val.value()) {
				i.pos.teleport(MakeWP(ori).val());
				
				return true;
			}
		}
		outp.error(_TRS("home.not.found"));
		return false;
		break;
	}
	default:
		break;
	}
	return true;
}
bool oncmd_home(CommandOrigin const& ori, CommandOutput& outp, MyEnum<HOMEOP> op, optional<string>& val) {
	return generic_home(ori, outp, getHomeInCache(MakeWP(ori).val().getXuid()), op, val);
}
bool oncmd_homeAs(CommandOrigin const& ori, CommandOutput& outp, string const& target, MyEnum<HOMEOP> op, optional<string>& val) {
	return generic_home(ori, outp, getHomeInCache(XIDREG::str2id(target).val()), op, val);
}
#pragma endregion
#pragma region BACK
bool oncmd_back(CommandOrigin const& ori, CommandOutput& outp) {
	ServerPlayer* sp = (ServerPlayer*)ori.getEntity();
	if (!deathPos._map.count(sp)) {
		outp.error(_TRS("home.not.found"));
		return false;
	}
	deathPos[sp].teleport({ *sp });
	deathPos._map.erase(sp);
	
	return true;
}
#pragma endregion
bool oncmd_suicide(CommandOrigin const& ori, CommandOutput& outp) {
	auto wp = MakeWP(ori);
	if (wp.set) {
		wp.val().kill();
		
	}
	return true;
}
void loadCfg() {
	try {
		ConfigJReader jr("config/tpa.json");
		jr.bind("max_homes", MAX_HOMES, 5);
		jr.bind("tpa_timeout", TPexpire, CLOCKS_PER_SEC * 20);
		jr.bind("tpa_ratelimit", TPratelimit, CLOCKS_PER_SEC * 5);
		jr.bind("home_land_distance", HOME_DISTANCE_LAND, -1);
		jr.bind("BACK_ENABLED", BACK_ENABLED, true);
		jr.bind("SUICIDE_ENABLED", SUICIDE_ENABLED, true);
	}
	catch (string e) {
		LOG("JSON ERROR", e);
		throw 0;
	}
}
static bool onReload(CommandOrigin const& ori, CommandOutput& outp) {
	loadCfg();
	return true;
}
void loadall() {
	string val;
	if (db->get("warps", val)) {
		RBStream rs{ val };
		rs.apply(warps);
	}
	loadCfg();
}
void InitLUAAPI();
#include<bdxlua.h>
void tpa_entry() {
	registerLuaLoadHook(InitLUAAPI);
	checkLandOwnerRange_stub({ 0, 0 }, { 0, 0 }, 0, 0);
	db=MakeKVDB(GetDataPath("tpa"), true, 8);
	loadall();
	reinitWARPGUI();
	schTask();
	addListener([](RegisterCommandEvent&) {
		CEnum<direction> _1("tpdir", { "to","here" });
		CEnum<WARPOP> _2("warpop", {"go","add","ls","del","gui"});
		CEnum<HOMEOP> _4("homeop", { "go","add","ls","del" });
		CEnum<TPAOP> _3("tpaop", {"ac","de","cancel","toggle"});
		MakeCommand("tpa", "tpa system", 0);
		MakeCommand("warp", "warp system", 0);
		MakeCommand("home", "home system", 0);
		MakeCommand("homeAs", "run home as a player", 1);
		
		CmdOverload(warp, oncmd_warp,"op","name");
		CmdOverload(home, oncmd_home,"op","name");
		CmdOverload(homeAs, oncmd_homeAs,"Pname","op","home_name");
		CmdOverload(tpa, oncmd_tpa, "dir","target");
		CmdOverload(tpa, oncmd_tpa2, "op");
		MakeCommand("tpareload", "reload tpa", 1);
		CmdOverload(tpareload, onReload);
		if (BACK_ENABLED) {
			MakeCommand("back", "back to last deathpoint", 0);
			CmdOverload(back, oncmd_back);
		}
		if (SUICIDE_ENABLED) {
			MakeCommand("suicide", "kill yourself", 0);
			CmdOverload(suicide, oncmd_suicide);
		}
	});
	if (BACK_ENABLED) {
		addListener([](PlayerDeathEvent& ev) {
			auto p = ev.getPlayer();
			deathPos[p.v] = Vec4{ p };
			p.sendText(_TRS("tpa.back.use"));
			});
	}
}