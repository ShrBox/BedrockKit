// 下列 ifdef 块是创建使从 DLL 导出更简单的
// 宏的标准方法。此 DLL 中的所有文件都是用命令行上定义的 BDXLUA_EXPORTS
// 符号编译的。在使用此 DLL 的
// 任何项目上不应定义此符号。这样，源文件中包含此文件的任何其他项目都会将
// BDXLUA_API 函数视为是从 DLL 导入的，而此 DLL 则将用此宏定义的
// 符号视为是被导出的。
#ifdef BDXLUA_EXPORTS
#define BDXLUA_API __declspec(dllexport)
#else
#define BDXLUA_API __declspec(dllimport)
#endif
#include<variant>
#include<vector>
#include<unordered_map>
#include<stl\optional.h>
using std::variant,std::vector,std::unordered_map;
typedef variant<long long, string> ldata_t;
typedef ldata_t(*luacb_t)(vector<ldata_t>& arg);
BDXLUA_API extern unordered_map<string, luacb_t> bindings;
BDXLUA_API optional<ldata_t> call_lua(const char* name, vector<ldata_t> const& arg);
#ifdef BDXLUA_EXPORTS
bool _lpcall(int in, int out, const char* name);
extern lua_State* L;
int lua_bind_GUI(lua_State* L);
#endif


