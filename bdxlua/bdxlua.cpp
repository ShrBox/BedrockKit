// bdxlua.cpp : 定义 DLL 的导出函数。
//

#include "pch.h"
#include "bdxlua.h"
lua_State* L;
BDXLUA_API unordered_map<string, luacb_t> bindings;
bool _lpcall(int in, int out,const char* name) {
	auto rv = lua_pcall(L, in, out, -in-1);
	if (rv != 0) {
		printf("[LUA] call lua %s error %s", name, lua_tostring(L, -1));
		return false;
	}
	return true;
}
ldata_t _lpget(int i) {
	if (lua_isinteger(L, i)) {
		return { lua_tointeger(L,i) };
	}
	else {
		if (lua_isstring(L, i)) {
			size_t ssz;
			auto str = lua_tolstring(L, i, &ssz);
			return { string{str,ssz} };
		}
		else {
			return { 0 };
		}
	}
}
BDXLUA_API optional<ldata_t> call_lua(const char* name, vector<ldata_t> const& arg) {
	int N = lua_gettop(L);
	if (lua_getglobal(L, name) == 0) {
		lua_settop(L, N);
		return {};
	}
	for (auto& i : arg) {
		if (i.index() == 0) {
			lua_pushinteger(L, std::get<0>(i));
		}
		else {
			lua_pushlstring(L, std::get<1>(i).data(), std::get<1>(i).size());
		}
	}
	if (!_lpcall(arg.size(), -1, name)) {
		lua_settop(L, N);
		return {};
	}
	auto n = lua_gettop(L);
	if (n != 0) {
		auto rval = _lpget(-1);
		lua_settop(L, N);
		return rval;
	}
	lua_settop(L, N);
	return { {0} };
}
int lua_call_bind_proxy(lua_State* L) {
	auto n = lua_gettop(L);
	if (n < 1) {
		luaL_error(L, "lbind needs len(args)>=1,use lbind(\"name\",...)");
		return 0;
	}
	if (!lua_isstring(L, 1)) {
		lua_settop(L, 0);
		luaL_error(L, "use lbind(\"name\",...)");
		return 0;
	}
	auto str = lua_tostring(L, 1);
	auto it = bindings.find(str);
	if (it == bindings.end()) {
		lua_settop(L, 0);
		luaL_error(L, "binding %s not found!", str);
		return 0;
	}
	vector<ldata_t> arg;
	for (int i = 2; i <= n; ++i) {
		auto data = _lpget(i);
		arg.emplace_back(std::move(data));
	}
	lua_settop(L, 0);
	try {
		auto suc = it->second(arg);
			if (suc.index() == 0) {
				//int
				lua_pushinteger(L, std::get<0>(suc));
			}
			else {
				lua_pushlstring(L, std::get<1>(suc).data(), std::get<1>(suc).size());
			}
		return 1;
	}
	catch (string e) {
		luaL_error(L, "lbind error in %s : %s",str,e.c_str());
		return 0;
	}
	return 0;
}
int lb_sendText(lua_State* L) {
	//name text [type]
	auto n = lua_gettop(L);
	if (n < 2) {
		luaL_error(L, "sendText: need at least 2 args");
		return 0;
	}
	TextType tp{ RAW };
	if (n == 3) {
		tp = (TextType)lua_tointeger(L, 3);
	}
	size_t namel, textl;
	auto _n=lua_tolstring(L, 1, &namel);
	auto _t=lua_tolstring(L, 2, &textl);
	string_view name(_n, namel);
	string_view text(_t, textl);
	auto sp = LocateS<WLevel>()->getPlayer(name);
	if (!sp.Set()) {
		luaL_error(L, "sendText: player not online");
		return 0;
	}
	sp.value().sendText(text, tp);
	lua_pop(L, n);
	return 0;
}
int lb_runcmd(lua_State* L) {
	auto n = lua_gettop(L);
	if (n < 1) {
		luaL_error(L,"runcmd(cmd)");
		return 0;
	}
	size_t s1;
	auto _n = lua_tolstring(L, 1, &s1);
	string_view cmd(_n, s1);
	auto rv = BDX::runcmd(string{ cmd });
	lua_pop(L, n);
	lua_pushboolean(L, rv);
	return 1;
}
int lb_runcmdAs(lua_State* L) {
	auto n = lua_gettop(L);
	if (n < 2) {
		luaL_error(L, "runcmdAs(name,cmd)");
		return 0;
	}
	size_t s1;
	auto _n = lua_tolstring(L, 2, &s1);
	string cmd(_n, s1);
	size_t s2;
	auto _n2 = lua_tolstring(L, 1, &s2);
	string name(_n2, s2);
	lua_pop(L, n);
	auto ply = LocateS<WLevel>()->getPlayer(name);
	if (ply.Set()) {
		lua_pushboolean(L, ply.value().runcmd(cmd));
	}
	else {
		lua_pushboolean(L, false);
	}
	return 1;
}
int lb_oListV(lua_State* L) {
	lua_settop(L, 0);
	auto& s = GUI::getPlayerListView();
	lua_pushlstring(L, s.data(),s.size());
	return 1;
}
int lb_oList(lua_State* L) {
	lua_settop(L, 0);
	auto ps = LocateS<WLevel>()->getUsers();
	lua_checkstack(L, ps.size()+20);
	lua_newtable(L);
	int idx = 0;
	for (auto i : ps) {
		lua_pushstring(L, i.getName().c_str());
		lua_rawseti(L, -2, ++idx);
	}
	return 1;
}
void reg_all_bindings() {
	lua_register(L, "lbind", lua_call_bind_proxy);
	lua_register(L, "sendText", lb_sendText);
	lua_register(L, "runCmd", lb_runcmd);
	lua_register(L, "runCmdAs", lb_runcmdAs);
	lua_register(L, "oList", lb_oList);
	lua_register(L, "oListV", lb_oListV);
	lua_register(L, "GUI", lua_bind_GUI);
}
bool loadlua() {
	if (L) {
		lua_close(L);
	}
	L = luaL_newstate();
	luaL_openlibs(L);
	reg_all_bindings();
	std::ifstream ifs("lua/main.lua");
	if (ifs.fail()) {
		printf("Cannot open lua file lua/main.lua\n");
		return false;
	}
	std::string str((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
	auto state = luaL_loadbufferx(L, str.data(), str.size(), "main.lua", nullptr);	
	if (state != 0) {
		auto str = lua_tostring(L, -1);
		printf("load lua error %s [%d\n", str,state);
		return false;
	}
	else {
		auto rv = lua_pcall(L, 0, LUA_MULTRET, 0);
		if (rv != 0) {
			auto str = lua_tostring(L, -1);
			printf("call lua err %s [%d\n", str,rv);
			return false;
		}
	}
	return true;
}
bool oncmd_lua(CommandOrigin const& ori, CommandOutput& outp, string& fn) {
	auto caller = ori.getName();
	if (lua_getglobal(L, ("u_" + fn).data()) == 0) {
		outp.error("cant find fn");
		return false;
	}
	lua_pushlstring(L, caller.data(), caller.size());
	auto rv=lua_pcall(L, 1, 0, 0);
	if (rv != 0) {
		auto str = lua_tostring(L, -1);
		outp.error(string("lua error ")+str);
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
void entry() {
	using namespace std::filesystem;
	create_directory("gui");
	loadlua();
	addListener([](RegisterCommandEvent&) {
		MakeCommand("lcall", "call lua fn", 0);
		CmdOverload(lcall, oncmd_lua, "func name");
		MakeCommand("lreload", "reload lua", 1);
		CmdOverload(lreload, oncmd_reloadlua);
		MakeCommand("gui", "show gui", 0);
		CmdOverload(gui, oncmd_gui,"path");
	});
	addListener([](PlayerJoinEvent& ev) {
		auto& name = ev.getPlayer().getName();
		call_lua("onJoin", { name });
	});
	addListener([](PlayerChatEvent& ev) {
		auto& name = ev.getPlayer().getName();
		auto data=call_lua("onChat", { name,ev.getChat() });
		if (data.Set() && std::get<0>(data.value()) == -1) ev.setCancelled();
	});
	addListener([](PlayerCMDEvent& ev) {
		auto& name = ev.getPlayer().getName();
		auto data = call_lua("onCMD", { name,ev.getCMD() });
		if (data.Set() && std::get<0>(data.value()) == -1) ev.setCancelled();
	});
}
