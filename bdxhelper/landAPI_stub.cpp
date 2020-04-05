#include"pch.h"
#include<bdxland.h>
#include"framework.h"
bool checkLandRange_stub(IVec2 vc, IVec2 vc2, int dim) {
	static bool inited = false;
	static decltype(&checkLandRange) ptr=nullptr;
	if (!inited) {
		auto handle = LoadLibraryA("bdxland.dll");
		if (handle) {
			ptr = (decltype(ptr))GetProcAddress(handle, "checkLandRange");
		}
		else {
			//?handle@ServerNetworkHandler@@UEAAXAEBVNetworkIdentifier@@AEBVCommandBlockUpdatePacket@@@Z
			printf("error1\n");
		}
		if (!ptr) {
			printf("[WProtect/WARNING] cant find bdxland,explosion_land_distance wont work!!!\n");
		}
	}
	if (ptr) return ptr(vc,vc2, dim); else return 0;
}