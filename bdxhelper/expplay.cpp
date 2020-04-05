#include "pch.h"
/*from codehz/element-0*/
THook(
	void,
	"?setStack@ResourcePackManager@@QEAAXV?$unique_ptr@VResourcePackStack@@U?$default_delete@VResourcePackStack@@@std@@"
	"@std@@W4ResourcePackStackType@@_N@Z", void* thi,
	void* ptr, int type, bool flag) {
	if (EXP_PLAY)
		((char*)thi)[227] = EXP_PLAY;
	original(thi, ptr, type, flag);
}

THook(void*, "??0MinecraftEventing@@QEAA@AEBVPath@Core@@@Z", void* a, void* b) {
	// HACK FOR LevelSettings
	char* access = (char*)b + 2800;
	access[76] = EXP_PLAY;
	//access[36] = settings.education_feature;
	return original(a, b);
}

THook(bool, "?isEnabled@FeatureToggles@@QEBA_NW4FeatureOptionID@@@Z", void* thi, int id) {
	if (EXP_PLAY) return true;
	return original(thi, id);
}
THook(void, "??0LevelSettings@@QEAA@AEBV0@@Z", char* lhs, char* rhs) {
	rhs[76] = EXP_PLAY;
	//rhs[36] = settings.education_feature;
	original(lhs, rhs);
}