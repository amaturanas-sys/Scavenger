// Esquema de la partida guardada: generado por tools/save/esquema.py, no editar a mano.
// Describe, campo por campo, todo lo que se guarda (docs/PARTIDAS.md); las posiciones y los
// tamaños los pone el compilador. Si cambias un struct que se guarda, vuelve a correr el script
// (CI lo verifica). Los punteros no se guardan: al cargar, el juego los vuelve a enlazar.

// Rng (src/sim/rng.h)
static const SfField SF_F_Rng[] = {
    SF_ONE(Rng, state, "uint32_t", SF_UINT, NULL),
};
static const SfType SF_T_Rng = SF_TYPE(Rng, SF_F_Rng);

// SavedMisc (src/game/save_game.h)
static const SfField SF_F_SavedMisc[] = {
    SF_ONE(SavedMisc, last_champion, "int", SF_INT, NULL),
    SF_ONE(SavedMisc, cam_yaw, "float", SF_FLOAT, NULL),
    SF_ONE(SavedMisc, rng, "Rng", SF_STRUCT, &SF_T_Rng),
};
static const SfType SF_T_SavedMisc = SF_TYPE(SavedMisc, SF_F_SavedMisc);

// Player (src/game/player.h)
static const SfField SF_F_Player[] = {
    SF_ONE(Player, pos, "Vector3", SF_BYTES, NULL),
    SF_ONE(Player, yaw, "float", SF_FLOAT, NULL),
    SF_ONE(Player, vy, "float", SF_FLOAT, NULL),
    SF_ONE(Player, grounded, "bool", SF_BOOL, NULL),
    SF_ONE(Player, sneaking, "bool", SF_BOOL, NULL),
    SF_ONE(Player, moving, "bool", SF_BOOL, NULL),
    SF_ONE(Player, stance, "Stance", SF_INT, NULL),
    SF_ONE(Player, noise, "float", SF_FLOAT, NULL),
    SF_ONE(Player, speed_scale, "float", SF_FLOAT, NULL),
    SF_ONE(Player, draw_lift, "float", SF_FLOAT, NULL),
};
static const SfType SF_T_Player = SF_TYPE(Player, SF_F_Player);

// Kingdom (src/sim/troop.h)
static const SfField SF_F_Kingdom[] = {
    SF_ARR(Kingdom, name, "char", SF_TEXT, NULL),
    SF_ONE(Kingdom, relation, "float", SF_FLOAT, NULL),
    SF_ARR(Kingdom, stance, "float", SF_FLOAT, NULL),
};
static const SfType SF_T_Kingdom = SF_TYPE(Kingdom, SF_F_Kingdom);

// Wound (src/sim/health.h)
static const SfField SF_F_Wound[] = {
    SF_ONE(Wound, kind, "WoundKind", SF_INT, NULL),
    SF_ONE(Wound, part, "BodyPart", SF_INT, NULL),
    SF_ONE(Wound, severity, "float", SF_FLOAT, NULL),
    SF_ONE(Wound, bleeding, "bool", SF_BOOL, NULL),
    SF_ONE(Wound, treated, "bool", SF_BOOL, NULL),
};
static const SfType SF_T_Wound = SF_TYPE(Wound, SF_F_Wound);

// Health (src/sim/health.h)
static const SfField SF_F_Health[] = {
    SF_ONE(Health, hp, "float", SF_FLOAT, NULL),
    SF_ONE(Health, hp_max, "float", SF_FLOAT, NULL),
    SF_ONE(Health, blood, "float", SF_FLOAT, NULL),
    SF_ARR(Health, wounds, "Wound", SF_STRUCT, &SF_T_Wound),
    SF_ONE(Health, wound_count, "int", SF_INT, NULL),
    SF_ONE(Health, down, "bool", SF_BOOL, NULL),
    SF_ONE(Health, dead, "bool", SF_BOOL, NULL),
    SF_ONE(Health, beast, "bool", SF_BOOL, NULL),
    SF_ONE(Health, venom, "float", SF_FLOAT, NULL),
};
static const SfType SF_T_Health = SF_TYPE(Health, SF_F_Health);

// ArmorPiece (src/sim/armor.h)
static const SfField SF_F_ArmorPiece[] = {
    SF_ARR(ArmorPiece, id, "char", SF_TEXT, NULL),
    SF_ONE(ArmorPiece, material, "ArmorMaterial", SF_INT, NULL),
    SF_ONE(ArmorPiece, zones, "unsigned", SF_UINT, NULL),
    SF_ONE(ArmorPiece, coverage, "float", SF_FLOAT, NULL),
    SF_ONE(ArmorPiece, mail, "bool", SF_BOOL, NULL),
    SF_ONE(ArmorPiece, durability, "float", SF_FLOAT, NULL),
    SF_ONE(ArmorPiece, durability_max, "float", SF_FLOAT, NULL),
};
static const SfType SF_T_ArmorPiece = SF_TYPE(ArmorPiece, SF_F_ArmorPiece);

// Armor (src/sim/armor.h)
static const SfField SF_F_Armor[] = {
    SF_ARR(Armor, slot, "ArmorPiece", SF_STRUCT, &SF_T_ArmorPiece),
};
static const SfType SF_T_Armor = SF_TYPE(Armor, SF_F_Armor);

// BagSlot (src/sim/storage.h)
static const SfField SF_F_BagSlot[] = {
    SF_ARR(BagSlot, id, "char", SF_TEXT, NULL),
    SF_ONE(BagSlot, count, "int", SF_INT, NULL),
    SF_ONE(BagSlot, condition, "float", SF_FLOAT, NULL),
    SF_ONE(BagSlot, var, "unsigned short", SF_UINT, NULL),
};
static const SfType SF_T_BagSlot = SF_TYPE(BagSlot, SF_F_BagSlot);

// Bag (src/sim/storage.h)
static const SfField SF_F_Bag[] = {
    SF_ONE(Bag, kind, "BagKind", SF_INT, NULL),
    SF_ONE(Bag, cap_kg, "float", SF_FLOAT, NULL),
    SF_ONE(Bag, max_size, "float", SF_FLOAT, NULL),
    SF_ONE(Bag, slots, "int", SF_INT, NULL),
    SF_ARR(Bag, s, "BagSlot", SF_STRUCT, &SF_T_BagSlot),
    SF_ONE(Bag, n, "int", SF_INT, NULL),
};
static const SfType SF_T_Bag = SF_TYPE(Bag, SF_F_Bag);

// Outfit (src/sim/apparel.h)
static const SfField SF_F_Outfit[] = {
    SF_ARR(Outfit, g, "signed char", SF_INT, NULL),
};
static const SfType SF_T_Outfit = SF_TYPE(Outfit, SF_F_Outfit);

