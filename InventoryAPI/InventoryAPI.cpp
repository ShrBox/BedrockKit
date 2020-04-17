// InventoryAPI.cpp : 定义 DLL 的导出函数。
//

#include "pch.h"
#include "InventoryAPI.h"
class Item;
MyItemStack::MyItemStack(string const& name,int count,int aux) {
	struct WeakData {
		Item* x;
		int refcount;
	};
	WeakData* pWeakPtr;
	int unk;
	SymCall("?lookupByName@ItemRegistry@@SA?AV?$WeakPtr@VItem@@@@AEAHAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z", void, WeakData**, int&, string const&)(&pWeakPtr,unk,name);
	if (!pWeakPtr) throw std::exception("item not exists");
	SymCall("??0ItemStackBase@@IEAA@AEBVItem@@HH@Z", void, void*,Item*,int,int)(filler,pWeakPtr->x,count,aux);
	pWeakPtr->refcount--;
	filler[0]=(uintptr_t)SYM("??_7ItemStack@@6B@");
}
MyItemStack::~MyItemStack() {
	SymCall("??_EItemInstance@@UEAAPEAXI@Z", void, void*, int)(filler, 0);
}
struct FillingContainer {
	int size() {
		return SymCall("?getContainerSize@FillingContainer@@UEBAHXZ", int, FillingContainer*)(this);
	}
	ItemStack& get(int i) {
		return SymCall("?getItem@FillingContainer@@UEBAAEBVItemStack@@H@Z", ItemStack&, void*, int)(this, i);
	}
	void remove(ItemStack& it,bool matchAux){
		printf("x %d\n",WItem(it).getCount());
		return SymCall("?removeResource@FillingContainer@@QEAAHAEBVItemStack@@_N1H@Z",void,void*,ItemStack&,bool,bool,int)(this,it,matchAux,0,WItem(it).getCount());
	}
};
string dumpFillingContainer(FillingContainer* container) {
	string rv;
	int siz = container->size();
	for (int i = 0; i < siz; ++i) {
		ItemStack& x = container->get(i);
		rv.append(x.toString());
		if(i!=siz-1) rv.append(" | ");
	}
	return rv;
}
#include<debug\MemSearcher.h>
static MSearcherEx<FillingContainer*> getContainer_S2;
/*+168
??_7Inventory@@6B@
*/ 
static MSearcherEx<void*> getContainer_S1;
/*
+3528
??_7PlayerInventoryProxy@@6BContainerSizeChangeListener@@@
*/
FillingContainer* getContainer(ServerPlayer& sp) {
	if (getContainer_S1.myOff == 0) {
		getContainer_S1.init(&sp, [](void* x) {
			return MreadPtr_Compare((const void***)x, SYM("??_7PlayerInventoryProxy@@6BContainerSizeChangeListener@@@"));
		}, 3528, 256);
		void* x = *getContainer_S1.get(&sp);
		getContainer_S2.init(x, [](void* x) {
			return MreadPtr_Compare((const void***)x, SYM("??_7Inventory@@6B@"));
		}, 168);
	}
	return *getContainer_S2.get(*getContainer_S1.get(&sp));
}
static MSearcherEx<void> createSSideTrans_S1;
void createSSideTrans(ServerPlayer& sp, ItemStack& from, ItemStack& to) {
	if (createSSideTrans_S1.myOff == 0) {
		createSSideTrans_S1.init(&sp, [&](void* x) {
			return Mcompare_pVoid(x, &sp);
		}, 5024, 256);
	}
	SymCall("?_createServerSideAction@InventoryTransactionManager@@QEAAXAEBVItemStack@@0@Z", void, void*, ItemStack&, ItemStack&)(createSSideTrans_S1.get(&sp), from, to);
}
ItemStack& EMPTYISK() {
	return *(ItemStack*)SYM("?EMPTY_ITEM@ItemStack@@2V1@B");
}
INVENTORYAPI_API string dumpContainer(ServerPlayer& sp) {
	return dumpFillingContainer(getContainer(sp));
}
INVENTORYAPI_API string dumpEnderContainer(ServerPlayer& sp) {
	return dumpFillingContainer(SymCall("?getEnderChestContainer@Player@@QEAAPEAVEnderChestContainer@@XZ",FillingContainer*,ServerPlayer&)(sp));
}
INVENTORYAPI_API int getItemCount(ServerPlayer& sp, short id,short aux) {
	FillingContainer* f = getContainer(sp);
	int sz = f->size();
	int tot = 0;
	for (int i = 0; i < sz; ++i) {
		ItemStack& is = f->get(i);
		if (is.getId() == id && (aux == -1 || is.getAuxValue() == aux)) {
				tot += WItem{ is }.getCount();
		}
	}
	return tot;
}
void removeItem_impl(ServerPlayer& sp, MyItemStack& my,bool matchAux) {
	createSSideTrans(sp, my, EMPTYISK());
	getContainer(sp)->remove(my,matchAux);
	sp.sendInventory(false);
}
INVENTORYAPI_API bool removeItem(ServerPlayer& sp, MyItemStack& my,short aux) {
	if (getItemCount(sp, my.get().getId(), aux) < WItem{ my }.getCount()) return false;
	removeItem_impl(sp, my,aux!=-1);
	return true;
}
INVENTORYAPI_API void ForceRemoveItem(ServerPlayer& sp, MyItemStack& my) {
	removeItem_impl(sp, my, true);
}
INVENTORYAPI_API void addItem(ServerPlayer& sp, MyItemStack& it) {
	createSSideTrans(sp, EMPTYISK(),it);
	sp.add(it);
	sp.sendInventory(false);
}
#if 0
THook(void * ,"?removeResource@FillingContainer@@QEAAHAEBVItemStack@@_N1H@Z",void* thi,void* item,bool unk,bool unk1,int unk2){  //unk2 maxcount //unk1 1
	printf("%d %d %d\n",unk,unk1,unk2);
	//unk matchAux unk1 false unk2 count
	return original(thi, item, unk,unk1, unk2);
}
#endif