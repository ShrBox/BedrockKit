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
	auto rv = BDX::runcmd(cmd[0] == '/' ? string{ cmd } : ('/'+string{ cmd }));
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
	auto [rv,res] = BDX::runcmdEx(cmd[0] == '/' ? string{ cmd } : ('/' + string{ cmd }));
	lua_pop(L, n);
	lua_pushboolean(L, rv);
	lua_pushlstring(L, res.data(),res.size());
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
	string cmd(_n, s1);
	size_t s2;
	auto _n2 = lua_tolstring(L, 1, &s2);
	string name(_n2, s2);
	lua_pop(L, n);
	auto ply = LocateS<WLevel>()->getPlayer(name);
	if (ply.Set()) {
		lua_pushboolean(L, ply.val().runcmd(cmd[0] == '/' ? string{ cmd } : ('/' + string{ cmd })));
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
	lua_checkstack(L, ps.size() + 20);
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
		xstring cont;
		fly.pops(cont);
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
		WPlayer wp = LocateS<WLevel>()->getPlayer({ lua_tostring(L,1) }).val();
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