// Esquema congelado de las partidas planas (formato 1: hasta la v0.4.3), para seguir leyendolas
// aunque los structs cambien. Volcado una sola vez con sf_dump_c desde el esquema de la v0.4.4,
// identico al de la v0.4.3 (Linux x86-64; las plataformas del juego tienen la misma disposicion:
// el encabezado de cada partida plana trae la huella de los tamaños y se compara). No se regenera:
// describe archivos viejos, no los structs de hoy (docs/PARTIDAS.md).

static const SfField V1_F_Rng[] = {
    { "state", "uint32_t", SF_UINT, 0, 4, 1, NULL },
};
static const SfType V1_T_Rng = { "Rng", 4, 1, V1_F_Rng };

static const SfField V1_F_Player[] = {
    { "pos", "Vector3", SF_BYTES, 0, 12, 1, NULL },
    { "yaw", "float", SF_FLOAT, 12, 4, 1, NULL },
    { "vy", "float", SF_FLOAT, 16, 4, 1, NULL },
    { "grounded", "bool", SF_BOOL, 20, 1, 1, NULL },
    { "sneaking", "bool", SF_BOOL, 21, 1, 1, NULL },
    { "moving", "bool", SF_BOOL, 22, 1, 1, NULL },
    { "stance", "Stance", SF_INT, 24, 4, 1, NULL },
    { "noise", "float", SF_FLOAT, 28, 4, 1, NULL },
    { "speed_scale", "float", SF_FLOAT, 32, 4, 1, NULL },
    { "draw_lift", "float", SF_FLOAT, 36, 4, 1, NULL },
};
static const SfType V1_T_Player = { "Player", 40, 10, V1_F_Player };

static const SfField V1_F_Kingdom[] = {
    { "name", "char", SF_TEXT, 0, 1, 32, NULL },
    { "relation", "float", SF_FLOAT, 32, 4, 1, NULL },
    { "stance", "float", SF_FLOAT, 36, 4, 7, NULL },
};
static const SfType V1_T_Kingdom = { "Kingdom", 64, 3, V1_F_Kingdom };

static const SfField V1_F_Wound[] = {
    { "kind", "WoundKind", SF_INT, 0, 4, 1, NULL },
    { "part", "BodyPart", SF_INT, 4, 4, 1, NULL },
    { "severity", "float", SF_FLOAT, 8, 4, 1, NULL },
    { "bleeding", "bool", SF_BOOL, 12, 1, 1, NULL },
    { "treated", "bool", SF_BOOL, 13, 1, 1, NULL },
};
static const SfType V1_T_Wound = { "Wound", 16, 5, V1_F_Wound };

static const SfField V1_F_Health[] = {
    { "hp", "float", SF_FLOAT, 0, 4, 1, NULL },
    { "hp_max", "float", SF_FLOAT, 4, 4, 1, NULL },
    { "blood", "float", SF_FLOAT, 8, 4, 1, NULL },
    { "wounds", "Wound", SF_STRUCT, 12, 16, 8, &V1_T_Wound },
    { "wound_count", "int", SF_INT, 140, 4, 1, NULL },
    { "down", "bool", SF_BOOL, 144, 1, 1, NULL },
    { "dead", "bool", SF_BOOL, 145, 1, 1, NULL },
    { "beast", "bool", SF_BOOL, 146, 1, 1, NULL },
    { "venom", "float", SF_FLOAT, 148, 4, 1, NULL },
};
static const SfType V1_T_Health = { "Health", 152, 9, V1_F_Health };

static const SfField V1_F_ArmorPiece[] = {
    { "id", "char", SF_TEXT, 0, 1, 72, NULL },
    { "material", "ArmorMaterial", SF_INT, 72, 4, 1, NULL },
    { "zones", "unsigned", SF_UINT, 76, 4, 1, NULL },
    { "coverage", "float", SF_FLOAT, 80, 4, 1, NULL },
    { "mail", "bool", SF_BOOL, 84, 1, 1, NULL },
    { "durability", "float", SF_FLOAT, 88, 4, 1, NULL },
    { "durability_max", "float", SF_FLOAT, 92, 4, 1, NULL },
};
static const SfType V1_T_ArmorPiece = { "ArmorPiece", 96, 7, V1_F_ArmorPiece };

static const SfField V1_F_Armor[] = {
    { "slot", "ArmorPiece", SF_STRUCT, 0, 96, 9, &V1_T_ArmorPiece },
};
static const SfType V1_T_Armor = { "Armor", 864, 1, V1_F_Armor };

static const SfField V1_F_BagSlot[] = {
    { "id", "char", SF_TEXT, 0, 1, 72, NULL },
    { "count", "int", SF_INT, 72, 4, 1, NULL },
    { "condition", "float", SF_FLOAT, 76, 4, 1, NULL },
    { "var", "unsigned short", SF_UINT, 80, 2, 1, NULL },
};
static const SfType V1_T_BagSlot = { "BagSlot", 84, 4, V1_F_BagSlot };

static const SfField V1_F_Bag[] = {
    { "kind", "BagKind", SF_INT, 0, 4, 1, NULL },
    { "cap_kg", "float", SF_FLOAT, 4, 4, 1, NULL },
    { "max_size", "float", SF_FLOAT, 8, 4, 1, NULL },
    { "slots", "int", SF_INT, 12, 4, 1, NULL },
    { "s", "BagSlot", SF_STRUCT, 16, 84, 32, &V1_T_BagSlot },
    { "n", "int", SF_INT, 2704, 4, 1, NULL },
};
static const SfType V1_T_Bag = { "Bag", 2708, 6, V1_F_Bag };

static const SfField V1_F_Outfit[] = {
    { "g", "signed char", SF_INT, 0, 1, 5, NULL },
};
static const SfType V1_T_Outfit = { "Outfit", 5, 1, V1_F_Outfit };

static const SfField V1_F_Member[] = {
    { "id", "int", SF_INT, 0, 4, 1, NULL },
    { "name", "char", SF_TEXT, 4, 1, 32, NULL },
    { "role", "Role", SF_INT, 36, 4, 1, NULL },
    { "status", "MemberStatus", SF_INT, 40, 4, 1, NULL },
    { "traits", "unsigned", SF_UINT, 44, 4, 1, NULL },
    { "morale", "float", SF_FLOAT, 48, 4, 1, NULL },
    { "loyalty", "float", SF_FLOAT, 52, 4, 1, NULL },
    { "champion", "int", SF_INT, 56, 4, 1, NULL },
    { "health", "Health", SF_STRUCT, 60, 152, 1, &V1_T_Health },
    { "armor", "Armor", SF_STRUCT, 212, 864, 1, &V1_T_Armor },
    { "outfitted", "bool", SF_BOOL, 1076, 1, 1, NULL },
    { "camp", "int", SF_INT, 1080, 4, 1, NULL },
    { "pack", "int", SF_INT, 1084, 4, 1, NULL },
    { "bag", "Bag", SF_STRUCT, 1088, 2708, 1, &V1_T_Bag },
    { "known", "uint64_t", SF_UINT, 3800, 8, 1, NULL },
    { "journey", "int", SF_INT, 3808, 4, 1, NULL },
    { "outfit", "Outfit", SF_STRUCT, 3812, 5, 1, &V1_T_Outfit },
};
static const SfType V1_T_Member = { "Member", 3824, 17, V1_F_Member };

