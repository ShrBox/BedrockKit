#include"pch.h"
#if 1
THook(
	void,
	"?registerCommand@CommandRegistry@@QEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@"
	"PEBDW4CommandPermissionLevel@@UCommandFlag@@3@Z",
	uintptr_t a0, uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5) {
	original(a0, a1, a2, a3, a4, 0x40);
}
#endif
THook(
	void**,
	"?getEncryptedPeerForUser@NetworkHandler@@QEAA?AV?$weak_ptr@VEncryptedNetworkPeer@@@std@@AEBVNetworkIdentifier@@@Z",
	void* a, void** b, void* c) {
	b[0] = b[1] = NULL;
	return b;
}
