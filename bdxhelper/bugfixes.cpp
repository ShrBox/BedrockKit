#include"pch.h"
extern bool NOFIXBDS_BONEMEAL_BUG;
#ifdef TRACING_ENABLED
THook(void*, "?tickWorld@ServerPlayer@@UEAAHAEBUTick@@@Z", void* a, void* b) {
	WatchDog dog("tickWorld@ServerPlayer");
	return original(a, b);
}
THook(void*, "?CompactRange@DBImpl@leveldb@@UEAAXPEBVSlice@2@0@Z", void* a, void* b, void* c) {
	WatchDog dog("DB_COMPACT");
	return original(a, b, c);
}
#endif
THook(void*, "?onFertilized@GrassBlock@@UEBA_NAEAVBlockSource@@AEBVBlockPos@@PEAVActor@@W4FertilizerType@@@Z", void* thi, void* a1, void* a2, void* a3_bds_bugs_here, void* a4) {
	if (a3_bds_bugs_here || NOFIXBDS_BONEMEAL_BUG) return original(thi, a1, a2, a3_bds_bugs_here, a4);
	return nullptr;
}
THook(void*, "?handle@ServerNetworkHandler@@UEAAXAEBVNetworkIdentifier@@AEBVCommandBlockUpdatePacket@@@Z", ServerNetworkHandler* thi, NetworkIdentifier& a2, unsigned char* pk) {
	auto sp = thi->_getServerPlayer(a2, pk[16]);
	if (sp) {
		WPlayer wp{ *sp };
		if (wp.getPermLvl() == 0) {
			LOG("CMDBLOCK CHEAT!!", wp.getName());
			return nullptr;
		}
		return original(thi, a2, pk);
	}
	return nullptr;
}