// Member (src/sim/troop.h)
static const SfField SF_F_Member[] = {
    SF_ONE(Member, id, "int", SF_INT, NULL),
    SF_ARR(Member, name, "char", SF_TEXT, NULL),
    SF_ONE(Member, role, "Role", SF_INT, NULL),
    SF_ONE(Member, status, "MemberStatus", SF_INT, NULL),
    SF_ONE(Member, traits, "unsigned", SF_UINT, NULL),
    SF_ONE(Member, morale, "float", SF_FLOAT, NULL),
    SF_ONE(Member, loyalty, "float", SF_FLOAT, NULL),
    SF_ONE(Member, champion, "int", SF_INT, NULL),
    SF_ONE(Member, health, "Health", SF_STRUCT, &SF_T_Health),
    SF_ONE(Member, armor, "Armor", SF_STRUCT, &SF_T_Armor),
    SF_ONE(Member, outfitted, "bool", SF_BOOL, NULL),
    SF_ONE(Member, camp, "int", SF_INT, NULL),
    SF_ONE(Member, pack, "int", SF_INT, NULL),
    SF_ONE(Member, bag, "Bag", SF_STRUCT, &SF_T_Bag),
    SF_ONE(Member, known, "uint64_t", SF_UINT, NULL),
    SF_ONE(Member, journey, "int", SF_INT, NULL),
    SF_ONE(Member, outfit, "Outfit", SF_STRUCT, &SF_T_Outfit),
};
static const SfType SF_T_Member = SF_TYPE(Member, SF_F_Member);

// CombatStats (src/sim/champion.h)
static const SfField SF_F_CombatStats[] = {
    SF_ONE(CombatStats, size, "float", SF_FLOAT, NULL),
    SF_ONE(CombatStats, speed, "float", SF_FLOAT, NULL),
    SF_ONE(CombatStats, strength, "float", SF_FLOAT, NULL),
    SF_ONE(CombatStats, endurance, "float", SF_FLOAT, NULL),
    SF_ONE(CombatStats, aim, "float", SF_FLOAT, NULL),
    SF_ONE(CombatStats, riding, "float", SF_FLOAT, NULL),
    SF_ONE(CombatStats, healing, "float", SF_FLOAT, NULL),
};
static const SfType SF_T_CombatStats = SF_TYPE(CombatStats, SF_F_CombatStats);

// Champion (src/sim/champion.h)
static const SfField SF_F_Champion[] = {
    SF_ARR(Champion, name, "char", SF_TEXT, NULL),
    SF_ARR(Champion, epithet, "char", SF_TEXT, NULL),
    SF_ONE(Champion, origin, "int", SF_INT, NULL),
    SF_ONE(Champion, exile, "int", SF_INT, NULL),
    SF_ONE(Champion, livelihood, "int", SF_INT, NULL),
    SF_ONE(Champion, aspiration, "int", SF_INT, NULL),
    SF_ONE(Champion, gifts, "unsigned", SF_UINT, NULL),
    SF_ONE(Champion, weapon, "int", SF_INT, NULL),
    SF_ONE(Champion, stats, "CombatStats", SF_STRUCT, &SF_T_CombatStats),
    SF_ONE(Champion, traits, "unsigned", SF_UINT, NULL),
};
static const SfType SF_T_Champion = SF_TYPE(Champion, SF_F_Champion);

// Troop (src/sim/troop.h)
static const SfField SF_F_Troop[] = {
    SF_ARR(Troop, members, "Member", SF_STRUCT, &SF_T_Member),
    SF_ONE(Troop, count, "int", SF_INT, NULL),
    SF_ONE(Troop, next_id, "int", SF_INT, NULL),
    // overlord: puntero, no se guarda
    SF_ARR(Troop, champions, "Champion", SF_STRUCT, &SF_T_Champion),
    SF_ONE(Troop, champion_count, "int", SF_INT, NULL),
    SF_ONE(Troop, rebellion_scale, "float", SF_FLOAT, NULL),
};
static const SfType SF_T_Troop = SF_TYPE(Troop, SF_F_Troop);

// Held (src/sim/actions.h)
static const SfField SF_F_Held[] = {
    SF_ARR(Held, id, "char", SF_TEXT, NULL),
    SF_ONE(Held, kind, "InvHands", SF_INT, NULL),
};
static const SfType SF_T_Held = SF_TYPE(Held, SF_F_Held);

// Hands (src/sim/actions.h)
static const SfField SF_F_Hands[] = {
    SF_ONE(Hands, right, "Held", SF_STRUCT, &SF_T_Held),
    SF_ONE(Hands, left, "Held", SF_STRUCT, &SF_T_Held),
    SF_ONE(Hands, sheathed, "bool", SF_BOOL, NULL),
    SF_ARR(Hands, carried, "char", SF_TEXT, NULL),
};
static const SfType SF_T_Hands = SF_TYPE(Hands, SF_F_Hands);

// StockEntry (src/sim/economy.h)
static const SfField SF_F_StockEntry[] = {
    SF_ARR(StockEntry, id, "char", SF_TEXT, NULL),
    SF_ONE(StockEntry, count, "int", SF_INT, NULL),
};
static const SfType SF_T_StockEntry = SF_TYPE(StockEntry, SF_F_StockEntry);

// Stockpile (src/sim/economy.h)
static const SfField SF_F_Stockpile[] = {
    SF_ARR(Stockpile, e, "StockEntry", SF_STRUCT, &SF_T_StockEntry),
    SF_ONE(Stockpile, n, "int", SF_INT, NULL),
};
static const SfType SF_T_Stockpile = SF_TYPE(Stockpile, SF_F_Stockpile);

// CampTask (src/sim/camps.h)
static const SfField SF_F_CampTask[] = {
    SF_ONE(CampTask, kind, "CampTaskKind", SF_INT, NULL),
    SF_ONE(CampTask, member, "int", SF_INT, NULL),
    SF_ONE(CampTask, role, "Role", SF_INT, NULL),
    SF_ONE(CampTask, work, "float", SF_FLOAT, NULL),
    SF_ONE(CampTask, done, "float", SF_FLOAT, NULL),
};
static const SfType SF_T_CampTask = SF_TYPE(CampTask, SF_F_CampTask);

// CampSite (src/sim/camps.h)
static const SfField SF_F_CampSite[] = {
    SF_ONE(CampSite, used, "bool", SF_BOOL, NULL),
    SF_ARR(CampSite, name, "char", SF_TEXT, NULL),
    SF_ONE(CampSite, x, "float", SF_FLOAT, NULL),
    SF_ONE(CampSite, z, "float", SF_FLOAT, NULL),
    SF_ONE(CampSite, guardian, "int", SF_INT, NULL),
    SF_ONE(CampSite, founded_day, "int", SF_INT, NULL),
    SF_ONE(CampSite, stock, "Stockpile", SF_STRUCT, &SF_T_Stockpile),
    SF_ARR(CampSite, task, "CampTask", SF_STRUCT, &SF_T_CampTask),
    SF_ONE(CampSite, ntask, "int", SF_INT, NULL),
};
static const SfType SF_T_CampSite = SF_TYPE(CampSite, SF_F_CampSite);

// BuildProject (src/sim/actions.h)
static const SfField SF_F_BuildProject[] = {
    SF_ONE(BuildProject, def, "BuildId", SF_INT, NULL),
    SF_ONE(BuildProject, x, "float", SF_FLOAT, NULL),
    SF_ONE(BuildProject, z, "float", SF_FLOAT, NULL),
    SF_ONE(BuildProject, progress, "float", SF_FLOAT, NULL),
    SF_ONE(BuildProject, done, "bool", SF_BOOL, NULL),
};
static const SfType SF_T_BuildProject = SF_TYPE(BuildProject, SF_F_BuildProject);

