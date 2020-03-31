#include"pch.h"
#include<mcapi/Level.h>
#include<mcapi/Player.h>
THook(
	void,
	"?registerCommand@CommandRegistry@@QEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@"
	"PEBDW4CommandPermissionLevel@@UCommandFlag@@3@Z",
	uintptr_t a0, uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5) {
	original(a0, a1, a2, a3, a4, 0x40);
}
THook(
	void**,
	"?getEncryptedPeerForUser@NetworkHandler@@QEAA?AV?$weak_ptr@VEncryptedNetworkPeer@@@std@@AEBVNetworkIdentifier@@@Z",
	void* a, void** b, void* c) {
	b[0] = b[1] = NULL;
	return b;
}
#include<random>
bool onCMD_map(CommandOrigin const& ori, CommandOutput& outp, string& name) {
	auto wp = MakeWP(ori);
	auto& ite=wp.val()->getCarriedItem();
	ActorUniqueID id;
	SymCall("?getMapId@MapItem@@SA?AUActorUniqueID@@AEBV?$unique_ptr@VCompoundTag@@U?$default_delete@VCompoundTag@@@std@@@std@@@Z", void, ActorUniqueID*, uintptr_t)(
		&id,
		((uintptr_t)&ite)+16
		);
	auto mapd = LocateS<ServerLevel>()->getMapSavedData(id);
	printf("map %p\n", mapd);
	dAccess<bool, 0x62>(mapd) = true; //locked
	auto& pix = dAccess<std::vector<unsigned int>, 0x30>(mapd);
	printf("mid %I64u %zu\n", id.get(),pix.size());
	ifstream ifs(name + ".bin", std::ios::binary);
	auto str=ifs2str(ifs);
	printf("size %d\n",str.size()/sizeof(unsigned int)/128);
	RBStream rs{ str };
	if (ifs.fail()) {
		outp.error("file open error");
		return false;
	}
	std::random_device rd;
	for (int i = 0; i < 16384; ++i) {
		unsigned int val;
		rs.apply(val);
		//printf("%p\n", val);
		pix[i] = 0xff000000|val;
	}
	for (int i = 0; i < 128; ++i)
		for (int j = 0; j < 128; ++j)
			SymCall("?setPixelDirty@MapItemSavedData@@QEAAXII@Z",void,void*, unsigned int, unsigned int)(mapd,i, j);
	SymCall("?save@MapItemSavedData@@QEAAXAEAVLevelStorage@@@Z", void, void*, void*)(mapd, LocateS<LevelStorage>()._srv);
	return true;
}
void entry_map() {
	addListener([](RegisterCommandEvent&) {
		MakeCommand("map", "map maker", 1);
		CmdOverload(map, onCMD_map, "filename");
	});
}