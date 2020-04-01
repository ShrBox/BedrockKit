// 下列 ifdef 块是创建使从 DLL 导出更简单的
// 宏的标准方法。此 DLL 中的所有文件都是用命令行上定义的 BDXLAND_EXPORTS
// 符号编译的。在使用此 DLL 的
// 任何项目上不应定义此符号。这样，源文件中包含此文件的任何其他项目都会将
// BDXLAND_API 函数视为是从 DLL 导入的，而此 DLL 则将用此宏定义的
// 符号视为是被导出的。
#ifdef BDXLAND_EXPORTS
#define BDXLAND_API __declspec(dllexport)
#else
#define BDXLAND_API __declspec(dllimport)
#endif
BDXLAND_API unsigned int getLandIDAt(int x, int z, int dim);
BDXLAND_API unsigned int checkLandRange(int x, int z, int dx, int dz, int dim);
BDXLAND_API bool checkLandOwnerRange(int x, int z, int dx, int dz, int dim, unsigned long long xuid);
#define iround(x) int(round(x))