// Npc (src/game/actions_game.h)
static const SfField SF_F_Npc[] = {
    SF_ONE(Npc, member_id, "int", SF_INT, NULL),
    SF_ONE(Npc, pos, "Vector3", SF_BYTES, NULL),
    SF_ONE(Npc, home, "Vector3", SF_BYTES, NULL),
    SF_ONE(Npc, yaw, "float", SF_FLOAT, NULL),
    SF_ONE(Npc, project, "int", SF_INT, NULL),
    SF_ONE(Npc, job, "int", SF_INT, NULL),
    SF_ONE(Npc, moving, "bool", SF_BOOL, NULL),
    SF_ONE(Npc, job_pos, "Vector3", SF_BYTES, NULL),
    SF_ONE(Npc, job_timer, "float", SF_FLOAT, NULL),
    SF_ONE(Npc, escort, "bool", SF_BOOL, NULL),
    SF_ONE(Npc, fighting, "bool", SF_BOOL, NULL),
    SF_ONE(Npc, fight_anim, "float", SF_FLOAT, NULL),
    SF_ONE(Npc, hurt_anim, "float", SF_FLOAT, NULL),
    SF_ONE(Npc, stagger, "float", SF_FLOAT, NULL),
    SF_ONE(Npc, knock, "float", SF_FLOAT, NULL),
    SF_ONE(Npc, move, "int", SF_INT, NULL),
    SF_ONE(Npc, combo, "int", SF_INT, NULL),
    SF_ONE(Npc, move_anim, "float", SF_FLOAT, NULL),
    SF_ONE(Npc, home_camp, "int", SF_INT, NULL),
    SF_ONE(Npc, leave_t, "float", SF_FLOAT, NULL),
    SF_ONE(Npc, leave_dir, "Vector3", SF_BYTES, NULL),
    SF_ONE(Npc, comfort, "signed char", SF_INT, NULL),
};
static const SfType SF_T_Npc = SF_TYPE(Npc, SF_F_Npc);

// Animal (src/sim/animals.h)
static const SfField SF_F_Animal[] = {
    SF_ONE(Animal, used, "bool", SF_BOOL, NULL),
    SF_ONE(Animal, species, "Species", SF_INT, NULL),
    SF_ONE(Animal, state, "AnimalState", SF_INT, NULL),
    SF_ONE(Animal, mode, "AnimalMode", SF_INT, NULL),
    SF_ONE(Animal, x, "float", SF_FLOAT, NULL),
    SF_ONE(Animal, z, "float", SF_FLOAT, NULL),
    SF_ONE(Animal, yaw, "float", SF_FLOAT, NULL),
    SF_ONE(Animal, alt, "float", SF_FLOAT, NULL),
    SF_ONE(Animal, home_x, "float", SF_FLOAT, NULL),
    SF_ONE(Animal, home_z, "float", SF_FLOAT, NULL),
    SF_ONE(Animal, tx, "float", SF_FLOAT, NULL),
    SF_ONE(Animal, tz, "float", SF_FLOAT, NULL),
    SF_ONE(Animal, flee_x, "float", SF_FLOAT, NULL),
    SF_ONE(Animal, flee_z, "float", SF_FLOAT, NULL),
    SF_ONE(Animal, timer, "float", SF_FLOAT, NULL),
    SF_ONE(Animal, fleeing, "bool", SF_BOOL, NULL),
    SF_ONE(Animal, ridden, "bool", SF_BOOL, NULL),
    SF_ONE(Animal, speed, "float", SF_FLOAT, NULL),
    SF_ONE(Animal, h, "Health", SF_STRUCT, &SF_T_Health),
    SF_ONE(Animal, group, "int", SF_INT, NULL),
    SF_ONE(Animal, tkind, "TargetKind", SF_INT, NULL),
    SF_ONE(Animal, target, "int", SF_INT, NULL),
    SF_ONE(Animal, cooldown, "float", SF_FLOAT, NULL),
    SF_ONE(Animal, attack_anim, "float", SF_FLOAT, NULL),
    SF_ONE(Animal, hit_anim, "float", SF_FLOAT, NULL),
    SF_ONE(Animal, alert, "float", SF_FLOAT, NULL),
    SF_ONE(Animal, fear_humans, "float", SF_FLOAT, NULL),
    SF_ONE(Animal, hunger, "float", SF_FLOAT, NULL),
    SF_ONE(Animal, thirst, "float", SF_FLOAT, NULL),
    SF_ONE(Animal, stamina, "float", SF_FLOAT, NULL),
    SF_ONE(Animal, rival_timer, "float", SF_FLOAT, NULL),
    SF_ONE(Animal, clock, "float", SF_FLOAT, NULL),
    SF_ONE(Animal, bound_timer, "float", SF_FLOAT, NULL),
    SF_ONE(Animal, corpse, "float", SF_FLOAT, NULL),
    SF_ONE(Animal, eaten, "float", SF_FLOAT, NULL),
    SF_ONE(Animal, butchered, "bool", SF_BOOL, NULL),
    SF_ONE(Animal, milked, "bool", SF_BOOL, NULL),
    SF_ONE(Animal, weakened, "bool", SF_BOOL, NULL),
    SF_ONE(Animal, killed_by_human, "bool", SF_BOOL, NULL),
};
static const SfType SF_T_Animal = SF_TYPE(Animal, SF_F_Animal);

// SwarmBody (src/sim/swarms.h)
static const SfField SF_F_SwarmBody[] = {
    SF_ONE(SwarmBody, x, "float", SF_FLOAT, NULL),
    SF_ONE(SwarmBody, y, "float", SF_FLOAT, NULL),
    SF_ONE(SwarmBody, z, "float", SF_FLOAT, NULL),
    SF_ONE(SwarmBody, vx, "float", SF_FLOAT, NULL),
    SF_ONE(SwarmBody, vy, "float", SF_FLOAT, NULL),
    SF_ONE(SwarmBody, vz, "float", SF_FLOAT, NULL),
    SF_ONE(SwarmBody, alive, "bool", SF_BOOL, NULL),
};
static const SfType SF_T_SwarmBody = SF_TYPE(SwarmBody, SF_F_SwarmBody);

