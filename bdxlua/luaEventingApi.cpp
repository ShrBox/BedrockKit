#include"pch.h"
#include"luaBindings.h"
#include<unordered_set>
static std::unordered_multimap<string, string> listeners;
static const std::unordered_set<string> AVAILABLE_EVENTS = {
"onJoin",
"onLeft",
"onChat",
"onCMD",
"onPlayerKillMob",
"onLCMD"
};
bool CallEvent(string const& name, static_queue<ldata_ref_t, 8> const& arg) {
	auto [it, ite] = listeners.equal_range(name);
	while (it != ite) {
		auto rv= call_lua(it->second.c_str(), arg);
		if (rv.set && rv.val() == -1) {
			return false;
		}
		++it;
	}
	return true;
}
int lb_regEventL(lua_State* L) {
	try {
		int n = lua_gettop(L);
		if (n != 2) {
			throw "2 args required"s;
		}
		if (!lua_isstring(L, 1) || !lua_isstring(L, 2)) {
			throw "str arg is required"s;
		}
		string name = lua_tostring(L, 1), cb = lua_tostring(L, 2);
		if (AVAILABLE_EVENTS.count(name) == 0) throw "event not available"s;
		listeners.insert({ name, cb });
		lua_settop(L, 0);
		return 0;
	}
	catch (string e) {
		luaL_error(L, e.c_str());
		return 0;
	}
}
int lb_unregEventL(lua_State* L) {
	try {
		int n = lua_gettop(L);
		if (n != 2) {
			throw "2 args required"s;
		}
		if (!lua_isstring(L, 1) || !lua_isstring(L, 2)) {
			throw "str arg is required"s;
		}
		string name = lua_tostring(L, 1), cb = lua_tostring(L, 2);
		if (AVAILABLE_EVENTS.count(name) == 0) throw "event not available"s;
		lua_settop(L, 0);
		auto [it, ite] = listeners.equal_range(name);
		while (it != ite) {
			if (it->second == cb) {
				listeners.erase(it);
				lua_pushboolean(L, 1);
				return 1;
			}
			++it;
		}
		lua_pushboolean(L, 0);
		return 1;
	}
	catch (string e) {
		luaL_error(L, e.c_str());
		return 0;
	}
}