#pragma once
#include"bdxlua.h"
int lua_call_bind_proxy(lua_State* L);


int lb_sendText(lua_State* L);
int lb_runcmd(lua_State* L);
int lb_runcmdAs(lua_State* L);
int lb_runcmdex(lua_State* L);
int lb_oListV(lua_State* L);
int lb_oList(lua_State* L);
int lb_getpos(lua_State* L);
int lb_bctext(lua_State* L);

int lb_log(lua_State* L);


int lb_dbget(lua_State* L);
int lb_dbput(lua_State* L);
int lb_dbdel(lua_State* L);
int lb_dbforeach(lua_State* L);
int lb_dbremove_prefix(lua_State* L);


void lua_scheduler_reload();
int lb_sch_cancel_ex(lua_State* L);
int lb_schedule_ex(lua_State* L);
int lb_sch_gettid(lua_State* L);

extern unique_ptr<KVDBImpl> db;

bool CallEvent(const char* name, static_queue<ldata_ref_t, 8> const& arg);

int lua_bind_GUI_Raw(lua_State* L);
int lua_bind_GUI(lua_State* L);

int stopAll_Nowait();
int l_StartThread(lua_State* L);
int l_StopThread(lua_State* L);
int l_SendMsg(lua_State* L);

