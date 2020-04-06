// 下列 ifdef 块是创建使从 DLL 导出更简单的
// 宏的标准方法。此 DLL 中的所有文件都是用命令行上定义的 BDXLUA_EXPORTS
// 符号编译的。在使用此 DLL 的
// 任何项目上不应定义此符号。这样，源文件中包含此文件的任何其他项目都会将
// BDXLUA_API 函数视为是从 DLL 导入的，而此 DLL 则将用此宏定义的
// 符号视为是被导出的。
#pragma once
#ifdef BDXLUA_EXPORTS
#define BDXLUA_API __declspec(dllexport)
#else
#define BDXLUA_API __declspec(dllimport)
#endif

#include<variant>
#include<vector>
#include<unordered_map>
#include<stl\optional.h>
#include<stl\static_queue.h>
using std::variant,std::vector,std::unordered_map;
typedef variant<long long, string> ldata_t;
typedef ldata_t(*luacb_t)(vector<ldata_t>& arg);
BDXLUA_API extern unordered_map<string, luacb_t> bindings;
struct ldata_ref_t {
	union {
		long long lv;
		string const* sp;
	} d;
	bool isLong;
	ldata_ref_t(long long x) {
		d.lv = x;
		isLong = true;
	}
	ldata_ref_t(string const* x) {
		d.sp = x;
		isLong = false;
	}
	string const& asStr() const{
		return *d.sp;
	}
	long long asLL() const {
		return d.lv;
	}
};
BDXLUA_API optional<long long> call_lua(const char* name, static_queue<ldata_ref_t,8> const& arg);
extern BDXLUA_API lua_State* L;
BDXLUA_API void registerLuaLoadHook(void(*)());
#ifdef BDXLUA_EXPORTS
int lua_bind_GUI(lua_State* L);
#endif