static const SfField V1_F_CombatStats[] = {
    { "size", "float", SF_FLOAT, 0, 4, 1, NULL },
    { "speed", "float", SF_FLOAT, 4, 4, 1, NULL },
    { "strength", "float", SF_FLOAT, 8, 4, 1, NULL },
    { "endurance", "float", SF_FLOAT, 12, 4, 1, NULL },
    { "aim", "float", SF_FLOAT, 16, 4, 1, NULL },
    { "riding", "float", SF_FLOAT, 20, 4, 1, NULL },
    { "healing", "float", SF_FLOAT, 24, 4, 1, NULL },
};
static const SfType V1_T_CombatStats = { "CombatStats", 28, 7, V1_F_CombatStats };

static const SfField V1_F_Champion[] = {
    { "name", "char", SF_TEXT, 0, 1, 32, NULL },
    { "epithet", "char", SF_TEXT, 32, 1, 32, NULL },
    { "origin", "int", SF_INT, 64, 4, 1, NULL },
    { "exile", "int", SF_INT, 68, 4, 1, NULL },
    { "livelihood", "int", SF_INT, 72, 4, 1, NULL },
    { "aspiration", "int", SF_INT, 76, 4, 1, NULL },
    { "gifts", "unsigned", SF_UINT, 80, 4, 1, NULL },
    { "weapon", "int", SF_INT, 84, 4, 1, NULL },
    { "stats", "CombatStats", SF_STRUCT, 88, 28, 1, &V1_T_CombatStats },
    { "traits", "unsigned", SF_UINT, 116, 4, 1, NULL },
};
static const SfType V1_T_Champion = { "Champion", 120, 10, V1_F_Champion };

static const SfField V1_F_Troop[] = {
    { "members", "Member", SF_STRUCT, 0, 3824, 64, &V1_T_Member },
    { "count", "int", SF_INT, 244736, 4, 1, NULL },
    { "next_id", "int", SF_INT, 244740, 4, 1, NULL },
    { "champions", "Champion", SF_STRUCT, 244752, 120, 64, &V1_T_Champion },
    { "champion_count", "int", SF_INT, 252432, 4, 1, NULL },
    { "rebellion_scale", "float", SF_FLOAT, 252436, 4, 1, NULL },
};
static const SfType V1_T_Troop = { "Troop", 252440, 6, V1_F_Troop };

static const SfField V1_F_Held[] = {
    { "id", "char", SF_TEXT, 0, 1, 72, NULL },
    { "kind", "InvHands", SF_INT, 72, 4, 1, NULL },
};
static const SfType V1_T_Held = { "Held", 76, 2, V1_F_Held };

static const SfField V1_F_Hands[] = {
    { "right", "Held", SF_STRUCT, 0, 76, 1, &V1_T_Held },
    { "left", "Held", SF_STRUCT, 76, 76, 1, &V1_T_Held },
    { "sheathed", "bool", SF_BOOL, 152, 1, 1, NULL },
    { "carried", "char", SF_TEXT, 153, 1, 72, NULL },
};
static const SfType V1_T_Hands = { "Hands", 228, 4, V1_F_Hands };

static const SfField V1_F_StockEntry[] = {
    { "id", "char", SF_TEXT, 0, 1, 72, NULL },
    { "count", "int", SF_INT, 72, 4, 1, NULL },
};
static const SfType V1_T_StockEntry = { "StockEntry", 76, 2, V1_F_StockEntry };

static const SfField V1_F_Stockpile[] = {
    { "e", "StockEntry", SF_STRUCT, 0, 76, 64, &V1_T_StockEntry },
    { "n", "int", SF_INT, 4864, 4, 1, NULL },
};
static const SfType V1_T_Stockpile = { "Stockpile", 4868, 2, V1_F_Stockpile };

static const SfField V1_F_CampTask[] = {
    { "kind", "CampTaskKind", SF_INT, 0, 4, 1, NULL },
    { "member", "int", SF_INT, 4, 4, 1, NULL },
    { "role", "Role", SF_INT, 8, 4, 1, NULL },
    { "work", "float", SF_FLOAT, 12, 4, 1, NULL },
    { "done", "float", SF_FLOAT, 16, 4, 1, NULL },
};
static const SfType V1_T_CampTask = { "CampTask", 20, 5, V1_F_CampTask };

static const SfField V1_F_CampSite[] = {
    { "used", "bool", SF_BOOL, 0, 1, 1, NULL },
    { "name", "char", SF_TEXT, 1, 1, 32, NULL },
    { "x", "float", SF_FLOAT, 36, 4, 1, NULL },
    { "z", "float", SF_FLOAT, 40, 4, 1, NULL },
    { "guardian", "int", SF_INT, 44, 4, 1, NULL },
    { "founded_day", "int", SF_INT, 48, 4, 1, NULL },
    { "stock", "Stockpile", SF_STRUCT, 52, 4868, 1, &V1_T_Stockpile },
    { "task", "CampTask", SF_STRUCT, 4920, 20, 6, &V1_T_CampTask },
    { "ntask", "int", SF_INT, 5040, 4, 1, NULL },
};
static const SfType V1_T_CampSite = { "CampSite", 5044, 9, V1_F_CampSite };

static const SfField V1_F_BuildProject[] = {
    { "def", "BuildId", SF_INT, 0, 4, 1, NULL },
    { "x", "float", SF_FLOAT, 4, 4, 1, NULL },
    { "z", "float", SF_FLOAT, 8, 4, 1, NULL },
    { "progress", "float", SF_FLOAT, 12, 4, 1, NULL },
    { "done", "bool", SF_BOOL, 16, 1, 1, NULL },
};
static const SfType V1_T_BuildProject = { "BuildProject", 20, 5, V1_F_BuildProject };