// Swarm (src/sim/swarms.h)
static const SfField SF_F_Swarm[] = {
    SF_ONE(Swarm, used, "bool", SF_BOOL, NULL),
    SF_ONE(Swarm, kind, "SwarmKind", SF_INT, NULL),
    SF_ONE(Swarm, hx, "float", SF_FLOAT, NULL),
    SF_ONE(Swarm, hy, "float", SF_FLOAT, NULL),
    SF_ONE(Swarm, hz, "float", SF_FLOAT, NULL),
    SF_ONE(Swarm, cx, "float", SF_FLOAT, NULL),
    SF_ONE(Swarm, cy, "float", SF_FLOAT, NULL),
    SF_ONE(Swarm, cz, "float", SF_FLOAT, NULL),
    SF_ONE(Swarm, tx, "float", SF_FLOAT, NULL),
    SF_ONE(Swarm, tz, "float", SF_FLOAT, NULL),
    SF_ONE(Swarm, timer, "float", SF_FLOAT, NULL),
    SF_ONE(Swarm, anger, "float", SF_FLOAT, NULL),
    SF_ONE(Swarm, calm, "float", SF_FLOAT, NULL),
    SF_ONE(Swarm, cooldown, "float", SF_FLOAT, NULL),
    SF_ONE(Swarm, active, "bool", SF_BOOL, NULL),
    SF_ONE(Swarm, honey_taken, "bool", SF_BOOL, NULL),
    SF_ONE(Swarm, n, "int", SF_INT, NULL),
    SF_ARR(Swarm, b, "SwarmBody", SF_STRUCT, &SF_T_SwarmBody),
    SF_ONE(Swarm, cell_x, "int", SF_INT, NULL),
    SF_ONE(Swarm, cell_z, "int", SF_INT, NULL),
};
static const SfType SF_T_Swarm = SF_TYPE(Swarm, SF_F_Swarm);

// Progress (src/sim/talents.h)
static const SfField SF_F_Progress[] = {
    SF_ONE(Progress, level, "int", SF_INT, NULL),
    SF_ONE(Progress, xp, "float", SF_FLOAT, NULL),
};
static const SfType SF_T_Progress = SF_TYPE(Progress, SF_F_Progress);

// TattooBody (src/sim/talents.h)
static const SfField SF_F_TattooBody[] = {
    SF_ARR(TattooBody, node, "signed char", SF_INT, NULL),
};
static const SfType SF_T_TattooBody = SF_TYPE(TattooBody, SF_F_TattooBody);

// WornJewel (src/sim/jewelry.h)
static const SfField SF_F_WornJewel[] = {
    SF_ARR(WornJewel, id, "char", SF_TEXT, NULL),
    SF_ONE(WornJewel, var, "unsigned short", SF_UINT, NULL),
};
static const SfType SF_T_WornJewel = SF_TYPE(WornJewel, SF_F_WornJewel);

// Jewelry (src/sim/jewelry.h)
static const SfField SF_F_Jewelry[] = {
    SF_ARR(Jewelry, slot, "WornJewel", SF_STRUCT, &SF_T_WornJewel),
};
static const SfType SF_T_Jewelry = SF_TYPE(Jewelry, SF_F_Jewelry);

// Hydration (src/sim/water.h)
static const SfField SF_F_Hydration[] = {
    SF_ONE(Hydration, water, "float", SF_FLOAT, NULL),
    SF_ONE(Hydration, drunk, "float", SF_FLOAT, NULL),
    SF_ONE(Hydration, curse, "float", SF_FLOAT, NULL),
    SF_ONE(Hydration, incubate, "float", SF_FLOAT, NULL),
};
static const SfType SF_T_Hydration = SF_TYPE(Hydration, SF_F_Hydration);

// DlgOption (src/game/dialog.h)
static const SfField SF_F_DlgOption[] = {
    SF_ONE(DlgOption, icon, "IconId", SF_INT, NULL),
    SF_ARR(DlgOption, label, "char", SF_TEXT, NULL),
    SF_ARR(DlgOption, hint, "char", SF_TEXT, NULL),
    SF_ARR(DlgOption, badge, "char", SF_TEXT, NULL),
    SF_ONE(DlgOption, enabled, "bool", SF_BOOL, NULL),
    SF_ONE(DlgOption, marked, "bool", SF_BOOL, NULL),
    SF_ONE(DlgOption, value, "int", SF_INT, NULL),
};
static const SfType SF_T_DlgOption = SF_TYPE(DlgOption, SF_F_DlgOption);

// Dialog (src/game/dialog.h)
static const SfField SF_F_Dialog[] = {
    SF_ONE(Dialog, open, "bool", SF_BOOL, NULL),
    SF_ARR(Dialog, speaker, "char", SF_TEXT, NULL),
    SF_ONE(Dialog, portrait, "IconId", SF_INT, NULL),
    SF_ARR(Dialog, text, "char", SF_TEXT, NULL),
    SF_ARR(Dialog, opt, "DlgOption", SF_STRUCT, &SF_T_DlgOption),
    SF_ONE(Dialog, n, "int", SF_INT, NULL),
    SF_ONE(Dialog, cursor, "int", SF_INT, NULL),
};
static const SfType SF_T_Dialog = SF_TYPE(Dialog, SF_F_Dialog);

// Journey (src/sim/travel.h)
static const SfField SF_F_Journey[] = {
    SF_ONE(Journey, used, "bool", SF_BOOL, NULL),
    SF_ONE(Journey, kind, "JourneyKind", SF_INT, NULL),
    SF_ARR(Journey, people, "int", SF_INT, NULL),
    SF_ONE(Journey, n, "int", SF_INT, NULL),
    SF_ONE(Journey, site, "int", SF_INT, NULL),
    SF_ONE(Journey, x0, "float", SF_FLOAT, NULL),
    SF_ONE(Journey, z0, "float", SF_FLOAT, NULL),
    SF_ONE(Journey, x1, "float", SF_FLOAT, NULL),
    SF_ONE(Journey, z1, "float", SF_FLOAT, NULL),
    SF_ONE(Journey, t, "float", SF_FLOAT, NULL),
    SF_ONE(Journey, eta, "float", SF_FLOAT, NULL),
    SF_ONE(Journey, mode, "TravelMode", SF_INT, NULL),
    SF_ONE(Journey, knowledge, "float", SF_FLOAT, NULL),
    SF_ONE(Journey, request, "int", SF_INT, NULL),
    SF_ONE(Journey, arrived, "bool", SF_BOOL, NULL),
    SF_ARR(Journey, fate, "int", SF_INT, NULL),
};
static const SfType SF_T_Journey = SF_TYPE(Journey, SF_F_Journey);

