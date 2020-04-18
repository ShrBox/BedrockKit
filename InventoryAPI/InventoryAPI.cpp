// InventoryAPI.cpp : 定义 DLL 的导出函数。
//

#include "pch.h"
#include "InventoryAPI.h"
class Item;
MyItemStack::MyItemStack() {
	SymCall("??0ItemStack@@QEAA@XZ", void, void*)(this);
}
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
		return SymCall("?removeResource@FillingContainer@@QEAAHAEBVItemStack@@_N1H@Z",void,void*,ItemStack&,bool,bool,int)(this,it,matchAux,0,WItem(it).getCount());
	}
	void set(int i, ItemStack const& x) {
		SymCall("?setItem@FillingContainer@@UEAAXHAEBVItemStack@@@Z", void, void*, int, ItemStack const&)(this, i, x);
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
struct CompoundTag {
	unsigned long long filler[3];
	CompoundTag() {
		SymCall("??0CompoundTag@@QEAA@XZ", void, void*)(this);
	}
	~CompoundTag() {
		SymCall("??1CompoundTag@@UEAA@XZ", void, void*)(this);
	}
	struct StrOutp {
		void* vtbl = SYM("??_7StringByteOutput@@6B@");
		string* payload;
		StrOutp(string& x) {
			payload = &x;
		}
	};
	struct StrInp {
		void* vtbl = SYM("??_7StringByteInput@@6B@");
		u64 posnow;
		u64 total;
		const char* strBase;
		StrInp(string_view v) {
			posnow = 0;
			total = v.size();
			strBase = v.data();
		}
	};
	string serialize() const {
		string o;
		StrOutp ou(o);
		SymCall("?write@CompoundTag@@UEBAXAEAVIDataOutput@@@Z", void, void const*, StrOutp&)(this, ou);
		return o;
	}
	void deserialize(string const& p) {
		StrInp in(p);
		SymCall("?load@CompoundTag@@UEAAXAEAVIDataInput@@@Z", void, void*, StrInp&)(this, in);
	}
};
struct ItemWrapper {
	class std::unique_ptr<class CompoundTag, struct std::default_delete<class CompoundTag> >  save() const {
		class std::unique_ptr<class CompoundTag, struct std::default_delete<class CompoundTag> >(ItemWrapper:: * rv)()const; *((void**)&rv) = dlsym("?save@ItemStackBase@@QEBA?AV?$unique_ptr@VCompoundTag@@U?$default_delete@VCompoundTag@@@std@@@std@@XZ"); return (this->*rv)();
	}
	static std::unique_ptr<MyItemStack> fromTag(class CompoundTag const& a0) {
		MyItemStack* ms=new MyItemStack("dirt", 1, 0);
		((void(*)(MyItemStack*,class CompoundTag const&))dlsym("?fromTag@ItemStack@@SA?AV1@AEBVCompoundTag@@@Z"))(ms,a0);
		return std::unique_ptr<MyItemStack>(ms);
	}
};
INVENTORYAPI_API string ItemStackSerialize(ItemStack const& x) {
	auto tag=((ItemWrapper*)&x)->save();
	return tag->serialize();
}
INVENTORYAPI_API std::unique_ptr<MyItemStack> ItemStackDeserialize(string const& x) {
	CompoundTag tag;
	tag.deserialize(x);
	return ItemWrapper::fromTag(tag);
}
#include<stl\Bstream.h>
INVENTORYAPI_API string EdumpInventory_bin(ServerPlayer& sp) {
	FillingContainer* c = (FillingContainer*)sp.getEnderChestContainer();
	std::vector<string> pack;
	if (c) {
		int s = c->size();
		pack.reserve(s);
		for (int i = 0; i < s; ++i) {
			auto& item = c->get(i);
			pack.emplace_back(ItemStackSerialize(item));
		}
	}
	WBStream ws;
	ws.apply(pack);
	return ws.data;
}
INVENTORYAPI_API void EclearInventory(ServerPlayer& sp) {
	FillingContainer* c = (FillingContainer*)sp.getEnderChestContainer();
	if (c) {
		int s = c->size();
		for (int i = 0; i < s; ++i) {
			c->set(i, EMPTYISK());
		}
	}
}
INVENTORYAPI_API void ErestoreInventory(ServerPlayer& sp, string_view bin) {
	FillingContainer* c = (FillingContainer*)sp.getEnderChestContainer();
	if (c) {
		std::vector<string> pack;
		RBStream rs(bin);
		rs.apply(pack);
		for (int i = 0; i < pack.size(); ++i) {
			auto ite = ItemStackDeserialize(pack[i]);
			c->set(i, ite->get());
		}
	}
}
#if 0
THook(void * ,"?removeResource@FillingContainer@@QEAAHAEBVItemStack@@_N1H@Z",void* thi,void* item,bool unk,bool unk1,int unk2){  //unk2 maxcount //unk1 1
	printf("%d %d %d\n",unk,unk1,unk2);
	//unk matchAux unk1 false unk2 count
	return original(thi, item, unk,unk1, unk2);
}
#endif