static const SfField V1_F_Npc[] = {
    { "member_id", "int", SF_INT, 0, 4, 1, NULL },
    { "pos", "Vector3", SF_BYTES, 4, 12, 1, NULL },
    { "home", "Vector3", SF_BYTES, 16, 12, 1, NULL },
    { "yaw", "float", SF_FLOAT, 28, 4, 1, NULL },
    { "project", "int", SF_INT, 32, 4, 1, NULL },
    { "job", "int", SF_INT, 36, 4, 1, NULL },
    { "moving", "bool", SF_BOOL, 40, 1, 1, NULL },
    { "job_pos", "Vector3", SF_BYTES, 44, 12, 1, NULL },
    { "job_timer", "float", SF_FLOAT, 56, 4, 1, NULL },
    { "escort", "bool", SF_BOOL, 60, 1, 1, NULL },
    { "fighting", "bool", SF_BOOL, 61, 1, 1, NULL },
    { "fight_anim", "float", SF_FLOAT, 64, 4, 1, NULL },
    { "hurt_anim", "float", SF_FLOAT, 68, 4, 1, NULL },
    { "stagger", "float", SF_FLOAT, 72, 4, 1, NULL },
    { "knock", "float", SF_FLOAT, 76, 4, 1, NULL },
    { "move", "int", SF_INT, 80, 4, 1, NULL },
    { "combo", "int", SF_INT, 84, 4, 1, NULL },
    { "move_anim", "float", SF_FLOAT, 88, 4, 1, NULL },
    { "home_camp", "int", SF_INT, 92, 4, 1, NULL },
    { "leave_t", "float", SF_FLOAT, 96, 4, 1, NULL },
    { "leave_dir", "Vector3", SF_BYTES, 100, 12, 1, NULL },
    { "comfort", "signed char", SF_INT, 112, 1, 1, NULL },
};
static const SfType V1_T_Npc = { "Npc", 116, 22, V1_F_Npc };

static const SfField V1_F_Animal[] = {
    { "used", "bool", SF_BOOL, 0, 1, 1, NULL },
    { "species", "Species", SF_INT, 4, 4, 1, NULL },
    { "state", "AnimalState", SF_INT, 8, 4, 1, NULL },
    { "mode", "AnimalMode", SF_INT, 12, 4, 1, NULL },
    { "x", "float", SF_FLOAT, 16, 4, 1, NULL },
    { "z", "float", SF_FLOAT, 20, 4, 1, NULL },
    { "yaw", "float", SF_FLOAT, 24, 4, 1, NULL },
    { "alt", "float", SF_FLOAT, 28, 4, 1, NULL },
    { "home_x", "float", SF_FLOAT, 32, 4, 1, NULL },
    { "home_z", "float", SF_FLOAT, 36, 4, 1, NULL },
    { "tx", "float", SF_FLOAT, 40, 4, 1, NULL },
    { "tz", "float", SF_FLOAT, 44, 4, 1, NULL },
    { "flee_x", "float", SF_FLOAT, 48, 4, 1, NULL },
    { "flee_z", "float", SF_FLOAT, 52, 4, 1, NULL },
    { "timer", "float", SF_FLOAT, 56, 4, 1, NULL },
    { "fleeing", "bool", SF_BOOL, 60, 1, 1, NULL },
    { "ridden", "bool", SF_BOOL, 61, 1, 1, NULL },
    { "speed", "float", SF_FLOAT, 64, 4, 1, NULL },
    { "h", "Health", SF_STRUCT, 68, 152, 1, &V1_T_Health },
    { "group", "int", SF_INT, 220, 4, 1, NULL },
    { "tkind", "TargetKind", SF_INT, 224, 4, 1, NULL },
    { "target", "int", SF_INT, 228, 4, 1, NULL },
    { "cooldown", "float", SF_FLOAT, 232, 4, 1, NULL },
    { "attack_anim", "float", SF_FLOAT, 236, 4, 1, NULL },
    { "hit_anim", "float", SF_FLOAT, 240, 4, 1, NULL },
    { "alert", "float", SF_FLOAT, 244, 4, 1, NULL },
    { "fear_humans", "float", SF_FLOAT, 248, 4, 1, NULL },
    { "hunger", "float", SF_FLOAT, 252, 4, 1, NULL },
    { "thirst", "float", SF_FLOAT, 256, 4, 1, NULL },
    { "stamina", "float", SF_FLOAT, 260, 4, 1, NULL },
    { "rival_timer", "float", SF_FLOAT, 264, 4, 1, NULL },
    { "clock", "float", SF_FLOAT, 268, 4, 1, NULL },
    { "bound_timer", "float", SF_FLOAT, 272, 4, 1, NULL },
    { "corpse", "float", SF_FLOAT, 276, 4, 1, NULL },
    { "eaten", "float", SF_FLOAT, 280, 4, 1, NULL },
    { "butchered", "bool", SF_BOOL, 284, 1, 1, NULL },
    { "milked", "bool", SF_BOOL, 285, 1, 1, NULL },
    { "weakened", "bool", SF_BOOL, 286, 1, 1, NULL },
    { "killed_by_human", "bool", SF_BOOL, 287, 1, 1, NULL },
};
static const SfType V1_T_Animal = { "Animal", 288, 39, V1_F_Animal };

static const SfField V1_F_SwarmBody[] = {
    { "x", "float", SF_FLOAT, 0, 4, 1, NULL },
    { "y", "float", SF_FLOAT, 4, 4, 1, NULL },
    { "z", "float", SF_FLOAT, 8, 4, 1, NULL },
    { "vx", "float", SF_FLOAT, 12, 4, 1, NULL },
    { "vy", "float", SF_FLOAT, 16, 4, 1, NULL },
    { "vz", "float", SF_FLOAT, 20, 4, 1, NULL },
    { "alive", "bool", SF_BOOL, 24, 1, 1, NULL },
};
static const SfType V1_T_SwarmBody = { "SwarmBody", 28, 7, V1_F_SwarmBody };

static const SfField V1_F_Swarm[] = {
    { "used", "bool", SF_BOOL, 0, 1, 1, NULL },
    { "kind", "SwarmKind", SF_INT, 4, 4, 1, NULL },
    { "hx", "float", SF_FLOAT, 8, 4, 1, NULL },
    { "hy", "float", SF_FLOAT, 12, 4, 1, NULL },
    { "hz", "float", SF_FLOAT, 16, 4, 1, NULL },
    { "cx", "float", SF_FLOAT, 20, 4, 1, NULL },
    { "cy", "float", SF_FLOAT, 24, 4, 1, NULL },
    { "cz", "float", SF_FLOAT, 28, 4, 1, NULL },
    { "tx", "float", SF_FLOAT, 32, 4, 1, NULL },
    { "tz", "float", SF_FLOAT, 36, 4, 1, NULL },
    { "timer", "float", SF_FLOAT, 40, 4, 1, NULL },
    { "anger", "float", SF_FLOAT, 44, 4, 1, NULL },
    { "calm", "float", SF_FLOAT, 48, 4, 1, NULL },
    { "cooldown", "float", SF_FLOAT, 52, 4, 1, NULL },
    { "active", "bool", SF_BOOL, 56, 1, 1, NULL },
    { "honey_taken", "bool", SF_BOOL, 57, 1, 1, NULL },
    { "n", "int", SF_INT, 60, 4, 1, NULL },
    { "b", "SwarmBody", SF_STRUCT, 64, 28, 14, &V1_T_SwarmBody },
    { "cell_x", "int", SF_INT, 456, 4, 1, NULL },
    { "cell_z", "int", SF_INT, 460, 4, 1, NULL },
};
static const SfType V1_T_Swarm = { "Swarm", 464, 20, V1_F_Swarm };

