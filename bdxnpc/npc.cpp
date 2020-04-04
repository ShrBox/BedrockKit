#include"pch.h"
#include"Bossbar.h"
typedef unsigned long long eid_t;
static eid_t NPCID= 9223372036854775809ull;
enum NPCOP_LIST :int {
	LIST = 1
};
enum NPCOP_ADD :int {
	ADD=1
};
enum NPCOP_SETROT :int {
	SETROT = 1
};
enum NPCOP_REMOVE:int {
	REMOVE=1
};
string MINECRAFT_ENTITY_TYPE(string x) {
	if (x.find(':') != x.npos) return x;
	else return "minecraft:" + x;
}
struct NPC {
	string name;
	string nametag;
	string type;
	Vec3 pos;
	Vec3 rot;
	eid_t eid;
	string data;
	void pack(WBStream& bs)const{
		bs.apply(name, nametag, type, pos,rot, eid, data);
	}
	void unpack(RBStream& bs) {
		bs.apply(name, nametag, type, pos,rot, eid, data);
		type = MINECRAFT_ENTITY_TYPE(type);
	}
	void NetAdd(WBStream& bs)const{
#define SBIT(x) (1ull<<x)
		bs.apply(VarULong(eid), VarULong(eid), MCString(type), pos, Vec3{ 0,0,0 }
			, rot, //rotation
			VarUInt(0), //attr
			VarUInt(3), //metadata :3
			VarUInt(0),VarUInt(7),VarULong(SBIT(14)|SBIT(15)), //FLAGS:showtag
			VarUInt(80),VarUInt(0),(char)1, //always show tag
			VarUInt(4),VarUInt(4),MCString(nametag), //nametag
			VarUInt(0) //entity link
		);
		/*
		 stream.putUnsignedVarInt(map.size());
		for (int id : map.keySet()) {
			EntityData d = map.get(id);
			stream.putUnsignedVarInt(id);
			stream.putUnsignedVarInt(d.getType());
			switch (d.getType()) {
			case Entity.DATA_TYPE_BYTE:
					stream.putByte(((ByteEntityData) d).getData().byteValue());
					break;
				case Entity.DATA_TYPE_SHORT:
					stream.putLShort(((ShortEntityData) d).getData());
					break;
				case Entity.DATA_TYPE_INT:
					stream.putVarInt(((IntEntityData) d).getData());

						public static final int DATA_TYPE_BYTE = 0;
	public static final int DATA_TYPE_SHORT = 1;
	public static final int DATA_TYPE_INT = 2;
	public static final int DATA_TYPE_FLOAT = 3;
	public static final int DATA_TYPE_STRING = 4;
	public static final int DATA_TYPE_NBT = 5;
	public static final int DATA_TYPE_POS = 6;
	public static final int DATA_TYPE_LONG = 7;
	public static final int DATA_TYPE_VECTOR3F = 8;

		public static final int DATA_FLAGS = 0;//long
		 public static final int DATA_NAMETAG = 4; //string
		  public static final int DATA_ENTITY_AGE = 24; //short
		  public static final int DATA_ALWAYS_SHOW_NAMETAG = 80; // byte

		   public static final int DATA_FLAG_INLOVE = 7;
		   public static final int DATA_FLAG_POWERED = 9;
	public static final int DATA_FLAG_IGNITED = 10;
		  public static final int DATA_FLAG_ONFIRE = 0;
			  public static final int DATA_FLAG_CAN_SHOW_NAMETAG = 14;
	public static final int DATA_FLAG_ALWAYS_SHOW_NAMETAG = 15;
		
		*/
	}
	void NetRemove(WBStream& bs) const {
		bs.apply(VarULong(eid));
	}
};
std::unordered_map<eid_t, NPC> npcs;
bool NPCCMD_LIST(CommandOrigin const& ori, CommandOutput& outp, MyEnum<NPCOP_LIST>,optional<string>& name) {
	for (auto& i : npcs) {
		if (name.set) {
			if (i.second.name == name.value()) {
				outp.addMessage("NPC " + i.second.name + " tag " + i.second.nametag + " pos " + i.second.pos.toString());
				return true;
			}
		}
		else {
			outp.addMessage("NPC " + i.second.name + " tag " + i.second.nametag + " pos " + i.second.pos.toString());
		}
	}
	return true;
}
void broadcastPKT(Packet& pk) {
	auto usr=LocateS<WLevel>()->getUsers();
	for (auto i : usr) {
		i->sendNetworkPacket(pk);
	}
}
void broadcastNPCADD(eid_t nid) {
	WBStream ws;
	npcs[nid].NetAdd(ws);
	MyPkt<0xd> pk{ ws };
	broadcastPKT(pk);
}
void broadcastNPCREMOVE(eid_t nid) {
	WBStream ws;
	npcs[nid].NetRemove(ws);
	MyPkt<14> pk{ ws };
	broadcastPKT(pk);
}
void sendNPCADD(WPlayer ply,eid_t nid) {
	WBStream ws;
	npcs[nid].NetAdd(ws);
	MyPkt<0xd> pk{ ws };
	ply->sendNetworkPacket(pk);
}
bool NPCCMD_ADD(CommandOrigin const& ori, CommandOutput& outp, MyEnum<NPCOP_ADD>,string& name,string& type,string& tag,string& data) {
	NPC npc;
	npc.name = name;
	npc.nametag = tag;
	npc.type = MINECRAFT_ENTITY_TYPE(type);
	npc.data = data;
	npc.eid = NPCID++;
	npc.pos = ori.getWorldPosition();
	npc.rot = { 0,0,0 };//cant get rot :(
	db->put("NPCID", to_view(NPCID));
	WBStream ws;
	ws.apply(npc);
	db->put("npc_" + name,ws);
	npcs.emplace(NPCID-1,std::move(npc));
	broadcastNPCADD(NPCID - 1);
	outp.addMessage("added npc " + name);
	return true;
}
bool NPCCMD_REMOVE(CommandOrigin const& ori, CommandOutput& outp, MyEnum<NPCOP_REMOVE>,string& name) {
	for (auto it = npcs.begin(); it != npcs.end();++it) {
		if (it->second.name == name) {
			db->del("npc_" + name);
			auto del = it->first;
			broadcastNPCREMOVE(it->first);
			npcs.erase(del);
			outp.addMessage("deleted!");
			return true;
		}
	}
	return false;
}
bool NPCCMD_SETROT(CommandOrigin const& ori, CommandOutput& outp, MyEnum <NPCOP_SETROT>,string& name, float a0, float a1, float a2) {
	for (auto it = npcs.begin(); it != npcs.end();++it) {
		if (it->second.name == name) {
			NPC& np = it->second;
			np.rot = { a0,a1,a2 };
			WBStream ws;
			ws.apply(np);
			db->put("npc_" + it->second.name, ws);
			broadcastNPCREMOVE(np.eid);
			broadcastNPCADD(np.eid);
			return true;
		}
	}
	return false;
}
void load() {
	db->iter([](string_view key,string_view val)->bool {
		if (key._Starts_with("npc_")) {
			NPC np;
			RBStream rs(val);
			rs.apply(np);
			auto eid = np.eid;
			npcs.emplace(eid, std::move(np));
		}
		return true;
	});
}
void entry_npc() {
	string val;
	if (db->get("NPCID", val)) {
		memcpy(&NPCID, val.data(), 8);
	}
	load();
	addListener([](RegisterCommandEvent&) {
		MakeCommand("npc", "npc command", 1);
		CEnum<NPCOP_SETROT> _1("npc_setrot", { "setrot" });
		CEnum<NPCOP_ADD> _2("npc_add", {"add"});
		CEnum<NPCOP_LIST> _3("npc_list", { "list" });
		CEnum<NPCOP_REMOVE> _4("npc_remove", { "remove" });
		CmdOverload(npc, NPCCMD_ADD,"add","name","type","nametag","lua_fn");
		CmdOverload(npc, NPCCMD_LIST,"list","name");
		CmdOverload(npc, NPCCMD_REMOVE,"remove","name");
		CmdOverload(npc, NPCCMD_SETROT, "setrot","name","pitch", "from_yaw", "to_yaw");
	});
	addListener([](PlayerJoinEvent& ev) {
		for(auto& i:npcs)
			sendNPCADD(ev.getPlayer(),i.first);
		});
	addListener([](PlayerUseItemOnEntityEvent& ev) {
		auto rtid = ev.rtid;
		auto it = npcs.find(rtid);
		if (it != npcs.end()) {
			if (!call_lua(it->second.data.c_str(), { {&ev.getPlayer().getName() } }).set) {
				ev.getPlayer().sendText("error calling lua");
			}
		}
		});
}