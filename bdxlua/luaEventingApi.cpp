#include"pch.h"
#include"luaBindings.h"
#include<unordered_set>
bool CallEvent(const char* name, static_queue<ldata_ref_t, 8> const& arg)
{
	if(lua_getglobal(L, name)==LUA_TNIL) return true;
	lua_getglobal(L, "EXCEPTION");
	auto EHIDX = lua_gettop(L);
	lua_pushnil(L);
	LuaFly fly{ L };
	while (lua_next(L, -3)) {
		//-1:func
		int N = lua_gettop(L);
		for (auto& i : arg) {
			if (i.isLong) {
				fly.push(i.asLL());
			}
			else {
				fly.push(i.asStr());
			}
		}
		if (!fly.pCall(name, arg.size(), 1, EHIDX)) {
			lua_settop(L, EHIDX - 2);
			return true;
		}
			if (lua_isinteger(L, -1)) {
				long long x;
				fly.pop(x);
				if (x == -1) {
					lua_settop(L, EHIDX - 2);
					return false;
				}
			}
		lua_settop(L,N - 1);
	}
	lua_settop(L, EHIDX - 2);
	return true;
}
void initEvents() {
	addListener([](PlayerJoinEvent& ev) {
		auto& name = ev.getPlayer().getName();
		CallEvent("EH_onJoin", { &name });
		});
	addListener([](PlayerLeftEvent& ev) {
		auto& name = ev.getPlayer().getName();
		CallEvent("EH_onLeft", { &name });
		});
	addListener([](PlayerChatEvent& ev) {
		auto& name = ev.getPlayer().getName();
		if (!CallEvent("EH_onChat", { &name,&ev.getChat() })) ev.setCancelled();
		});
	addListener([](PlayerCMDEvent& ev) {
		auto& name = ev.getPlayer().getName();
		if (!CallEvent("EH_onCMD", { &name,&ev.getCMD() })) ev.setCancelled();
		});
	addListener([](MobDeathEvent& ev) {
		auto& src = ev.getSource();
		if (src.isEntitySource() || src.isChildEntitySource()) {
			auto id = src.getEntityUniqueID();
			Actor* ac = LocateS<ServerLevel>()->fetchEntity(id, false);
			ServerPlayer* sp = MakeSP(ac);
			if (!sp) return;
			WPlayer wp{ *sp };
			CallEvent("EH_onPlayerKillMob", { &wp.getName(),ev.getMob()->getEntityTypeId() });
		}
		});
}