static const SfField V1_F_Progress[] = {
    { "level", "int", SF_INT, 0, 4, 1, NULL },
    { "xp", "float", SF_FLOAT, 4, 4, 1, NULL },
};
static const SfType V1_T_Progress = { "Progress", 8, 2, V1_F_Progress };

static const SfField V1_F_TattooBody[] = {
    { "node", "signed char", SF_INT, 0, 1, 8, NULL },
};
static const SfType V1_T_TattooBody = { "TattooBody", 8, 1, V1_F_TattooBody };

static const SfField V1_F_WornJewel[] = {
    { "id", "char", SF_TEXT, 0, 1, 72, NULL },
    { "var", "unsigned short", SF_UINT, 72, 2, 1, NULL },
};
static const SfType V1_T_WornJewel = { "WornJewel", 74, 2, V1_F_WornJewel };

static const SfField V1_F_Jewelry[] = {
    { "slot", "WornJewel", SF_STRUCT, 0, 74, 9, &V1_T_WornJewel },
};
static const SfType V1_T_Jewelry = { "Jewelry", 666, 1, V1_F_Jewelry };

static const SfField V1_F_Hydration[] = {
    { "water", "float", SF_FLOAT, 0, 4, 1, NULL },
    { "drunk", "float", SF_FLOAT, 4, 4, 1, NULL },
    { "curse", "float", SF_FLOAT, 8, 4, 1, NULL },
    { "incubate", "float", SF_FLOAT, 12, 4, 1, NULL },
};
static const SfType V1_T_Hydration = { "Hydration", 16, 4, V1_F_Hydration };

static const SfField V1_F_DlgOption[] = {
    { "icon", "IconId", SF_INT, 0, 4, 1, NULL },
    { "label", "char", SF_TEXT, 4, 1, 64, NULL },
    { "hint", "char", SF_TEXT, 68, 1, 160, NULL },
    { "badge", "char", SF_TEXT, 228, 1, 8, NULL },
    { "enabled", "bool", SF_BOOL, 236, 1, 1, NULL },
    { "marked", "bool", SF_BOOL, 237, 1, 1, NULL },
    { "value", "int", SF_INT, 240, 4, 1, NULL },
};
static const SfType V1_T_DlgOption = { "DlgOption", 244, 7, V1_F_DlgOption };

static const SfField V1_F_Dialog[] = {
    { "open", "bool", SF_BOOL, 0, 1, 1, NULL },
    { "speaker", "char", SF_TEXT, 1, 1, 48, NULL },
    { "portrait", "IconId", SF_INT, 52, 4, 1, NULL },
    { "text", "char", SF_TEXT, 56, 1, 320, NULL },
    { "opt", "DlgOption", SF_STRUCT, 376, 244, 24, &V1_T_DlgOption },
    { "n", "int", SF_INT, 6232, 4, 1, NULL },
    { "cursor", "int", SF_INT, 6236, 4, 1, NULL },
};
static const SfType V1_T_Dialog = { "Dialog", 6240, 7, V1_F_Dialog };

static const SfField V1_F_Journey[] = {
    { "used", "bool", SF_BOOL, 0, 1, 1, NULL },
    { "kind", "JourneyKind", SF_INT, 4, 4, 1, NULL },
    { "people", "int", SF_INT, 8, 4, 8, NULL },
    { "n", "int", SF_INT, 40, 4, 1, NULL },
    { "site", "int", SF_INT, 44, 4, 1, NULL },
    { "x0", "float", SF_FLOAT, 48, 4, 1, NULL },
    { "z0", "float", SF_FLOAT, 52, 4, 1, NULL },
    { "x1", "float", SF_FLOAT, 56, 4, 1, NULL },
    { "z1", "float", SF_FLOAT, 60, 4, 1, NULL },
    { "t", "float", SF_FLOAT, 64, 4, 1, NULL },
    { "eta", "float", SF_FLOAT, 68, 4, 1, NULL },
    { "mode", "TravelMode", SF_INT, 72, 4, 1, NULL },
    { "knowledge", "float", SF_FLOAT, 76, 4, 1, NULL },
    { "request", "int", SF_INT, 80, 4, 1, NULL },
    { "arrived", "bool", SF_BOOL, 84, 1, 1, NULL },
    { "fate", "int", SF_INT, 88, 4, 8, NULL },
};
static const SfType V1_T_Journey = { "Journey", 120, 16, V1_F_Journey };

