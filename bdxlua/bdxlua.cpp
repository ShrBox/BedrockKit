// bdxlua.cpp : 定义 DLL 的导出函数。
//

#include "pch.h"
#include "bdxlua.h"
lua_State* L;
unique_ptr<KVDBImpl> db;
BDXLUA_API unordered_map<string, luacb_t> bindings;
BDXLUA_API optional<ldata_t> call_lua(const char* name, static_queue<ldata_ref_t,8> const& arg) {
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
		return {};
	}
	else {
		try {
			if (lua_isstring(L, -1)) {
				string x;
				fly.pop(x);
				lua_settop(L, EHIDX - 1);
				return { {std::move(x)} };
			}
			else {
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
		}
		catch (...) {
			return {};
		}
	}
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
int lb_dbget(lua_State* L) {
	try {
		LuaFly fly{ L };
		string rv;
		string mainkey;
		xstring key;
		fly.pops(key,mainkey);
		db->get(mainkey + "-" + key, rv);
		fly.push(rv);
		return 1;
	}
	catch (string e) {
		luaL_error(L, e.c_str());
		return 0;
	}
}
int lb_dbput(lua_State* L) {
	try {
	LuaFly fly{ L };
	string mainkey;
	xstring key;
	xstring cont;
	fly.pops(cont, key, mainkey);
	db->put(mainkey + "-" + key, cont);
	return 0;
}
catch (string e) {
	luaL_error(L, e.c_str());
	return 0;
}
}
int lb_dbdel(lua_State* L) {
	try {
	LuaFly fly{ L };
	string mainkey;
	xstring key;
	fly.pops(key, mainkey);
	db->del(mainkey + "-" + key);
	return 0;
}
catch (string e) {
	luaL_error(L, e.c_str());
	return 0;
}
}
void reg_all_bindings() {
	lua_register(L, "lbind", lua_call_bind_proxy);
	lua_register(L, "L", lua_call_bind_proxy);
	lua_register(L, "sendText", lb_sendText);
	lua_register(L, "runCmd", lb_runcmd);
	lua_register(L, "runCmdAs", lb_runcmdAs);
	lua_register(L, "oList", lb_oList);
	lua_register(L, "oListV", lb_oListV);
	lua_register(L, "GUI", lua_bind_GUI);
	lua_register(L, "dget", lb_dbget);
	lua_register(L, "ddel", lb_dbdel);
	lua_register(L, "dput", lb_dbput);
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
	return call_lua("onLCMD", { &ori.getName(),&msg }).set;
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
	db = MakeKVDB(GetDataPath("lua"));
	create_directory("gui");
	loadlua();
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
		call_lua("onJoin", { &name });
	});
	addListener([](PlayerChatEvent& ev) {
		auto& name = ev.getPlayer().getName();
		auto data=call_lua("onChat", { &name,&ev.getChat() });
		if (data.Set() && std::get<0>(data.value()) == -1) ev.setCancelled();
	});
	addListener([](PlayerCMDEvent& ev) {
		auto& name = ev.getPlayer().getName();
		auto data = call_lua("onCMD", { &name,&ev.getCMD() });
		if (data.Set() && std::get<0>(data.value()) == -1) ev.setCancelled();
	});
	addListener([](MobDeathEvent& ev) {
		auto& src=ev.getSource();
		if (src.isEntitySource() || src.isChildEntitySource()) {
			auto id=src.getEntityUniqueID();
			Actor* ac=LocateS<ServerLevel>()->fetchEntity(id, false);
			ServerPlayer* sp=MakeSP(ac);
			if (!sp) return;
			WPlayer wp{ *sp };
			call_lua("onPlayerKillMob", { &wp.getName(),ev.getMob()->getEntityTypeId() });
		}
	});
}
