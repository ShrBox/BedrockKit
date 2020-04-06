// bdxlua.cpp : 定义 DLL 的导出函数。
//

#include "pch.h"
#include "bdxlua.h"
#include "luaBindings.h"
static vector<void(*)()> onLuaReloaded;
BDXLUA_API void registerLuaLoadHook(void(*x)()) {
	onLuaReloaded.push_back(x);
}
lua_State* L;
unique_ptr<KVDBImpl> db;
BDXLUA_API optional<long long> call_lua(const char* name, static_queue<ldata_ref_t,8> const& arg) {
	lua_getglobal(L, "EXCEPTION");
	auto EHIDX = lua_gettop(L);
	if (lua_getglobal(L, name) == 0) {
		printf("[LUA] function %s not found\n", name);
		lua_settop(L, EHIDX - 1);
		return {};
	}
	LuaFly fly{ L };
	for (auto& i : arg) {
		if (i.isLong) {
			fly.push(i.asLL());
		}
		else {
			fly.push(i.asStr());
		}
	}
	if (!fly.pCall(name, arg.size(), -1, EHIDX)) {
		lua_settop(L, EHIDX - 1);
		return {};
	}
	auto rvcount = fly.top() - EHIDX;
	if (!rvcount) {
		lua_settop(L, EHIDX - 1);
		return { {0} };
	}
	else {
		try {
				if (lua_isinteger(L, -1)) {
					long long x;
					fly.pop(x);
					lua_settop(L, EHIDX - 1);
					return { {x} };
				}
				else {
					lua_settop(L, EHIDX - 1);
					return { {0} };
				}
		}
		catch (...) {
			return {};
		}
	}
}