static const SfField V1_F_GameActions[] = {
    { "rng", "Rng", SF_STRUCT, 8, 4, 1, &V1_T_Rng },
    { "hands", "Hands", SF_STRUCT, 12, 228, 1, &V1_T_Hands },
    { "preset", "int", SF_INT, 240, 4, 1, NULL },
    { "torch_lit", "bool", SF_BOOL, 244, 1, 1, NULL },
    { "doing", "int", SF_INT, 248, 4, 1, NULL },
    { "timer", "float", SF_FLOAT, 252, 4, 1, NULL },
    { "target", "int", SF_INT, 256, 4, 1, NULL },
    { "menu_open", "bool", SF_BOOL, 260, 1, 1, NULL },
    { "stock_open", "bool", SF_BOOL, 261, 1, 1, NULL },
    { "cursor", "int", SF_INT, 264, 4, 1, NULL },
    { "camps", "CampSite", SF_STRUCT, 268, 5044, 6, &V1_T_CampSite },
    { "here", "int", SF_INT, 30532, 4, 1, NULL },
    { "player_pos", "Vector3", SF_BYTES, 30536, 12, 1, NULL },
    { "found_pending", "bool", SF_BOOL, 30548, 1, 1, NULL },
    { "found_x", "float", SF_FLOAT, 30552, 4, 1, NULL },
    { "found_z", "float", SF_FLOAT, 30556, 4, 1, NULL },
    { "burn_camp", "int", SF_INT, 30560, 4, 1, NULL },
    { "burn_next", "int", SF_INT, 30564, 4, 1, NULL },
    { "projects", "BuildProject", SF_STRUCT, 30568, 20, 8, &V1_T_BuildProject },
    { "project_count", "int", SF_INT, 30728, 4, 1, NULL },
    { "crew_present", "int", SF_INT, 30732, 4, 8, NULL },
    { "crew_size", "int", SF_INT, 30764, 4, 8, NULL },
    { "crafting", "int", SF_INT, 30796, 4, 1, NULL },
    { "craft_timer", "float", SF_FLOAT, 30800, 4, 1, NULL },
    { "craft_total", "float", SF_FLOAT, 30804, 4, 1, NULL },
    { "npcs", "Npc", SF_STRUCT, 30808, 116, 64, &V1_T_Npc },
    { "animals", "Animal", SF_STRUCT, 38232, 288, 64, &V1_T_Animal },
    { "animal_count", "int", SF_INT, 56664, 4, 1, NULL },
    { "mounted", "int", SF_INT, 56668, 4, 1, NULL },
    { "next_group", "int", SF_INT, 56672, 4, 1, NULL },
    { "fauna_timer", "float", SF_FLOAT, 56676, 4, 1, NULL },
    { "reveal_timer", "float", SF_FLOAT, 56680, 4, 1, NULL },
    { "swarms", "Swarm", SF_STRUCT, 56684, 464, 24, &V1_T_Swarm },
    { "swarm_timer", "float", SF_FLOAT, 67820, 4, 1, NULL },
    { "sting_timer", "float", SF_FLOAT, 67824, 4, 1, NULL },
    { "fires_out", "bool", SF_BOOL, 67828, 1, 1, NULL },
    { "raining", "bool", SF_BOOL, 67829, 1, 1, NULL },
    { "grass", "bool", SF_BOOL, 67830, 1, 1, NULL },
    { "fire_near", "bool", SF_BOOL, 67831, 1, 1, NULL },
    { "ignite_at", "Vector3", SF_BYTES, 67832, 12, 8, NULL },
    { "ignite_n", "int", SF_INT, 67928, 4, 1, NULL },
    { "pockets", "Bag", SF_STRUCT, 67932, 2708, 1, &V1_T_Bag },
    { "backpack", "Bag", SF_STRUCT, 70640, 2708, 1, &V1_T_Bag },
    { "cart", "Bag", SF_STRUCT, 73348, 2708, 1, &V1_T_Bag },
    { "armory", "Bag", SF_STRUCT, 76056, 2708, 1, &V1_T_Bag },
    { "packs", "Bag", SF_STRUCT, 78764, 2708, 4, &V1_T_Bag },
    { "pack_animal", "int", SF_INT, 89596, 4, 4, NULL },
    { "prog", "Progress", SF_STRUCT, 89612, 8, 1, &V1_T_Progress },
    { "tattoos", "TattooBody", SF_STRUCT, 89620, 8, 1, &V1_T_TattooBody },
    { "jewels", "Jewelry", SF_STRUCT, 89628, 666, 1, &V1_T_Jewelry },
    { "outfit", "Outfit", SF_STRUCT, 90294, 5, 1, &V1_T_Outfit },
    { "wear_cursor", "int", SF_INT, 90300, 4, 1, NULL },
    { "dress_t", "float", SF_FLOAT, 90304, 4, 1, NULL },
    { "hydro", "Hydration", SF_STRUCT, 90308, 16, 1, &V1_T_Hydration },
    { "dry_t", "float", SF_FLOAT, 90324, 4, 1, NULL },
    { "last_region", "int", SF_INT, 90328, 4, 1, NULL },
    { "seen_settle", "uint32_t", SF_UINT, 90332, 4, 1, NULL },
    { "seen_dens", "uint64_t", SF_UINT, 90336, 8, 1, NULL },
    { "seen_tribes", "uint32_t", SF_UINT, 90344, 4, 1, NULL },
    { "raided_tribes", "uint32_t", SF_UINT, 90348, 4, 1, NULL },
    { "seen_sites", "uint64_t", SF_UINT, 90352, 8, 1, NULL },
    { "ab_timer", "float", SF_FLOAT, 90360, 4, 9, NULL },
    { "ab_cd", "float", SF_FLOAT, 90396, 4, 9, NULL },
    { "ab_pot", "float", SF_FLOAT, 90432, 4, 9, NULL },
    { "warmth_boost", "float", SF_FLOAT, 90468, 4, 1, NULL },
    { "dlg", "Dialog", SF_STRUCT, 90472, 6240, 1, &V1_T_Dialog },
    { "talk_mode", "int", SF_INT, 96712, 4, 1, NULL },
    { "talk_member", "int", SF_INT, 96716, 4, 1, NULL },
    { "talk_a", "int", SF_INT, 96720, 4, 1, NULL },
    { "talk_b", "int", SF_INT, 96724, 4, 1, NULL },
    { "talk_c", "int", SF_INT, 96728, 4, 1, NULL },
    { "equip_tab", "int", SF_INT, 96732, 4, 1, NULL },
    { "jewel_cursor", "int", SF_INT, 96736, 4, 1, NULL },
    { "tattoo_cursor", "int", SF_INT, 96740, 4, 1, NULL },
    { "journeys", "Journey", SF_STRUCT, 96744, 120, 8, &V1_T_Journey },
    { "travel_pick", "int", SF_INT, 97704, 4, 8, NULL },
    { "travel_npick", "int", SF_INT, 97736, 4, 1, NULL },
    { "inv_open", "bool", SF_BOOL, 97740, 1, 1, NULL },
    { "equip_open", "bool", SF_BOOL, 97741, 1, 1, NULL },
    { "inv_pane", "int", SF_INT, 97744, 4, 1, NULL },
    { "inv_cont", "int", SF_INT, 97748, 4, 2, NULL },
    { "inv_cursor", "int", SF_INT, 97756, 4, 2, NULL },
    { "equip_cursor", "int", SF_INT, 97764, 4, 1, NULL },
    { "menu_tab", "int", SF_INT, 97768, 4, 1, NULL },
    { "tab_cursor", "int", SF_INT, 97772, 4, 4, NULL },
    { "take_all", "bool", SF_BOOL, 97788, 1, 1, NULL },
    { "loot", "Bag", SF_STRUCT, 97792, 2708, 8, &V1_T_Bag },
    { "loot_pos", "Vector3", SF_BYTES, 119456, 12, 8, NULL },
    { "loot_age", "float", SF_FLOAT, 119552, 4, 8, NULL },
    { "pack_size", "int", SF_INT, 119608, 4, 1, NULL },
    { "pack_where", "int", SF_INT, 119612, 4, 1, NULL },
    { "pack_anchor", "int", SF_INT, 119616, 4, 1, NULL },
    { "pack_pos", "Vector3", SF_BYTES, 119620, 12, 1, NULL },
    { "climbing", "bool", SF_BOOL, 119632, 1, 1, NULL },
    { "climb_t", "float", SF_FLOAT, 119636, 4, 1, NULL },
    { "climb_from", "Vector3", SF_BYTES, 119640, 12, 1, NULL },
    { "climb_top", "Vector3", SF_BYTES, 119652, 12, 1, NULL },
    { "climb_over", "Vector3", SF_BYTES, 119664, 12, 1, NULL },
    { "climb_to", "Vector3", SF_BYTES, 119676, 12, 1, NULL },
    { "hidden", "bool", SF_BOOL, 119688, 1, 1, NULL },
    { "pl_down", "bool", SF_BOOL, 119689, 1, 1, NULL },
    { "pl_hit", "bool", SF_BOOL, 119690, 1, 1, NULL },
    { "pl_blocking", "bool", SF_BOOL, 119691, 1, 1, NULL },
    { "pl_limping", "bool", SF_BOOL, 119692, 1, 1, NULL },
    { "pl_spear", "bool", SF_BOOL, 119693, 1, 1, NULL },
    { "pl_knocked", "bool", SF_BOOL, 119694, 1, 1, NULL },
    { "pl_attacking", "int", SF_INT, 119696, 4, 1, NULL },
    { "pl_ranged", "int", SF_INT, 119700, 4, 1, NULL },
    { "pl_move", "int", SF_INT, 119704, 4, 1, NULL },
    { "swap_anim", "float", SF_FLOAT, 119708, 4, 1, NULL },
};
static const SfType V1_T_GameActions = { "GameActions", 119712, 110, V1_F_GameActions };

