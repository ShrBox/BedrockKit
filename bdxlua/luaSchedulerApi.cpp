#include"pch.h"
#include"luaBindings.h"
#include<api\scheduler\scheduler.h>
#include<unordered_set>
using std::unordered_set;
static unordered_set<taskid_t> LUA_TASKS;
static int lb_sch_gettid(lua_State* L) {
	lua_pushinteger(L, Handler::gtaskid+1);
	return 1;
}
static int lb_sch_cancel_ex(lua_State* L) {
	int n = lua_gettop(L);
	if (n != 1 || !lua_isinteger(L, 1)) {
		luaL_error(L, "cancel(taskid)");
		return 0;
	}
	taskid_t tid;
	tid = taskid_t(lua_tointeger(L, 1));
	LUA_TASKS.erase(tid);
	lua_pushboolean(L, Handler::cancel(tid));
	return 1;
}
static int lb_schedule_ex(lua_State* L) {
	try {
		int n = lua_gettop(L);
		if (n < 2) {
			throw "schedule_ex(cb,interval,delay)"s;
		}
		int delay = 0, interval = 0;
		if (!lua_isinteger(L,1) || !lua_isinteger(L, 2)) {
			throw "schedule_ex(cb,interval,delay)"s;
		}
		long long CB = lua_tointeger(L, 1);
		interval = int(lua_tointeger(L, 2));
		if (n == 3) {
			delay = int(lua_tointeger(L, 3));
		}
		auto tid = Handler::schedule(DelayedRepeatingTask([CB]() {
			lua_getglobal(::L, "EXCEPTION");
			auto EHIDX = lua_gettop(::L);
			lua_getglobal(::L, "_TASKS");
			lua_rawgeti(::L, -1, CB);
			if (!LuaFly(::L).pCall("[scheduler task]", 0, 0, EHIDX)) {
				lua_settop(::L, EHIDX - 1);
				return;
			}
			lua_settop(::L, EHIDX - 1);
		}, delay, interval));
		LUA_TASKS.insert(tid);
		lua_settop(L, 0);
		lua_pushinteger(L, tid);
		return 1;
	}
	catch (string e) {
		luaL_error(L, e.c_str());
		return 0;
	}
}
void lua_scheduler_reload() {
	for (taskid_t id : LUA_TASKS) {
		Handler::cancel(id);
	}
}
/*
lua_register(L, "__schedule", lb_schedule_ex);
	lua_register(L, "__cancel", lb_sch_cancel_ex);
	lua_register(L, "__gettid", lb_sch_gettid);
*/
static const luaL_Reg R[] =
{
	{ "schedule",	lb_schedule_ex	},
	{ "cancel",	lb_sch_cancel_ex	},
	{ "gettid",lb_sch_gettid },
	{ NULL,		NULL	}
};
int lua_sch_entry(lua_State* L) {
	lua_newtable(L);
	luaL_setfuncs(L, R, 0);
	lua_setglobal(L, "schapi");
	return 0;
}