void reg_all_bindings() {
	lua_register(L, "lbind", lua_call_bind_proxy);
	lua_register(L, "L", lua_call_bind_proxy);
	lua_register(L, "sendText", lb_sendText);
	lua_register(L, "bcText", lb_bctext);
	lua_register(L, "runCmd", lb_runcmd);
	lua_register(L, "runCmdAs", lb_runcmdAs);
	lua_register(L, "oList", lb_oList);
	lua_register(L, "oListV", lb_oListV);
	lua_register(L, "GUI", lua_bind_GUI);
	lua_register(L, "dget", lb_dbget);
	lua_register(L, "ddel", lb_dbdel);
	lua_register(L, "dput", lb_dbput);
	lua_register(L, "schedule", lb_schedule);
	lua_register(L, "cancel", lb_sch_cancel);
	lua_register(L, "getPos", lb_getpos);
	lua_register(L, "Listen", lb_regEventL);
	lua_register(L, "Unlisten", lb_unregEventL);
	for (auto i : onLuaReloaded) {
		i();
	}
}
bool dofile_lua(string const& name) {
	std::ifstream ifs(name);
	std::string str((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
	auto state = luaL_loadbufferx(L, str.data(), str.size(), name.c_str(), nullptr);
	if (state != 0) {
		auto str = lua_tostring(L, -1);
		printf("load lua error %s [%d\n", str, state);
		return false;
	}
	else {
		auto rv = lua_pcall(L, 0, LUA_MULTRET, 0);
		if (rv != 0) {
			auto str = lua_tostring(L, -1);
			printf("call lua err %s [%d\n", str, rv);
			return false;
		}
	}
	return true;
}
#include<filesystem>
#include"framework.h"
string wstr2str(std::wstring const& str) {
	string res;
	int len = WideCharToMultiByte(GetACP(), 0, str.c_str(), str.size(), nullptr, 0, nullptr, nullptr);
	res.append(len, 0);
	WideCharToMultiByte(GetACP(), 0, str.c_str(), str.size(), res.data(), res.size(), nullptr, nullptr);
	return res;
}
bool loadlua() {
	if (L) {
		lua_close(L);
		lua_scheduler_reload();
	}
	L = luaL_newstate();
	luaL_openlibs(L);
	reg_all_bindings();
	using namespace std::filesystem;
	{
		directory_iterator ent("lua");
		for (auto& i : ent) {
			if (i.is_regular_file() && i.path().extension() == ".lua") {
				dofile_lua(wstr2str(i.path()));
			}
		}
	}
	if (lua_getglobal(L, "EXCEPTION") == 0) {
		printf("[LUA/Warn] no EXCEPTION handler found!!!\n");
		return false;
	}
	lua_pop(L, 1);
	return true;
}
bool oncmd_lua(CommandOrigin const& ori, CommandOutput& outp, string& fn) {
	auto caller = ori.getName();
	lua_getglobal(L, "EXCEPTION");
	auto EHIDX = lua_gettop(L);
	if (lua_getglobal(L, ("u_" + fn).c_str()) == 0) {
		outp.error("cant find fn");
		return false;
	}
	lua_pushlstring(L, caller.data(), caller.size());
	auto rv=lua_pcall(L, 1, 0, EHIDX);
	if (rv != 0) {
		auto str = lua_tostring(L, -1);
		outp.error(string("lua error ")+str);
		printf("[LUA] lua error in lcall %s for %s,->%s\n", ("u_" + fn).c_str(),caller.c_str(),str);
		return false;
	}
	return true;
}
bool oncmd_reloadlua(CommandOrigin const& ori, CommandOutput& outp) {
	return loadlua();
}
#include<filesystem>
void glang_send(WPlayer wp, const string& payload);
string getForm(const string& name);
bool oncmd_gui(CommandOrigin const& ori, CommandOutput& outp,string& v) {
	auto wp = MakeWP(ori);
	if (v.find('.') != v.npos || v.find('/') != v.npos || v.find('\\') != v.npos) return false;
	glang_send(wp.val(), getForm("u_" + v));
	return true;
}
bool oncmd_luacmd(CommandOrigin const& ori, CommandOutput& outp, CommandMessage& m) {
	string msg=m.get(ori);
	auto pos = msg.find(' ');
	string PRE = msg.substr(0, pos);
	string NEX;
	if (pos != PRE.npos)
		NEX = msg.substr(pos + 1);
	CallEvent("onLCMD", { &ori.getName(),&PRE,&NEX });
	return true;
}
bool oncmd_dumpdb(CommandOrigin const& ori, CommandOutput& outp,string& target) {
	bool flag = false;
	db->iter([&](string_view k) {
		if (k._Starts_with(target)) {
			flag = true;
			string v;
			db->get(k, v);
			outp.addMessage(string{ k.substr(target.size()) }+" = "+v);
		}
		else {
			if (flag) return false;  //db->iter's key is ordered
		}
		return true;
	});
	return true;
}
void entry() {
	using namespace std::filesystem;
	db = MakeKVDB(GetDataPath("lua"),true,2);
	create_directory("gui");
	addListener([](RegisterCommandEvent&) {
		MakeCommand("lcall", "call lua fn", 0);
		CmdOverload(lcall, oncmd_lua, "func name");
		MakeCommand("lreload", "reload lua", 1);
		CmdOverload(lreload, oncmd_reloadlua);
		MakeCommand("gui", "show gui", 0);
		CmdOverload(gui, oncmd_gui,"path");
		MakeCommand("l", "use lua command", 1);
		CmdOverload(l, oncmd_luacmd, "cmd");
		MakeCommand("lua_db", "dump lua db", 1);
		CmdOverload(lua_db, oncmd_dumpdb, "player_name-");
	});
	addListener([](PlayerJoinEvent& ev) {
		auto& name = ev.getPlayer().getName();
		CallEvent("onJoin", { &name });
	});
	addListener([](PlayerLeftEvent& ev) {
		auto& name = ev.getPlayer().getName();
		CallEvent("onLeft", { &name });
		});
	addListener([](PlayerChatEvent& ev) {
		auto& name = ev.getPlayer().getName();
		if (!CallEvent("onChat", { &name,&ev.getChat() })) ev.setCancelled();
	});
	addListener([](PlayerCMDEvent& ev) {
		auto& name = ev.getPlayer().getName();
		if (!CallEvent("onCMD", { &name,&ev.getCMD() })) ev.setCancelled();
	});
	addListener([](MobDeathEvent& ev) {
		auto& src=ev.getSource();
		if (src.isEntitySource() || src.isChildEntitySource()) {
			auto id=src.getEntityUniqueID();
			Actor* ac=LocateS<ServerLevel>()->fetchEntity(id, false);
			ServerPlayer* sp=MakeSP(ac);
			if (!sp) return;
			WPlayer wp{ *sp };
			CallEvent("onPlayerKillMob", { &wp.getName(),ev.getMob()->getEntityTypeId() });
		}
	});
	addListener([](ServerStartedEvent&) {loadlua();});
}