static const SfField V1_F_Enemy[] = {
    { "used", "bool", SF_BOOL, 0, 1, 1, NULL },
    { "kind", "EnemyKind", SF_INT, 4, 4, 1, NULL },
    { "pos", "Vector3", SF_BYTES, 8, 12, 1, NULL },
    { "home", "Vector3", SF_BYTES, 20, 12, 1, NULL },
    { "wander_to", "Vector3", SF_BYTES, 32, 12, 1, NULL },
    { "yaw", "float", SF_FLOAT, 44, 4, 1, NULL },
    { "speed", "float", SF_FLOAT, 48, 4, 1, NULL },
    { "h", "Health", SF_STRUCT, 52, 152, 1, &V1_T_Health },
    { "state", "EnemyState", SF_INT, 204, 4, 1, NULL },
    { "timer", "float", SF_FLOAT, 208, 4, 1, NULL },
    { "cooldown", "float", SF_FLOAT, 212, 4, 1, NULL },
    { "attack_anim", "float", SF_FLOAT, 216, 4, 1, NULL },
    { "hit_anim", "float", SF_FLOAT, 220, 4, 1, NULL },
    { "corpse", "float", SF_FLOAT, 224, 4, 1, NULL },
    { "shown", "float", SF_FLOAT, 228, 4, 1, NULL },
    { "reload", "float", SF_FLOAT, 232, 4, 1, NULL },
    { "target", "int", SF_INT, 236, 4, 1, NULL },
    { "armor", "Armor", SF_STRUCT, 240, 864, 1, &V1_T_Armor },
    { "shield", "bool", SF_BOOL, 1104, 1, 1, NULL },
    { "armed", "bool", SF_BOOL, 1105, 1, 1, NULL },
    { "blocking", "bool", SF_BOOL, 1106, 1, 1, NULL },
    { "block_timer", "float", SF_FLOAT, 1108, 4, 1, NULL },
    { "stagger", "float", SF_FLOAT, 1112, 4, 1, NULL },
    { "knock", "float", SF_FLOAT, 1116, 4, 1, NULL },
    { "move", "int", SF_INT, 1120, 4, 1, NULL },
    { "combo", "int", SF_INT, 1124, 4, 1, NULL },
    { "move_anim", "float", SF_FLOAT, 1128, 4, 1, NULL },
    { "mounted", "bool", SF_BOOL, 1132, 1, 1, NULL },
    { "loot_dropped", "bool", SF_BOOL, 1133, 1, 1, NULL },
    { "awed", "bool", SF_BOOL, 1134, 1, 1, NULL },
};
static const SfType V1_T_Enemy = { "Enemy", 1136, 30, V1_F_Enemy };

static const SfField V1_F_V3[] = {
    { "x", "float", SF_FLOAT, 0, 4, 1, NULL },
    { "y", "float", SF_FLOAT, 4, 4, 1, NULL },
    { "z", "float", SF_FLOAT, 8, 4, 1, NULL },
};
static const SfType V1_T_V3 = { "V3", 12, 3, V1_F_V3 };

static const SfField V1_F_Projectile[] = {
    { "pos", "V3", SF_STRUCT, 0, 12, 1, &V1_T_V3 },
    { "vel", "V3", SF_STRUCT, 12, 12, 1, &V1_T_V3 },
    { "kind", "ProjectileKind", SF_INT, 24, 4, 1, NULL },
    { "alive", "bool", SF_BOOL, 28, 1, 1, NULL },
};
static const SfType V1_T_Projectile = { "Projectile", 32, 4, V1_F_Projectile };

static const SfField V1_F_Shot[] = {
    { "p", "Projectile", SF_STRUCT, 0, 32, 1, &V1_T_Projectile },
    { "owner", "int", SF_INT, 32, 4, 1, NULL },
    { "life", "float", SF_FLOAT, 36, 4, 1, NULL },
    { "stuck", "bool", SF_BOOL, 40, 1, 1, NULL },
    { "dir", "V3", SF_STRUCT, 44, 12, 1, &V1_T_V3 },
    { "burning", "bool", SF_BOOL, 56, 1, 1, NULL },
};
static const SfType V1_T_Shot = { "Shot", 60, 6, V1_F_Shot };

