#include "pch.h"
bool checkLandRange_stub(IVec2 vc, IVec2 vc2, int dim);
void InitExplodeProtect() {
	addListener([](LevelExplodeEvent& ev) {
		if (NO_EXPLOSION) {
			ev.exp.firing = false;
			ev.exp.breaking = false;
			return;
		}
		IVec2 vc{ *ev.exp.pos };
		IVec2 vc2 = vc;
		vc += -explosion_land_dist;
		vc2 += explosion_land_dist;
		auto dimid = ev.exp.bs.getDim().getID();
		if (checkLandRange_stub(vc, vc2, dimid)) {
			ev.exp.firing = false;
			ev.exp.breaking = false;
			return;
		}
		});
}