// GameActions (src/game/actions_game.h)
static const SfField SF_F_GameActions[] = {
    // inv: puntero, no se guarda
    SF_ONE(GameActions, rng, "Rng", SF_STRUCT, &SF_T_Rng),
    SF_ONE(GameActions, hands, "Hands", SF_STRUCT, &SF_T_Hands),
    SF_ONE(GameActions, preset, "int", SF_INT, NULL),
    SF_ONE(GameActions, torch_lit, "bool", SF_BOOL, NULL),
    SF_ONE(GameActions, doing, "int", SF_INT, NULL),
    SF_ONE(GameActions, timer, "float", SF_FLOAT, NULL),
    SF_ONE(GameActions, target, "int", SF_INT, NULL),
    SF_ONE(GameActions, menu_open, "bool", SF_BOOL, NULL),
    SF_ONE(GameActions, stock_open, "bool", SF_BOOL, NULL),
    SF_ONE(GameActions, cursor, "int", SF_INT, NULL),
    SF_ARR(GameActions, camps, "CampSite", SF_STRUCT, &SF_T_CampSite),
    SF_ONE(GameActions, here, "int", SF_INT, NULL),
    SF_ONE(GameActions, player_pos, "Vector3", SF_BYTES, NULL),
    SF_ONE(GameActions, found_pending, "bool", SF_BOOL, NULL),
    SF_ONE(GameActions, found_x, "float", SF_FLOAT, NULL),
    SF_ONE(GameActions, found_z, "float", SF_FLOAT, NULL),
    SF_ONE(GameActions, burn_camp, "int", SF_INT, NULL),
    SF_ONE(GameActions, burn_next, "int", SF_INT, NULL),
    SF_ARR(GameActions, projects, "BuildProject", SF_STRUCT, &SF_T_BuildProject),
    SF_ONE(GameActions, project_count, "int", SF_INT, NULL),
    SF_ARR(GameActions, crew_present, "int", SF_INT, NULL),
    SF_ARR(GameActions, crew_size, "int", SF_INT, NULL),
    SF_ONE(GameActions, crafting, "int", SF_INT, NULL),
    SF_ONE(GameActions, craft_timer, "float", SF_FLOAT, NULL),
    SF_ONE(GameActions, craft_total, "float", SF_FLOAT, NULL),
    SF_ARR(GameActions, npcs, "Npc", SF_STRUCT, &SF_T_Npc),
    SF_ARR(GameActions, animals, "Animal", SF_STRUCT, &SF_T_Animal),
    SF_ONE(GameActions, animal_count, "int", SF_INT, NULL),
    SF_ONE(GameActions, mounted, "int", SF_INT, NULL),
    SF_ONE(GameActions, next_group, "int", SF_INT, NULL),
    SF_ONE(GameActions, fauna_timer, "float", SF_FLOAT, NULL),
    SF_ONE(GameActions, reveal_timer, "float", SF_FLOAT, NULL),
    SF_ARR(GameActions, swarms, "Swarm", SF_STRUCT, &SF_T_Swarm),
    SF_ONE(GameActions, swarm_timer, "float", SF_FLOAT, NULL),
    SF_ONE(GameActions, sting_timer, "float", SF_FLOAT, NULL),
    SF_ONE(GameActions, fires_out, "bool", SF_BOOL, NULL),
    SF_ONE(GameActions, raining, "bool", SF_BOOL, NULL),
    SF_ONE(GameActions, grass, "bool", SF_BOOL, NULL),
    SF_ONE(GameActions, fire_near, "bool", SF_BOOL, NULL),
    SF_ARR(GameActions, ignite_at, "Vector3", SF_BYTES, NULL),
    SF_ONE(GameActions, ignite_n, "int", SF_INT, NULL),
    SF_ONE(GameActions, pockets, "Bag", SF_STRUCT, &SF_T_Bag),
    SF_ONE(GameActions, backpack, "Bag", SF_STRUCT, &SF_T_Bag),
    SF_ONE(GameActions, cart, "Bag", SF_STRUCT, &SF_T_Bag),
    SF_ONE(GameActions, armory, "Bag", SF_STRUCT, &SF_T_Bag),
    SF_ARR(GameActions, packs, "Bag", SF_STRUCT, &SF_T_Bag),
    SF_ARR(GameActions, pack_animal, "int", SF_INT, NULL),
    SF_ONE(GameActions, prog, "Progress", SF_STRUCT, &SF_T_Progress),
    SF_ONE(GameActions, tattoos, "TattooBody", SF_STRUCT, &SF_T_TattooBody),
    SF_ONE(GameActions, jewels, "Jewelry", SF_STRUCT, &SF_T_Jewelry),
    SF_ONE(GameActions, outfit, "Outfit", SF_STRUCT, &SF_T_Outfit),
    SF_ONE(GameActions, wear_cursor, "int", SF_INT, NULL),
    SF_ONE(GameActions, dress_t, "float", SF_FLOAT, NULL),
    SF_ONE(GameActions, hydro, "Hydration", SF_STRUCT, &SF_T_Hydration),
    SF_ONE(GameActions, dry_t, "float", SF_FLOAT, NULL),
    SF_ONE(GameActions, last_region, "int", SF_INT, NULL),
    SF_ONE(GameActions, seen_settle, "uint32_t", SF_UINT, NULL),
    SF_ONE(GameActions, seen_dens, "uint64_t", SF_UINT, NULL),
    SF_ONE(GameActions, seen_tribes, "uint32_t", SF_UINT, NULL),
    SF_ONE(GameActions, raided_tribes, "uint32_t", SF_UINT, NULL),
    SF_ONE(GameActions, seen_sites, "uint64_t", SF_UINT, NULL),
    SF_ARR(GameActions, ab_timer, "float", SF_FLOAT, NULL),
    SF_ARR(GameActions, ab_cd, "float", SF_FLOAT, NULL),
    SF_ARR(GameActions, ab_pot, "float", SF_FLOAT, NULL),
    SF_ONE(GameActions, warmth_boost, "float", SF_FLOAT, NULL),
    SF_ONE(GameActions, dlg, "Dialog", SF_STRUCT, &SF_T_Dialog),
    SF_ONE(GameActions, talk_mode, "int", SF_INT, NULL),
    SF_ONE(GameActions, talk_member, "int", SF_INT, NULL),
    SF_ONE(GameActions, talk_a, "int", SF_INT, NULL),
    SF_ONE(GameActions, talk_b, "int", SF_INT, NULL),
    SF_ONE(GameActions, talk_c, "int", SF_INT, NULL),
    SF_ONE(GameActions, equip_tab, "int", SF_INT, NULL),
    SF_ONE(GameActions, jewel_cursor, "int", SF_INT, NULL),
    SF_ONE(GameActions, tattoo_cursor, "int", SF_INT, NULL),
    SF_ARR(GameActions, journeys, "Journey", SF_STRUCT, &SF_T_Journey),
    SF_ARR(GameActions, travel_pick, "int", SF_INT, NULL),
    SF_ONE(GameActions, travel_npick, "int", SF_INT, NULL),
    SF_ONE(GameActions, inv_open, "bool", SF_BOOL, NULL),
    SF_ONE(GameActions, equip_open, "bool", SF_BOOL, NULL),
    SF_ONE(GameActions, inv_pane, "int", SF_INT, NULL),
    SF_ARR(GameActions, inv_cont, "int", SF_INT, NULL),
    SF_ARR(GameActions, inv_cursor, "int", SF_INT, NULL),
    SF_ONE(GameActions, equip_cursor, "int", SF_INT, NULL),
    SF_ONE(GameActions, menu_tab, "int", SF_INT, NULL),
    SF_ARR(GameActions, tab_cursor, "int", SF_INT, NULL),
    SF_ONE(GameActions, take_all, "bool", SF_BOOL, NULL),
    SF_ARR(GameActions, loot, "Bag", SF_STRUCT, &SF_T_Bag),
    SF_ARR(GameActions, loot_pos, "Vector3", SF_BYTES, NULL),
    SF_ARR(GameActions, loot_age, "float", SF_FLOAT, NULL),
    // player_armor: puntero, no se guarda
    // terrain: puntero, no se guarda
    // troop_ref: puntero, no se guarda
    SF_ONE(GameActions, pack_size, "int", SF_INT, NULL),
    SF_ONE(GameActions, pack_where, "int", SF_INT, NULL),
    SF_ONE(GameActions, pack_anchor, "int", SF_INT, NULL),
    SF_ONE(GameActions, pack_pos, "Vector3", SF_BYTES, NULL),
    SF_ONE(GameActions, climbing, "bool", SF_BOOL, NULL),
    SF_ONE(GameActions, climb_t, "float", SF_FLOAT, NULL),
    SF_ONE(GameActions, climb_from, "Vector3", SF_BYTES, NULL),
    SF_ONE(GameActions, climb_top, "Vector3", SF_BYTES, NULL),
    SF_ONE(GameActions, climb_over, "Vector3", SF_BYTES, NULL),
    SF_ONE(GameActions, climb_to, "Vector3", SF_BYTES, NULL),
    SF_ONE(GameActions, hidden, "bool", SF_BOOL, NULL),
    SF_ONE(GameActions, pl_down, "bool", SF_BOOL, NULL),
    SF_ONE(GameActions, pl_hit, "bool", SF_BOOL, NULL),
    SF_ONE(GameActions, pl_blocking, "bool", SF_BOOL, NULL),
    SF_ONE(GameActions, pl_limping, "bool", SF_BOOL, NULL),
    SF_ONE(GameActions, pl_spear, "bool", SF_BOOL, NULL),
    SF_ONE(GameActions, pl_knocked, "bool", SF_BOOL, NULL),
    SF_ONE(GameActions, pl_attacking, "int", SF_INT, NULL),
    SF_ONE(GameActions, pl_ranged, "int", SF_INT, NULL),
    SF_ONE(GameActions, pl_move, "int", SF_INT, NULL),
    SF_ONE(GameActions, swap_anim, "float", SF_FLOAT, NULL),
};
static const SfType SF_T_GameActions = SF_TYPE(GameActions, SF_F_GameActions);

