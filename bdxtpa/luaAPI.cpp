#include"pch.h"
#include<bdxlua.h>
#include<lua.hpp>
#include<luafly.h>
#include"homeStorage.h"
string Vec2Str(vector<string_view> const& n) {
	string rv = "[\"";
	for (auto i : n) {
		rv.append(i);
		rv.append("\",\"");
	}
	if (rv.size() != 2) {
		rv.pop_back();
	}
	rv.back() = ']';
	return rv;
}
template<bool View=false>
int lb_getHomes(lua_State* L) {
	try {
		LuaFly lf{ L };
		string x;
		lf.pop(x);
		xuid_t xid=XIDREG::str2id(x).val();
		auto& homes=getHomeInCache(xid);
		vector<string_view> homenames;
		for (auto& i : homes.data) {
			homenames.emplace_back(i.name);
		}
		if constexpr (View) {
			lf.push(Vec2Str(homenames));
		}
		else {
			lf.push(homenames);
		}
		return 1;
	}CATCH()
}
void InitLUAAPI() {
	registerLuaModule(LModule([](lua_State* L){
		lua_register(L, "getHomes", lb_getHomes<false>);
		lua_register(L, "getHomesV", lb_getHomes<true>);
	}));
}