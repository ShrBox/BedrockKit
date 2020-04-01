#include"pch.h"
#include<mcapi/Level.h>
bool onCMD_map(CommandOrigin const& ori, CommandOutput& outp, string& name) {
	auto wp = MakeWP(ori);
	auto& ite=wp.val()->getCarriedItem();
	//static auto map_vtable = SYM("??_7MapItem@@6B@");
	//printf("%p %p\n", map_vtable, dAccess<void*, 0>(&ite));
	ActorUniqueID id;
	SymCall("?getMapId@MapItem@@SA?AUActorUniqueID@@AEBV?$unique_ptr@VCompoundTag@@U?$default_delete@VCompoundTag@@@std@@@std@@@Z", void, ActorUniqueID*, uintptr_t)(
		&id,
		((uintptr_t)&ite)+16
		);
	auto mapd = LocateS<ServerLevel>()->getMapSavedData(id);
	printf("map %p\n", mapd);
	if (!mapd) { outp.error("not a filled map"); return false; }
	dAccess<bool, 0x62>(mapd) = true; //locked
	auto& pix = dAccess<std::vector<unsigned int>, 0x30>(mapd);
	printf("mid %I64u %zu\n", id.get(),pix.size());
	ifstream ifs(name + ".bin", std::ios::binary);
	auto str=ifs2str(ifs);
	printf("size %zd\n",str.size()/sizeof(unsigned int)/128);
	RBStream rs{ str };
	if (ifs.fail()) {
		outp.error("file open error");
		return false;
	}
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