// Enemy (src/game/combat_game.h)
static const SfField SF_F_Enemy[] = {
    SF_ONE(Enemy, used, "bool", SF_BOOL, NULL),
    SF_ONE(Enemy, kind, "EnemyKind", SF_INT, NULL),
    SF_ONE(Enemy, pos, "Vector3", SF_BYTES, NULL),
    SF_ONE(Enemy, home, "Vector3", SF_BYTES, NULL),
    SF_ONE(Enemy, wander_to, "Vector3", SF_BYTES, NULL),
    SF_ONE(Enemy, yaw, "float", SF_FLOAT, NULL),
    SF_ONE(Enemy, speed, "float", SF_FLOAT, NULL),
    SF_ONE(Enemy, h, "Health", SF_STRUCT, &SF_T_Health),
    SF_ONE(Enemy, state, "EnemyState", SF_INT, NULL),
    SF_ONE(Enemy, timer, "float", SF_FLOAT, NULL),
    SF_ONE(Enemy, cooldown, "float", SF_FLOAT, NULL),
    SF_ONE(Enemy, attack_anim, "float", SF_FLOAT, NULL),
    SF_ONE(Enemy, hit_anim, "float", SF_FLOAT, NULL),
    SF_ONE(Enemy, corpse, "float", SF_FLOAT, NULL),
    SF_ONE(Enemy, shown, "float", SF_FLOAT, NULL),
    SF_ONE(Enemy, reload, "float", SF_FLOAT, NULL),
    SF_ONE(Enemy, target, "int", SF_INT, NULL),
    SF_ONE(Enemy, armor, "Armor", SF_STRUCT, &SF_T_Armor),
    SF_ONE(Enemy, shield, "bool", SF_BOOL, NULL),
    SF_ONE(Enemy, armed, "bool", SF_BOOL, NULL),
    SF_ONE(Enemy, blocking, "bool", SF_BOOL, NULL),
    SF_ONE(Enemy, block_timer, "float", SF_FLOAT, NULL),
    SF_ONE(Enemy, stagger, "float", SF_FLOAT, NULL),
    SF_ONE(Enemy, knock, "float", SF_FLOAT, NULL),
    SF_ONE(Enemy, move, "int", SF_INT, NULL),
    SF_ONE(Enemy, combo, "int", SF_INT, NULL),
    SF_ONE(Enemy, move_anim, "float", SF_FLOAT, NULL),
    SF_ONE(Enemy, mounted, "bool", SF_BOOL, NULL),
    SF_ONE(Enemy, loot_dropped, "bool", SF_BOOL, NULL),
    SF_ONE(Enemy, awed, "bool", SF_BOOL, NULL),
};
static const SfType SF_T_Enemy = SF_TYPE(Enemy, SF_F_Enemy);

// V3 (src/sim/body.h)
static const SfField SF_F_V3[] = {
    SF_ONE(V3, x, "float", SF_FLOAT, NULL),
    SF_ONE(V3, y, "float", SF_FLOAT, NULL),
    SF_ONE(V3, z, "float", SF_FLOAT, NULL),
};
static const SfType SF_T_V3 = SF_TYPE(V3, SF_F_V3);

// Projectile (src/sim/ballistics.h)
static const SfField SF_F_Projectile[] = {
    SF_ONE(Projectile, pos, "V3", SF_STRUCT, &SF_T_V3),
    SF_ONE(Projectile, vel, "V3", SF_STRUCT, &SF_T_V3),
    SF_ONE(Projectile, kind, "ProjectileKind", SF_INT, NULL),
    SF_ONE(Projectile, alive, "bool", SF_BOOL, NULL),
};
static const SfType SF_T_Projectile = SF_TYPE(Projectile, SF_F_Projectile);

// Shot (src/game/combat_game.h)
static const SfField SF_F_Shot[] = {
    SF_ONE(Shot, p, "Projectile", SF_STRUCT, &SF_T_Projectile),
    SF_ONE(Shot, owner, "int", SF_INT, NULL),
    SF_ONE(Shot, life, "float", SF_FLOAT, NULL),
    SF_ONE(Shot, stuck, "bool", SF_BOOL, NULL),
    SF_ONE(Shot, dir, "V3", SF_STRUCT, &SF_T_V3),
    SF_ONE(Shot, burning, "bool", SF_BOOL, NULL),
};
static const SfType SF_T_Shot = SF_TYPE(Shot, SF_F_Shot);

