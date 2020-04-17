#include"pch.h"
bool NOGREYTEXT;
#if 0
static bool inner = false;
THook(bool, "?getBool@GameRules@@QEBA_NUGameRuleId@@@Z", void* a0, int* a1) {
	if (!inner) return original(a0, a1);
	return *a1 == 0;
}
#endif
THook(void, "?sendToAdmins@CommandOutputSender@@QEAAXAEBVCommandOrigin@@AEBVCommandOutput@@W4CommandPermissionLevel@@@Z", void* a0, void* a1, void* a2, void* a3) {
	/*inner = true;
	auto rv = original(a0, a1, a2, a3);
	inner = false;
	return rv;*/
	if (!NOGREYTEXT) return original(a0, a1, a2, a3);
}
	