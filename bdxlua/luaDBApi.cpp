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
int lb_dbforeach(lua_State* L) {
	try {
		LuaFly fly{ L };
		xstring prefix,cb;
		fly.pops(cb,prefix);
		bool flag = false;
		db->iter([prefix,&fly,cb,&flag](string_view k, string_view v) {
			if (k._Starts_with(prefix)) {
				flag = true;
				fly.Call(cb.c_str(), 0, k, v);
			}
			else {
				if (flag) return false;
			}
			return true;
		});
		return 0;
	}
	catch (string e) {
		luaL_error(L, e.c_str());
		return 0;
	}
}
int lb_dbremove_prefix(lua_State* L) {
	try {
		LuaFly fly{ L };
		xstring prefix, cb;
		fly.pops(cb, prefix);
		bool flag = false;
		vector<string> to_del;
		db->iter([prefix,&flag,&to_del](string_view k) {
			if (k._Starts_with(prefix)) {
				flag = true;
				to_del.emplace_back(prefix);
			}
			else {
				if (flag) return false;
			}
			return true;
		});
		for (auto& i : to_del) {
			db->del(i);
		}
		return 0;
	}
	catch (string e) {
		luaL_error(L, e.c_str());
		return 0;
	}
}