// Combat (src/game/combat_game.h)
static const SfField SF_F_Combat[] = {
    SF_ONE(Combat, seed, "unsigned", SF_UINT, NULL),
    SF_ONE(Combat, rng, "Rng", SF_STRUCT, &SF_T_Rng),
    SF_ONE(Combat, player, "Health", SF_STRUCT, &SF_T_Health),
    SF_ARR(Combat, enemies, "Enemy", SF_STRUCT, &SF_T_Enemy),
    SF_ONE(Combat, attack_cd, "float", SF_FLOAT, NULL),
    SF_ONE(Combat, attack_anim, "float", SF_FLOAT, NULL),
    SF_ONE(Combat, hit_anim, "float", SF_FLOAT, NULL),
    SF_ONE(Combat, combo, "int", SF_INT, NULL),
    SF_ONE(Combat, blocking, "bool", SF_BOOL, NULL),
    SF_ONE(Combat, spawn_timer, "float", SF_FLOAT, NULL),
    SF_ONE(Combat, down_timer, "float", SF_FLOAT, NULL),
    SF_ONE(Combat, healer_timer, "float", SF_FLOAT, NULL),
    SF_ARR(Combat, comp_cd, "float", SF_FLOAT, NULL),
    SF_ONE(Combat, frost_timer, "float", SF_FLOAT, NULL),
    SF_ONE(Combat, show_panel, "bool", SF_BOOL, NULL),
    SF_ONE(Combat, armor, "Armor", SF_STRUCT, &SF_T_Armor),
    SF_ARR(Combat, shots, "Shot", SF_STRUCT, &SF_T_Shot),
    SF_ONE(Combat, aiming, "bool", SF_BOOL, NULL),
    SF_ONE(Combat, draw, "float", SF_FLOAT, NULL),
    SF_ONE(Combat, reload, "float", SF_FLOAT, NULL),
    SF_ONE(Combat, aim_pitch, "float", SF_FLOAT, NULL),
    SF_ONE(Combat, ammo_shown, "int", SF_INT, NULL),
    SF_ONE(Combat, arrow_lit, "bool", SF_BOOL, NULL),
    SF_ONE(Combat, arrow_lit_timer, "float", SF_FLOAT, NULL),
    SF_ONE(Combat, v_hold, "float", SF_FLOAT, NULL),
    SF_ONE(Combat, combo_timer, "float", SF_FLOAT, NULL),
    SF_ONE(Combat, move, "int", SF_INT, NULL),
    SF_ONE(Combat, move_anim, "float", SF_FLOAT, NULL),
    SF_ONE(Combat, stagger, "float", SF_FLOAT, NULL),
    SF_ONE(Combat, knock, "float", SF_FLOAT, NULL),
    SF_ONE(Combat, charge_timer, "float", SF_FLOAT, NULL),
    SF_ONE(Combat, charge_hit, "bool", SF_BOOL, NULL),
    SF_ONE(Combat, last_pos, "Vector3", SF_BYTES, NULL),
    SF_ONE(Combat, pl_speed, "float", SF_FLOAT, NULL),
    SF_ONE(Combat, trample_cd, "float", SF_FLOAT, NULL),
    SF_ARR(Combat, loose_horse, "Vector3", SF_BYTES, NULL),
    SF_ARR(Combat, loose_yaw, "float", SF_FLOAT, NULL),
    SF_ONE(Combat, loose_n, "int", SF_INT, NULL),
};
static const SfType SF_T_Combat = SF_TYPE(Combat, SF_F_Combat);

// Warmth (src/sim/hazards.h)
static const SfField SF_F_Warmth[] = {
    SF_ONE(Warmth, heat, "float", SF_FLOAT, NULL),
    SF_ONE(Warmth, wet, "float", SF_FLOAT, NULL),
};
static const SfType SF_T_Warmth = SF_TYPE(Warmth, SF_F_Warmth);

// HeatStress (src/sim/apparel.h)
static const SfField SF_F_HeatStress[] = {
    SF_ONE(HeatStress, load, "float", SF_FLOAT, NULL),
};
static const SfType SF_T_HeatStress = SF_TYPE(HeatStress, SF_F_HeatStress);

// Qte (src/sim/hazards.h)
static const SfField SF_F_Qte[] = {
    SF_ARR(Qte, keys, "int", SF_INT, NULL),
    SF_ONE(Qte, len, "int", SF_INT, NULL),
    SF_ONE(Qte, pos, "int", SF_INT, NULL),
    SF_ONE(Qte, per_key, "float", SF_FLOAT, NULL),
    SF_ONE(Qte, time_left, "float", SF_FLOAT, NULL),
    SF_ONE(Qte, mistakes, "int", SF_INT, NULL),
    SF_ONE(Qte, max_mistakes, "int", SF_INT, NULL),
    SF_ONE(Qte, state, "QteState", SF_INT, NULL),
};
static const SfType SF_T_Qte = SF_TYPE(Qte, SF_F_Qte);

// Hazards (src/game/hazards_game.h)
static const SfField SF_F_Hazards[] = {
    SF_ONE(Hazards, seed, "unsigned", SF_UINT, NULL),
    SF_ONE(Hazards, rng, "Rng", SF_STRUCT, &SF_T_Rng),
    SF_ONE(Hazards, warmth, "Warmth", SF_STRUCT, &SF_T_Warmth),
    SF_ONE(Hazards, faint_timer, "float", SF_FLOAT, NULL),
    SF_ONE(Hazards, feels, "float", SF_FLOAT, NULL),
    SF_ONE(Hazards, fire, "float", SF_FLOAT, NULL),
    SF_ONE(Hazards, heat, "HeatStress", SF_STRUCT, &SF_T_HeatStress),
    SF_ONE(Hazards, sun, "float", SF_FLOAT, NULL),
    SF_ONE(Hazards, temp_here, "float", SF_FLOAT, NULL),
    SF_ONE(Hazards, heat_faint, "float", SF_FLOAT, NULL),
    SF_ONE(Hazards, outfit_speed, "float", SF_FLOAT, NULL),
    SF_ONE(Hazards, on_ice, "bool", SF_BOOL, NULL),
    SF_ONE(Hazards, wading, "bool", SF_BOOL, NULL),
    SF_ONE(Hazards, swimming, "bool", SF_BOOL, NULL),
    SF_ONE(Hazards, mud, "float", SF_FLOAT, NULL),
    SF_ONE(Hazards, danger, "bool", SF_BOOL, NULL),
    SF_ONE(Hazards, danger_pos, "Vector3", SF_BYTES, NULL),
    SF_ONE(Hazards, danger_radius, "float", SF_FLOAT, NULL),
    SF_ONE(Hazards, danger_kind, "SinkKind", SF_INT, NULL),
    SF_ONE(Hazards, trap, "TrapKind", SF_INT, NULL),
    SF_ONE(Hazards, trap_pos, "Vector3", SF_BYTES, NULL),
    SF_ONE(Hazards, qte, "Qte", SF_STRUCT, &SF_T_Qte),
    SF_ONE(Hazards, qte_rescue, "bool", SF_BOOL, NULL),
    SF_ONE(Hazards, helper, "int", SF_INT, NULL),
    SF_ONE(Hazards, sink_depth, "float", SF_FLOAT, NULL),
    SF_ARR(Hazards, holes, "Vector3", SF_BYTES, NULL),
    SF_ONE(Hazards, hole_count, "int", SF_INT, NULL),
    SF_ONE(Hazards, hole_day, "int", SF_INT, NULL),
    SF_ONE(Hazards, used_x, "float", SF_FLOAT, NULL),
    SF_ONE(Hazards, used_z, "float", SF_FLOAT, NULL),
    SF_ONE(Hazards, used_day, "int", SF_INT, NULL),
    SF_ONE(Hazards, victim, "int", SF_INT, NULL),
    SF_ONE(Hazards, victim_trap, "TrapKind", SF_INT, NULL),
    SF_ONE(Hazards, victim_timer, "float", SF_FLOAT, NULL),
    SF_ONE(Hazards, escort_on, "bool", SF_BOOL, NULL),
    SF_ONE(Hazards, ice_roll, "float", SF_FLOAT, NULL),
    // body: puntero, no se guarda
    SF_ONE(Hazards, frost_timer, "float", SF_FLOAT, NULL),
};
static const SfType SF_T_Hazards = SF_TYPE(Hazards, SF_F_Hazards);

