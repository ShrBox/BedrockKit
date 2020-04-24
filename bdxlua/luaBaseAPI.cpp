#include"pch.h"
#include"luaBindings.h"
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
	auto _n = lua_tolstring(L, 1, &namel);
	auto _t = lua_tolstring(L, 2, &textl);
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
		luaL_error(L, "runcmd(cmd)");
		return 0;
	}
	size_t s1;
	auto _n = lua_tolstring(L, 1, &s1);
	string_view cmd(_n, s1);
	bool rv;
	{
		LuaCtxSwapper S(L);
		rv = BDX::runcmd(string{ cmd });
	}
	lua_pop(L, n);
	lua_pushboolean(L, rv);
	return 1;
}
int lb_runcmdex(lua_State* L) {
	auto n = lua_gettop(L);
	if (n < 1) {
		luaL_error(L, "runcmdex(cmd)");
		return 0;
	}
	size_t s1;
	auto _n = lua_tolstring(L, 1, &s1);
	string_view cmd(_n, s1);
	std::pair<bool, string> rvres;
	{
		LuaCtxSwapper S(L);
		rvres=BDX::runcmdEx(string{ cmd });
	}
	lua_pop(L, n);
	lua_pushboolean(L, rvres.first);
	lua_pushlstring(L, rvres.second.data(), rvres.second.size());
	return 2;
}
int lb_runcmdAs(lua_State* L) {
	auto n = lua_gettop(L);
	if (n < 2) {
		luaL_error(L, "runcmdAs(name,cmd)");
		return 0;
	}
	size_t s1;
	auto _n = lua_tolstring(L, 2, &s1);
	string_view cmd(_n, s1);
	size_t s2;
	auto _n2 = lua_tolstring(L, 1, &s2);
	string_view name(_n2, s2);
	auto ply = LocateS<WLevel>()->getPlayer(name);
	if (ply.Set()) {
		bool rv;
		{
			LuaCtxSwapper S(L);
			rv=ply.val().runcmd(string{ cmd });
		}
		lua_pushboolean(L, rv);
	}
	else {
		lua_pushboolean(L, false);
	}
	return 1;
}
int lb_oListV(lua_State* L) {
	lua_settop(L, 0);
	auto& s = GUI::getPlayerListView();
	lua_pushlstring(L, s.data(), s.size());
	return 1;
}
int lb_oList(lua_State* L) {
	lua_settop(L, 0);
	auto ps = LocateS<WLevel>()->getUsers();
	lua_newtable(L);
	int idx = 0;
	for (auto i : ps) {
		lua_pushstring(L, i.getName().c_str());
		lua_rawseti(L, -2, ++idx);
	}
	return 1;
}
int lb_bctext(lua_State* L) {
	//broadcast text
	try {
		LuaFly fly{ L };
		string_view cont(LReadStr(L, 1));
		TextType tp{ RAW };
		if (lua_gettop(L)) {
			fly.pops(*(int*)&tp);
		}
		LocateS<WLevel>()->broadcastText(cont, tp);
		return 0;
	}
	CATCH()
}
int lb_getpos(lua_State* L) {
	try {
		if (lua_gettop(L) != 1) throw "getPos(name)"s;
		WPlayer wp = LocateS<WLevel>()->getPlayer(LReadStr(L, 1)).val();
		//x y z dim
		lua_pop(L, 1);
		auto& pos = wp->getPos();
		lua_pushnumber(L, pos.x);
		lua_pushnumber(L, pos.y);
		lua_pushnumber(L, pos.z);
		lua_pushinteger(L, wp.getDimID());
		return 4;
	}
	CATCH()
}
LModule luaBase_module() {
	return LModule{ [](lua_State* L) {
		lua_register(L, "sendText", lb_sendText);
		lua_register(L, "bcText", lb_bctext);
		lua_register(L, "runCmd", lb_runcmd);
		lua_register(L, "runCmdAs", lb_runcmdAs);
		lua_register(L, "runCmdEx", lb_runcmdex);
		lua_register(L, "oList", lb_oList);
		lua_register(L, "getPos", lb_getpos);
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
	} };
}