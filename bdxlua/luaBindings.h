#pragma once
#include"bdxlua.h"
int lua_call_bind_proxy(lua_State* L);


int lb_sendText(lua_State* L);
int lb_runcmd(lua_State* L);
int lb_runcmdAs(lua_State* L);
int lb_oListV(lua_State* L);
int lb_oList(lua_State* L);
int lb_bctext(lua_State* L);
int lb_getpos(lua_State* L);


int lb_dbget(lua_State* L);
int lb_dbput(lua_State* L);
int lb_dbdel(lua_State* L);


void lua_scheduler_reload();
int lb_sch_cancel(lua_State* L);
int lb_schedule(lua_State* L);


extern unique_ptr<KVDBImpl> db;

bool CallEvent(string const& name, static_queue<ldata_ref_t, 8> const& arg);
int lb_regEventL(lua_State* L);
int lb_unregEventL(lua_State* L);

int lua_bind_GUI_Raw(lua_State* L);
int lua_bind_GUI(lua_State* L);