static const SfField V1_F_Combat[] = {
    { "seed", "unsigned", SF_UINT, 0, 4, 1, NULL },
    { "rng", "Rng", SF_STRUCT, 4, 4, 1, &V1_T_Rng },
    { "player", "Health", SF_STRUCT, 8, 152, 1, &V1_T_Health },
    { "enemies", "Enemy", SF_STRUCT, 160, 1136, 16, &V1_T_Enemy },
    { "attack_cd", "float", SF_FLOAT, 18336, 4, 1, NULL },
    { "attack_anim", "float", SF_FLOAT, 18340, 4, 1, NULL },
    { "hit_anim", "float", SF_FLOAT, 18344, 4, 1, NULL },
    { "combo", "int", SF_INT, 18348, 4, 1, NULL },
    { "blocking", "bool", SF_BOOL, 18352, 1, 1, NULL },
    { "spawn_timer", "float", SF_FLOAT, 18356, 4, 1, NULL },
    { "down_timer", "float", SF_FLOAT, 18360, 4, 1, NULL },
    { "healer_timer", "float", SF_FLOAT, 18364, 4, 1, NULL },
    { "comp_cd", "float", SF_FLOAT, 18368, 4, 64, NULL },
    { "frost_timer", "float", SF_FLOAT, 18624, 4, 1, NULL },
    { "show_panel", "bool", SF_BOOL, 18628, 1, 1, NULL },
    { "armor", "Armor", SF_STRUCT, 18632, 864, 1, &V1_T_Armor },
    { "shots", "Shot", SF_STRUCT, 19496, 60, 48, &V1_T_Shot },
    { "aiming", "bool", SF_BOOL, 22376, 1, 1, NULL },
    { "draw", "float", SF_FLOAT, 22380, 4, 1, NULL },
    { "reload", "float", SF_FLOAT, 22384, 4, 1, NULL },
    { "aim_pitch", "float", SF_FLOAT, 22388, 4, 1, NULL },
    { "ammo_shown", "int", SF_INT, 22392, 4, 1, NULL },
    { "arrow_lit", "bool", SF_BOOL, 22396, 1, 1, NULL },
    { "arrow_lit_timer", "float", SF_FLOAT, 22400, 4, 1, NULL },
    { "v_hold", "float", SF_FLOAT, 22404, 4, 1, NULL },
    { "combo_timer", "float", SF_FLOAT, 22408, 4, 1, NULL },
    { "move", "int", SF_INT, 22412, 4, 1, NULL },
    { "move_anim", "float", SF_FLOAT, 22416, 4, 1, NULL },
    { "stagger", "float", SF_FLOAT, 22420, 4, 1, NULL },
    { "knock", "float", SF_FLOAT, 22424, 4, 1, NULL },
    { "charge_timer", "float", SF_FLOAT, 22428, 4, 1, NULL },
    { "charge_hit", "bool", SF_BOOL, 22432, 1, 1, NULL },
    { "last_pos", "Vector3", SF_BYTES, 22436, 12, 1, NULL },
    { "pl_speed", "float", SF_FLOAT, 22448, 4, 1, NULL },
    { "trample_cd", "float", SF_FLOAT, 22452, 4, 1, NULL },
    { "loose_horse", "Vector3", SF_BYTES, 22456, 12, 4, NULL },
    { "loose_yaw", "float", SF_FLOAT, 22504, 4, 4, NULL },
    { "loose_n", "int", SF_INT, 22520, 4, 1, NULL },
};
static const SfType V1_T_Combat = { "Combat", 22524, 38, V1_F_Combat };

static const SfField V1_F_Warmth[] = {
    { "heat", "float", SF_FLOAT, 0, 4, 1, NULL },
    { "wet", "float", SF_FLOAT, 4, 4, 1, NULL },
};
static const SfType V1_T_Warmth = { "Warmth", 8, 2, V1_F_Warmth };

static const SfField V1_F_HeatStress[] = {
    { "load", "float", SF_FLOAT, 0, 4, 1, NULL },
};
static const SfType V1_T_HeatStress = { "HeatStress", 4, 1, V1_F_HeatStress };

static const SfField V1_F_Qte[] = {
    { "keys", "int", SF_INT, 0, 4, 12, NULL },
    { "len", "int", SF_INT, 48, 4, 1, NULL },
    { "pos", "int", SF_INT, 52, 4, 1, NULL },
    { "per_key", "float", SF_FLOAT, 56, 4, 1, NULL },
    { "time_left", "float", SF_FLOAT, 60, 4, 1, NULL },
    { "mistakes", "int", SF_INT, 64, 4, 1, NULL },
    { "max_mistakes", "int", SF_INT, 68, 4, 1, NULL },
    { "state", "QteState", SF_INT, 72, 4, 1, NULL },
};
static const SfType V1_T_Qte = { "Qte", 76, 8, V1_F_Qte };

static const SfField V1_F_Hazards[] = {
    { "seed", "unsigned", SF_UINT, 0, 4, 1, NULL },
    { "rng", "Rng", SF_STRUCT, 4, 4, 1, &V1_T_Rng },
    { "warmth", "Warmth", SF_STRUCT, 8, 8, 1, &V1_T_Warmth },
    { "faint_timer", "float", SF_FLOAT, 16, 4, 1, NULL },
    { "feels", "float", SF_FLOAT, 20, 4, 1, NULL },
    { "fire", "float", SF_FLOAT, 24, 4, 1, NULL },
    { "heat", "HeatStress", SF_STRUCT, 28, 4, 1, &V1_T_HeatStress },
    { "sun", "float", SF_FLOAT, 32, 4, 1, NULL },
    { "temp_here", "float", SF_FLOAT, 36, 4, 1, NULL },
    { "heat_faint", "float", SF_FLOAT, 40, 4, 1, NULL },
    { "outfit_speed", "float", SF_FLOAT, 44, 4, 1, NULL },
    { "on_ice", "bool", SF_BOOL, 48, 1, 1, NULL },
    { "wading", "bool", SF_BOOL, 49, 1, 1, NULL },
    { "swimming", "bool", SF_BOOL, 50, 1, 1, NULL },
    { "mud", "float", SF_FLOAT, 52, 4, 1, NULL },
    { "danger", "bool", SF_BOOL, 56, 1, 1, NULL },
    { "danger_pos", "Vector3", SF_BYTES, 60, 12, 1, NULL },
    { "danger_radius", "float", SF_FLOAT, 72, 4, 1, NULL },
    { "danger_kind", "SinkKind", SF_INT, 76, 4, 1, NULL },
    { "trap", "TrapKind", SF_INT, 80, 4, 1, NULL },
    { "trap_pos", "Vector3", SF_BYTES, 84, 12, 1, NULL },
    { "qte", "Qte", SF_STRUCT, 96, 76, 1, &V1_T_Qte },
    { "qte_rescue", "bool", SF_BOOL, 172, 1, 1, NULL },
    { "helper", "int", SF_INT, 176, 4, 1, NULL },
    { "sink_depth", "float", SF_FLOAT, 180, 4, 1, NULL },
    { "holes", "Vector3", SF_BYTES, 184, 12, 12, NULL },
    { "hole_count", "int", SF_INT, 328, 4, 1, NULL },
    { "hole_day", "int", SF_INT, 332, 4, 1, NULL },
    { "used_x", "float", SF_FLOAT, 336, 4, 1, NULL },
    { "used_z", "float", SF_FLOAT, 340, 4, 1, NULL },
    { "used_day", "int", SF_INT, 344, 4, 1, NULL },
    { "victim", "int", SF_INT, 348, 4, 1, NULL },
    { "victim_trap", "TrapKind", SF_INT, 352, 4, 1, NULL },
    { "victim_timer", "float", SF_FLOAT, 356, 4, 1, NULL },
    { "escort_on", "bool", SF_BOOL, 360, 1, 1, NULL },
    { "ice_roll", "float", SF_FLOAT, 364, 4, 1, NULL },
    { "frost_timer", "float", SF_FLOAT, 376, 4, 1, NULL },
};
static const SfType V1_T_Hazards = { "Hazards", 384, 37, V1_F_Hazards };

