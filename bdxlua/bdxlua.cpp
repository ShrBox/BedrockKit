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
	if (!fly.pCall(name, arg.size(), 1, EHIDX)) {
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
	lua_register(L, "L", lua_call_bind_proxy);
	lua_register(L, "sendText", lb_sendText);
	lua_register(L, "bcText", lb_bctext);
	lua_register(L, "runCmd", lb_runcmd);
	lua_register(L, "runCmdAs", lb_runcmdAs);
	lua_register(L, "runCmdEx", lb_runcmdex);
	lua_register(L, "Log", lb_log);
	lua_register(L, "oList", lb_oList);
	//lua_register(L, "oListV", lb_oListV);
	lua_register(L, "GUI", lua_bind_GUI);
	lua_register(L, "GUIR", lua_bind_GUI_Raw);
	lua_register(L, "dget", lb_dbget);
	lua_register(L, "ddel", lb_dbdel);
	lua_register(L, "dput", lb_dbput);
	lua_register(L, "dforeach", lb_dbforeach);
	lua_register(L, "ddel_prefix", lb_dbremove_prefix);
	lua_register(L, "getPos", lb_getpos);
	lua_register(L, "startThread", l_StartThread);
	lua_register(L, "stopThread", l_StopThread);
	lua_register(L, "TSendMsg", l_SendMsg);
	lua_register(L, "isOP", [](lua_State* L) {
		try {
			LuaFly lf{ L };
			string name;
			lf.pop(name);
			if (name == "Server") {
				lua_pushboolean(L, true);
				return 1;
			}
			lua_pushboolean(L, LocateS<WLevel>()->getPlayer(name).val().getPermLvl() != 0);
			return 1;
		}CATCH()
	});
	lb_fs_entry(L);
	lua_sch_entry(L);
	lua_register(L, "TSize", [](lua_State* L) {
		if (lua_type(L, 1) != LUA_TTABLE) {
			luaL_error(L, "table required in TSize");
			return 0;
		}
		lua_pushnil(L);
		int c = 0;
		while (lua_next(L, 1)) {
			c++;
			lua_pop(L, 1);
		}
		lua_settop(L, 0);
		lua_pushinteger(L, c);
		return 1;
	});
	for (auto i : onLuaReloaded) {
		i();
	}
}
static bool isReload=false;
bool dofile_lua(string const& name,bool HandleException=false) {
	std::ifstream ifs(name);
	std::string str((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
	int EHIDX = 0;
	if (HandleException) {
		lua_getglobal(L, "EXCEPTION");
		EHIDX = lua_gettop(L);
	}
	auto state = luaL_loadbufferx(L, str.data(), str.size(), name.c_str(), nullptr);
	if (state != 0) {
		auto str = lua_tostring(L, -1);
		printf("load lua error %s\n", str);
		if(!isReload)
		exit(1);
		return false;
	}
	else {
		auto rv = lua_pcall(L, 0, LUA_MULTRET, EHIDX);
		if (rv != 0) {
			auto str = lua_tostring(L, -1);
			printf("call lua err %s\n", str);
			if(!isReload)
			exit(1);
			return false;
		}
	}
	lua_settop(L, EHIDX == 0 ? 0 : (EHIDX - 1));
	return true;
}
#include<filesystem>
#include"framework.h"
string wstr2str(std::wstring const& str) {
	string res;
	int len = WideCharToMultiByte(GetACP(), 0, str.c_str(), int(str.size()), nullptr, 0, nullptr, nullptr);
	res.append(len, 0);
	WideCharToMultiByte(GetACP(), 0, str.c_str(), int(str.size()), res.data(), int(res.size()), nullptr, nullptr);
	return res;
}
bool loadlua() {
	if (L) {
		lua_close(L);
		lua_scheduler_reload();
		stopAll_Nowait();
	}
	L = luaL_newstate();
	luaL_openlibs(L);
	reg_all_bindings();
	using namespace std::filesystem;
	dofile_lua("lua/init.lua");
	{
		directory_iterator ent("lua");
		for (auto& i : ent) {
			if (i.is_regular_file() && i.path().extension() == ".lua" && i.path().filename()!="init.lua") {
				dofile_lua(wstr2str(i.path()),true);
			}
		}
	}
	if (lua_getglobal(L, "EXCEPTION") == 0) {
		printf("[LUA/Warn] no EXCEPTION handler found!!!\n");
		return false;
	}
	lua_settop(L, 0);
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
string getForm(string_view name);
namespace LEX {
	string Compile(string_view source, vector<string> const& val);
};
bool oncmd_gui(CommandOrigin const& ori, CommandOutput& outp,string& v) {
	auto wp = MakeWP(ori);
	if (v.find('.') != v.npos || v.find('/') != v.npos || v.find('\\') != v.npos) return false;
	glang_send(wp.val(), LEX::Compile(getForm("u_" + v), {}));
	return true;
}
bool oncmd_luacmd(CommandOrigin const& ori, CommandOutput& outp, CommandMessage& m) {
	string msg = m.get(ori);
	auto pos = msg.find(' ');
	string PRE = msg.substr(0, pos);
	string NEX;
	if (pos != PRE.npos)
		NEX = msg.substr(pos + 1);
	CallEvent("EH_onLCMD", { &ori.getName(),&PRE,&NEX });
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
bool oncmd_deldb(CommandOrigin const& ori, CommandOutput& outp, string& target) {
	bool flag = false;
	vector<string> to_del;
	db->iter([&](string_view k) {
		if (k._Starts_with(target)) {
			to_del.emplace_back(k);
			outp.addMessage(string{ k.substr(target.size()) }+" deleted");
		}
		else {
			if (flag) return false;  //db->iter's key is ordered
		}
		return true;
	});
	for (auto& i : to_del) {
		db->del(i);
	}
	return true;
}
void initEvents();
void entry() {
	using namespace std::filesystem;
	db = MakeKVDB(GetDataPath("lua"),true,2);
	create_directory("gui");
	initEvents();
	addListener([](RegisterCommandEvent&) {
		MakeCommand("lcall", "call lua fn", 0);
		CmdOverload(lcall, oncmd_lua, "func name");
		MakeCommand("lreload", "reload lua", 1);
		CmdOverload(lreload, oncmd_reloadlua);
		MakeCommand("gui", "show gui", 0);
		CmdOverload(gui, oncmd_gui,"path");
		MakeCommand("l", "use lua command", 0);
		CmdOverload(l, oncmd_luacmd, "cmd");
		MakeCommand("lua_db", "dump lua db", 1);
		CmdOverload(lua_db, oncmd_dumpdb, "prefix");
		MakeCommand("lua_db_del", "del lua db", 1);
		CmdOverload(lua_db_del, oncmd_deldb,"prefix");
	});
	addListener([](ServerStartedEvent&) {loadlua();isReload=true;});
}
