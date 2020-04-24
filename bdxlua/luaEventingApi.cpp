#include"pch.h"
#include"luaBindings.h"
#include<unordered_set>
enum EVENTS_TYPE {
	ONJOIN = 0,
	ONLEFT = 1,
	ONCMD = 2,
	ONLCMD = 3,
	ONCHAT = 4,
	ONKILLMOB = 5,
	EVENTS_COUNT = 6
};
#include<vector>
using std::vector;
static vector<int> listeners[EVENTS_COUNT];
const static unordered_map<string_view, int> EVENT_IDS = {
	{"onJoin",ONJOIN},
	{"onLeft",ONLEFT},
	{"onCMD",ONCMD},
	{"onLCMD",ONLCMD},
	{"onChat",ONCHAT},
	{"onPlayerKillMob",ONKILLMOB}
};
int lb_Listen(lua_State* L) {
	string_view ev(LReadStr(L, 1));
	auto it = EVENT_IDS.find(ev);
	if (it == EVENT_IDS.end()) {
		luaL_error(L, "cant find event");
		return 0;
	}
	int LREF = luaL_ref(L, LUA_REGISTRYINDEX);
	listeners[it->second].push_back(LREF);
	return 0;
}
int lb_cancel(lua_State* L) {
	luaL_unref(L, LUA_REGISTRYINDEX, lua_tointeger(L, 1));
	return 0;
}
template<typename... TP>
bool CallEventEx(EVENTS_TYPE type, TP&&... arg)
{
	LuaFly fly(_L);
	LuaStackBalance B(_L);
	int EHIDX(getEHIDX());
	for (int lid: listeners[type]) {
		//-1:func
		lua_rawgeti(_L, LUA_REGISTRYINDEX,lid );
		fly.pushs(std::forward<TP>(arg)...);
		if (lua_pcall(_L, sizeof...(arg), 1, EHIDX) != 0) {
			printf("[LUA] callevent error : %s\n", lua_tostring(_L, -1));
			return true;
		}
		if (lua_isinteger(_L, -1)) {
			long long x;
			fly.pop(x);
			if (x == -1) {
				return false;
			}
		}
	}
	return true;
}

bool oncmd_luacmd(CommandOrigin const& ori, CommandOutput& outp, CommandMessage& m) {
	string msg = m.get(ori);
	auto pos = msg.find(' ');
	string_view PRE = string_view(msg).substr(0, pos);
	string_view NEX;
	if (pos != PRE.npos)
		NEX = string_view(msg).substr(pos + 1);
	CallEventEx(ONLCMD,ori.getName(),PRE,NEX);
	return true;
}
void initEvents() {
	addListener([](PlayerJoinEvent& ev) {
		auto& name = ev.getPlayer().getName();
		CallEventEx(ONJOIN, name);
		});
	addListener([](PlayerLeftEvent& ev) {
		auto& name = ev.getPlayer().getName();
		CallEventEx(ONLEFT, name);
		});
	addListener([](PlayerChatEvent& ev) {
		auto& name = ev.getPlayer().getName();
		if (!CallEventEx(ONCHAT, name,ev.getChat())) ev.setCancelled();
		});
	addListener([](PlayerCMDEvent& ev) {
		auto& name = ev.getPlayer().getName();
		if (!CallEventEx(ONCMD, name,ev.getCMD())) ev.setCancelled();
		});
	addListener([](MobDeathEvent& ev) {
		auto& src = ev.getSource();
		if (src.isEntitySource() || src.isChildEntitySource()) {
			auto id = src.getEntityUniqueID();
			Actor* ac = LocateS<ServerLevel>()->fetchEntity(id, false);
			ServerPlayer* sp = MakeSP(ac);
			if (!sp) return;
			WPlayer wp{ *sp };
			CallEventEx(ONKILLMOB, wp.getName(),ev.getMob()->getEntityTypeId());
		}
		});
	addListener([](RegisterCommandEvent&) {
		MakeCommand("l", "use lua command", 0);
		CmdOverload(l, oncmd_luacmd, "cmd");
	});
}
LModule luaEvent_module() {
	return LModule{ [](lua_State* L)->void {
		static bool inited = false;
		if (!inited) {
			inited = true;
			initEvents();
		}
		lua_register(L, "Listen2", lb_Listen);
		lua_register(L, "__Cancel", lb_cancel);
	},[](lua_State*) {
		for (auto& i : listeners) {
			i.clear();
		}
	} };
}