static const SfField V1_F_FireCell[] = {
    { "used", "bool", SF_BOOL, 0, 1, 1, NULL },
    { "x", "float", SF_FLOAT, 4, 4, 1, NULL },
    { "z", "float", SF_FLOAT, 8, 4, 1, NULL },
    { "heat", "float", SF_FLOAT, 12, 4, 1, NULL },
    { "fuel", "float", SF_FLOAT, 16, 4, 1, NULL },
    { "kind", "FuelKind", SF_INT, 20, 4, 1, NULL },
    { "ref", "int", SF_INT, 24, 4, 1, NULL },
    { "age", "float", SF_FLOAT, 28, 4, 1, NULL },
};
static const SfType V1_T_FireCell = { "FireCell", 32, 8, V1_F_FireCell };

static const SfField V1_F_Scorch[] = {
    { "x", "float", SF_FLOAT, 0, 4, 1, NULL },
    { "z", "float", SF_FLOAT, 4, 4, 1, NULL },
    { "r", "float", SF_FLOAT, 8, 4, 1, NULL },
    { "age", "float", SF_FLOAT, 12, 4, 1, NULL },
};
static const SfType V1_T_Scorch = { "Scorch", 16, 4, V1_F_Scorch };

static const SfField V1_F_FireField[] = {
    { "cells", "FireCell", SF_STRUCT, 0, 32, 160, &V1_T_FireCell },
    { "scorch", "Scorch", SF_STRUCT, 5120, 16, 400, &V1_T_Scorch },
    { "scorch_next", "int", SF_INT, 11520, 4, 1, NULL },
    { "rng", "Rng", SF_STRUCT, 11524, 4, 1, &V1_T_Rng },
    { "spread_timer", "float", SF_FLOAT, 11528, 4, 1, NULL },
};
static const SfType V1_T_FireField = { "FireField", 11532, 5, V1_F_FireField };

static const SfField V1_F_Disasters[] = {
    { "fire", "FireField", SF_STRUCT, 0, 11532, 1, &V1_T_FireField },
    { "rng", "Rng", SF_STRUCT, 11532, 4, 1, &V1_T_Rng },
    { "tree_burn", "float", SF_FLOAT, 11536, 4, 80, NULL },
    { "bolt_time", "float", SF_FLOAT, 11856, 4, 1, NULL },
    { "bolt_to", "Vector3", SF_BYTES, 11860, 12, 1, NULL },
    { "bolt_seed", "float", SF_FLOAT, 11872, 4, 1, NULL },
    { "relight_timer", "float", SF_FLOAT, 11876, 4, 1, NULL },
    { "burn_tick", "float", SF_FLOAT, 11880, 4, 1, NULL },
    { "warn_timer", "float", SF_FLOAT, 11884, 4, 1, NULL },
    { "camp_warned", "bool", SF_BOOL, 11888, 1, 1, NULL },
};
static const SfType V1_T_Disasters = { "Disasters", 11892, 10, V1_F_Disasters };

static const SfField V1_F_SavedProp[] = {
    { "id", "char", SF_TEXT, 0, 1, 72, NULL },
    { "pos", "Vector3", SF_BYTES, 72, 12, 1, NULL },
    { "yaw", "float", SF_FLOAT, 84, 4, 1, NULL },
    { "condition", "float", SF_FLOAT, 88, 4, 1, NULL },
};
static const SfType V1_T_SavedProp = { "SavedProp", 92, 4, V1_F_SavedProp };

static const SfField V1_F_MemoryCell[] = {
    { "light", "float", SF_FLOAT, 0, 4, 1, NULL },
    { "familiarity", "float", SF_FLOAT, 4, 4, 1, NULL },
    { "t_last", "float", SF_FLOAT, 8, 4, 1, NULL },
};
static const SfType V1_T_MemoryCell = { "MemoryCell", 12, 3, V1_F_MemoryCell };

static const SfField V1_F_MemoryPage[] = {
    { "px", "int", SF_INT, 0, 4, 1, NULL },
    { "pz", "int", SF_INT, 4, 4, 1, NULL },
    { "cells", "MemoryCell", SF_STRUCT, 8, 12, 1024, &V1_T_MemoryCell },
};
static const SfType V1_T_MemoryPage = { "MemoryPage", 12296, 3, V1_F_MemoryPage };

static const SfField V1_F_MapMarker[] = {
    { "x", "float", SF_FLOAT, 0, 4, 1, NULL },
    { "z", "float", SF_FLOAT, 4, 4, 1, NULL },
    { "kind", "MarkerKind", SF_INT, 8, 4, 1, NULL },
};
static const SfType V1_T_MapMarker = { "MapMarker", 12, 3, V1_F_MapMarker };

static const SfField V1_F_Remains[] = {
    { "x", "float", SF_FLOAT, 0, 4, 1, NULL },
    { "y", "float", SF_FLOAT, 4, 4, 1, NULL },
    { "z", "float", SF_FLOAT, 8, 4, 1, NULL },
    { "yaw", "float", SF_FLOAT, 12, 4, 1, NULL },
};
static const SfType V1_T_Remains = { "Remains", 16, 4, V1_F_Remains };

static const SfField V1_F_DeathSave[] = {
    { "has_rest", "int", SF_INT, 0, 4, 1, NULL },
    { "rest_x", "float", SF_FLOAT, 4, 4, 1, NULL },
    { "rest_z", "float", SF_FLOAT, 8, 4, 1, NULL },
    { "rest_name", "char", SF_TEXT, 12, 1, 48, NULL },
    { "deaths", "int", SF_INT, 60, 4, 1, NULL },
    { "count", "int", SF_INT, 64, 4, 1, NULL },
    { "next", "int", SF_INT, 68, 4, 1, NULL },
    { "remains", "Remains", SF_STRUCT, 72, 16, 16, &V1_T_Remains },
};
static const SfType V1_T_DeathSave = { "DeathSave", 328, 8, V1_F_DeathSave };

static const SfField V1_F_QbSlot[] = {
    { "kind", "int", SF_INT, 0, 4, 1, NULL },
    { "grip", "int", SF_INT, 4, 4, 1, NULL },
    { "id", "char", SF_TEXT, 8, 1, 72, NULL },
};
static const SfType V1_T_QbSlot = { "QbSlot", 80, 3, V1_F_QbSlot };

#define V1_MAX_MARKERS 32
#define V1_QUICK_SLOTS 9
