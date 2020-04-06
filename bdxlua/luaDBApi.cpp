#include"pch.h"
#include"luaBindings.h"
int lb_dbget(lua_State* L) {
	try {
		LuaFly fly{ L };
		string rv;
		string mainkey;
		xstring key;
		fly.pops(key, mainkey);
		auto has = db->get(mainkey + "-" + key, rv);
		if (has)
			fly.push(rv);
		else
			lua_pushnil(L);
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
