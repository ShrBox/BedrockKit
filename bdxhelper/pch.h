// pch.h: 这是预编译标头文件。
// 下方列出的文件仅编译一次，提高了将来生成的生成性能。
// 这还将影响 IntelliSense 性能，包括代码完成和许多代码浏览功能。
// 但是，如果此处列出的文件中的任何一个在生成之间有更新，它们全部都将被重新编译。
// 请勿在此处添加要频繁更新的文件，这将使得性能优势无效。

#ifndef PCH_H
#define PCH_H

// 添加要在此处预编译的标头
#include<lbpch.h>
#include<stl\langPack.h>
#include<api\command\commands.h>
#include<stl\Logger.h>
#include<JsonLoader.h>
#include<api\gui\gui.h>
#include<mcapi\Item.h>
using std::unordered_map;
#include<stl\KVDB.h>
#include<api\myPacket.h>
#include<mcapi/Player.h>
#include<stl\varint.h>
#include<api\xuidreg\xuidreg.h>
#include<mcapi\Certificate.h>
#include<api\types\helper.h>
//#include<api\scheduler\scheduler.h>
//#include<mcapi\VanillaBlocks.h>
//#include<mcapi\BlockSource.h>
#include<api\scheduler\scheduler.h>
#include<bdxlua.h>
#include<unordered_set>
#include<bdxland.h>
    extern Logger<stacked<stdio_commit, file_commit>> LOG;
    extern bool EXP_PLAY;
    extern int explosion_land_dist;
    extern bool NO_EXPLOSION;
#endif //PCH_H