// FireCell (src/sim/fire.h)
static const SfField SF_F_FireCell[] = {
    SF_ONE(FireCell, used, "bool", SF_BOOL, NULL),
    SF_ONE(FireCell, x, "float", SF_FLOAT, NULL),
    SF_ONE(FireCell, z, "float", SF_FLOAT, NULL),
    SF_ONE(FireCell, heat, "float", SF_FLOAT, NULL),
    SF_ONE(FireCell, fuel, "float", SF_FLOAT, NULL),
    SF_ONE(FireCell, kind, "FuelKind", SF_INT, NULL),
    SF_ONE(FireCell, ref, "int", SF_INT, NULL),
    SF_ONE(FireCell, age, "float", SF_FLOAT, NULL),
};
static const SfType SF_T_FireCell = SF_TYPE(FireCell, SF_F_FireCell);

// Scorch (src/sim/fire.h)
static const SfField SF_F_Scorch[] = {
    SF_ONE(Scorch, x, "float", SF_FLOAT, NULL),
    SF_ONE(Scorch, z, "float", SF_FLOAT, NULL),
    SF_ONE(Scorch, r, "float", SF_FLOAT, NULL),
    SF_ONE(Scorch, age, "float", SF_FLOAT, NULL),
};
static const SfType SF_T_Scorch = SF_TYPE(Scorch, SF_F_Scorch);

// FireField (src/sim/fire.h)
static const SfField SF_F_FireField[] = {
    SF_ARR(FireField, cells, "FireCell", SF_STRUCT, &SF_T_FireCell),
    SF_ARR(FireField, scorch, "Scorch", SF_STRUCT, &SF_T_Scorch),
    SF_ONE(FireField, scorch_next, "int", SF_INT, NULL),
    SF_ONE(FireField, rng, "Rng", SF_STRUCT, &SF_T_Rng),
    SF_ONE(FireField, spread_timer, "float", SF_FLOAT, NULL),
};
static const SfType SF_T_FireField = SF_TYPE(FireField, SF_F_FireField);

// Disasters (src/game/disasters_game.h)
static const SfField SF_F_Disasters[] = {
    SF_ONE(Disasters, fire, "FireField", SF_STRUCT, &SF_T_FireField),
    SF_ONE(Disasters, rng, "Rng", SF_STRUCT, &SF_T_Rng),
    SF_ARR(Disasters, tree_burn, "float", SF_FLOAT, NULL),
    SF_ONE(Disasters, bolt_time, "float", SF_FLOAT, NULL),
    SF_ONE(Disasters, bolt_to, "Vector3", SF_BYTES, NULL),
    SF_ONE(Disasters, bolt_seed, "float", SF_FLOAT, NULL),
    SF_ONE(Disasters, relight_timer, "float", SF_FLOAT, NULL),
    SF_ONE(Disasters, burn_tick, "float", SF_FLOAT, NULL),
    SF_ONE(Disasters, warn_timer, "float", SF_FLOAT, NULL),
    SF_ONE(Disasters, camp_warned, "bool", SF_BOOL, NULL),
};
static const SfType SF_T_Disasters = SF_TYPE(Disasters, SF_F_Disasters);

// SavedProp (src/game/save_game.h)
static const SfField SF_F_SavedProp[] = {
    SF_ARR(SavedProp, id, "char", SF_TEXT, NULL),
    SF_ONE(SavedProp, pos, "Vector3", SF_BYTES, NULL),
    SF_ONE(SavedProp, yaw, "float", SF_FLOAT, NULL),
    SF_ONE(SavedProp, condition, "float", SF_FLOAT, NULL),
};
static const SfType SF_T_SavedProp = SF_TYPE(SavedProp, SF_F_SavedProp);

// MemoryCell (src/sim/memory_map.h)
static const SfField SF_F_MemoryCell[] = {
    SF_ONE(MemoryCell, light, "float", SF_FLOAT, NULL),
    SF_ONE(MemoryCell, familiarity, "float", SF_FLOAT, NULL),
    SF_ONE(MemoryCell, t_last, "float", SF_FLOAT, NULL),
};
static const SfType SF_T_MemoryCell = SF_TYPE(MemoryCell, SF_F_MemoryCell);

// MemoryPage (src/sim/memory_map.h)
static const SfField SF_F_MemoryPage[] = {
    SF_ONE(MemoryPage, px, "int", SF_INT, NULL),
    SF_ONE(MemoryPage, pz, "int", SF_INT, NULL),
    SF_ARR(MemoryPage, cells, "MemoryCell", SF_STRUCT, &SF_T_MemoryCell),
};
static const SfType SF_T_MemoryPage = SF_TYPE(MemoryPage, SF_F_MemoryPage);

// MapMarker (src/sim/memory_map.h)
static const SfField SF_F_MapMarker[] = {
    SF_ONE(MapMarker, x, "float", SF_FLOAT, NULL),
    SF_ONE(MapMarker, z, "float", SF_FLOAT, NULL),
    SF_ONE(MapMarker, kind, "MarkerKind", SF_INT, NULL),
};
static const SfType SF_T_MapMarker = SF_TYPE(MapMarker, SF_F_MapMarker);

// Remains (src/game/death_game.h)
static const SfField SF_F_Remains[] = {
    SF_ONE(Remains, x, "float", SF_FLOAT, NULL),
    SF_ONE(Remains, y, "float", SF_FLOAT, NULL),
    SF_ONE(Remains, z, "float", SF_FLOAT, NULL),
    SF_ONE(Remains, yaw, "float", SF_FLOAT, NULL),
};
static const SfType SF_T_Remains = SF_TYPE(Remains, SF_F_Remains);

// DeathSave (src/game/death_game.h)
static const SfField SF_F_DeathSave[] = {
    SF_ONE(DeathSave, has_rest, "int", SF_INT, NULL),
    SF_ONE(DeathSave, rest_x, "float", SF_FLOAT, NULL),
    SF_ONE(DeathSave, rest_z, "float", SF_FLOAT, NULL),
    SF_ARR(DeathSave, rest_name, "char", SF_TEXT, NULL),
    SF_ONE(DeathSave, deaths, "int", SF_INT, NULL),
    SF_ONE(DeathSave, count, "int", SF_INT, NULL),
    SF_ONE(DeathSave, next, "int", SF_INT, NULL),
    SF_ARR(DeathSave, remains, "Remains", SF_STRUCT, &SF_T_Remains),
};
static const SfType SF_T_DeathSave = SF_TYPE(DeathSave, SF_F_DeathSave);

// QbSlot (src/game/hud_game.h)
static const SfField SF_F_QbSlot[] = {
    SF_ONE(QbSlot, kind, "int", SF_INT, NULL),
    SF_ONE(QbSlot, grip, "int", SF_INT, NULL),
    SF_ARR(QbSlot, id, "char", SF_TEXT, NULL),
};
static const SfType SF_T_QbSlot = SF_TYPE(QbSlot, SF_F_QbSlot);
