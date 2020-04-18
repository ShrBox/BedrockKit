#include"pch.h"
#include"InventoryAPI.h"
#include<bdxlua.h>
#include<luafly.h>
int lb_safeclear(lua_State* L) {
    try {
        LuaFly lf{ L };
        string pname;
        string iname;
        int aux, count;
        short ID;
        //pname itemname count aux
        lf.pops(aux, count, iname, pname);
        MyItemStack IT(iname, count, aux);
        ID = IT.get().getId();
        WPlayer wp = LocateS<WLevel>()->getPlayer(pname).val();
        bool success=removeItem(wp, IT, aux);
        lua_pushboolean(L, success);
        return 1;
    }CATCH()
}
int lb_giveItem(lua_State* L) {
    try {
        LuaFly lf{ L };
        string pname;
        //pname itemname count aux
        //pname itemname count aux lore
        string iname;
        int count, aux;
        vector<string> lore;
        if (lua_gettop(L) == 5) lf.pops(lore, aux, count, iname, pname);
        else lf.pops(aux, count, iname, pname);
        WPlayer wp = LocateS<WLevel>()->getPlayer(pname).val();
        MyItemStack It(iname,count,aux);
        if (lore.size()) It.get().setCustomLore(lore);
        addItem(wp, It);
        return 0;
    }CATCH()
}
int lb_dumpinv(lua_State* L) {
    try {
        LuaFly lf{ L };
        string pname;
        bool isEnder;
        lf.pops(isEnder, pname);
        if (isEnder) {
            lf.push(dumpEnderContainer(LocateS<WLevel>()->getPlayer(pname).val()));
        }
        else {
            lf.push(dumpContainer(LocateS<WLevel>()->getPlayer(pname).val()));
        }
        return 1;
    }CATCH()
}
int lb_cleanHand(lua_State* L) {
    try {
        LuaFly lf{ L };
        string pname;
        lf.pops(pname);
        WPlayer wp = LocateS<WLevel>()->getPlayer(pname).val();
        ForceRemoveItem(wp, *(MyItemStack*)&wp->getCarriedItem());
        return 0;
    }CATCH()
}
int lb_test(lua_State* L) {
    try {
        LuaFly lf{ L };
        string pname,fname;
        lf.pops(fname,pname);
        WPlayer wp = LocateS<WLevel>()->getPlayer(pname).val();
        string x=EdumpInventory_bin(wp);
        std::ofstream ofs(fname, std::ios::ate | std::ios::binary);
        ofs.write(x.data(), x.size());
        EclearInventory(wp);
        return 0;
    }CATCH()
}
int lb_test2(lua_State* L) {
    try {
        LuaFly lf{ L };
        string pname, fname;
        lf.pops(fname,pname);
        WPlayer wp = LocateS<WLevel>()->getPlayer(pname).val();
        std::ifstream ifs(fname, std::ios::binary);
        ErestoreInventory(wp, ifs2str(ifs));
        return 0;
    }CATCH()
}
int lb_getHand(lua_State* L) {
    try {
        LuaFly lf{ L };
        string pname;
        lf.pops(pname);
        WPlayer wp = LocateS<WLevel>()->getPlayer(pname).val();
        ItemStack& is = *(ItemStack*)&wp->getCarriedItem();
        lf.pushs(is.toString(), is.getAuxValue());
        return 2;
    }CATCH()
}

static const luaL_Reg R[] = {
         { "sClear", lb_safeclear	},
    { "giveItem", lb_giveItem	},
    { "dumpInv", lb_dumpinv	},
    { "clearHand", lb_cleanHand	},
    { "getHand", lb_getHand	},
    { "MoveEnderChestTo", lb_test	},
     { "LoadEnderChestFrom", lb_test2	},
    { NULL,		NULL	}
};
extern "C" {
    _declspec(dllexport) void onPostInit() {
        std::ios::sync_with_stdio(false);
        registerLuaLoadHook([] {
            lua_newtable(L);
            luaL_setfuncs(L, R, 0);
            lua_setglobal(L, "invapi");
        });
    }
}
