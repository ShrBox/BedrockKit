// 下列 ifdef 块是创建使从 DLL 导出更简单的
// 宏的标准方法。此 DLL 中的所有文件都是用命令行上定义的 INVENTORYAPI_EXPORTS
// 符号编译的。在使用此 DLL 的
// 任何项目上不应定义此符号。这样，源文件中包含此文件的任何其他项目都会将
// INVENTORYAPI_API 函数视为是从 DLL 导入的，而此 DLL 则将用此宏定义的
// 符号视为是被导出的。
#pragma once
#ifdef INVENTORYAPI_EXPORTS
#define INVENTORYAPI_API __declspec(dllexport)
#else
#define INVENTORYAPI_API __declspec(dllimport)
#endif
#include<string>
using std::string;
class ItemStack;
class ServerPlayer;
// 此类是从 dll 导出的
class INVENTORYAPI_API MyItemStack {
	unsigned long long filler[18];
public:
	MyItemStack();
	MyItemStack(string const& name,int count,int aux);
	~MyItemStack();
	MyItemStack(MyItemStack const&) = delete;
	MyItemStack(MyItemStack&&) = delete;
	MyItemStack& operator=(MyItemStack const&) = delete;
	ItemStack& get() {
		return *(ItemStack*)this;
	}
	operator ItemStack& () {
		return get();
	}
};
INVENTORYAPI_API string dumpContainer(ServerPlayer&);
INVENTORYAPI_API string dumpEnderContainer(ServerPlayer&);
INVENTORYAPI_API int getItemCount(ServerPlayer& sp, short id, short aux=-1);
INVENTORYAPI_API bool removeItem(ServerPlayer&, MyItemStack& ite, short aux=-1);
INVENTORYAPI_API void addItem(ServerPlayer&, MyItemStack&);
INVENTORYAPI_API void ForceRemoveItem(ServerPlayer& sp, MyItemStack& my);
INVENTORYAPI_API string ItemStackSerialize(ItemStack const&);
INVENTORYAPI_API std::unique_ptr<MyItemStack> ItemStackDeserialize(string const&);
INVENTORYAPI_API string EdumpInventory_bin(ServerPlayer& sp);
INVENTORYAPI_API void EclearInventory(ServerPlayer& sp);
INVENTORYAPI_API void ErestoreInventory(ServerPlayer& sp, string_view bin);
