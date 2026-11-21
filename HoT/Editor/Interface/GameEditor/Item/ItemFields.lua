-- Item editor field data, names follow HoTEmulator ItemTemplate.h, SharedDefines.h and VirtualItemMgr.h.
GameEditorData = GameEditorData or {}
GameEditorData.Fields = GameEditorData.Fields or {}
GameEditorData.Groups = GameEditorData.Groups or {}
GameEditorData.Enums = GameEditorData.Enums or {}
GameEditorData.Flags = GameEditorData.Flags or {}

local Enums = GameEditorData.Enums
local Flags = GameEditorData.Flags

-- ItemTemplate.h ItemModType, 49 and up plus the 5000 resist slots are HoT stats
Enums.ItemModType = {
    [0] = "Mana",
    [1] = "Health",
    [3] = "Agility",
    [4] = "Strength",
    [5] = "Intellect",
    [6] = "Spirit",
    [7] = "Stamina",
    [12] = "Defense Rating",
    [13] = "Dodge Rating",
    [14] = "Parry Rating",
    [15] = "Block Rating",
    [16] = "Melee Hit Rating",
    [17] = "Ranged Hit Rating",
    [18] = "Spell Hit Rating",
    [19] = "Melee Crit Rating",
    [20] = "Ranged Crit Rating",
    [21] = "Spell Crit Rating",
    [22] = "Melee Hit Taken Rating",
    [23] = "Ranged Hit Taken Rating",
    [24] = "Spell Hit Taken Rating",
    [25] = "Melee Crit Taken Rating",
    [26] = "Ranged Crit Taken Rating",
    [27] = "Spell Crit Taken Rating",
    [28] = "Melee Haste Rating",
    [29] = "Ranged Haste Rating",
    [30] = "Spell Haste Rating",
    [31] = "Hit Rating",
    [32] = "Crit Rating",
    [33] = "Hit Taken Rating",
    [34] = "Crit Taken Rating",
    [35] = "Resilience Rating",
    [36] = "Haste Rating",
    [37] = "Expertise Rating",
    [38] = "Attack Power",
    [39] = "Ranged Attack Power",
    [41] = "Spell Healing (old)",
    [42] = "Spell Damage (old)",
    [43] = "Mana Regeneration",
    [44] = "Armor Penetration Rating",
    [45] = "Spell Power",
    [46] = "Health Regeneration",
    [47] = "Spell Penetration",
    [48] = "Block Value",
    [49] = "Magic Find",
    [50] = "Leech",
    [51] = "Mana Leech",
    [52] = "Crit Damage",
    [53] = "Crit Healing",
    [54] = "Focus",
    [55] = "Focus Percent",
    [56] = "Attack Power Percent",
    [57] = "Spell Damage Percent",
    [58] = "Healing Percent",
    [59] = "Strength Percent",
    [60] = "Agility Percent",
    [61] = "Stamina Percent",
    [62] = "Intellect Percent",
    [63] = "Spirit Percent",
    [64] = "All Stats Percent",
    [65] = "Physical Damage Reduction",
    [66] = "Magic Damage Reduction",
    [67] = "Weapon Damage",
    [68] = "Weapon Damage Percent",
    [69] = "Armor Percent",
    [70] = "Additional Socket",
    [71] = "Movement Speed",
    [72] = "Potion Effect",
    [73] = "All Primary Stats",
    [74] = "Thorns",
    [5000] = "Holy Resistance (generator)",
    [5001] = "Fire Resistance (generator)",
    [5002] = "Nature Resistance (generator)",
    [5003] = "Frost Resistance (generator)",
    [5004] = "Shadow Resistance (generator)",
    [5005] = "Arcane Resistance (generator)",
    [5006] = "Random Resistance (generator)",
}

-- ItemTemplate.h ItemSpelltriggerType
Enums.ItemSpellTrigger = {
    [0] = "On Use",
    [1] = "On Equip",
    [2] = "Chance on Hit",
    [3] = "On Socket (display only)",
    [4] = "Soulstone",
    [5] = "On Use, no equip delay",
    [6] = "Learn Spell",
}

-- ItemTemplate.h ItemBondingType
Enums.ItemBonding = {
    [0] = "No Bind",
    [1] = "Bind on Pickup",
    [2] = "Bind on Equip",
    [3] = "Bind on Use",
    [4] = "Quest Item",
    [5] = "Quest Item (unused)",
}

Enums.ItemGenBonding = {
    [-1] = "Generator default (bind on pickup)",
    [0] = "No Bind",
    [1] = "Bind on Pickup",
    [2] = "Bind on Equip",
    [3] = "Bind on Use",
    [4] = "Quest Item",
}

-- SharedDefines.h SheathTypes
Enums.ItemSheath = {
    [0] = "None",
    [1] = "Main Hand",
    [2] = "Off Hand",
    [3] = "Large Weapon Left",
    [4] = "Large Weapon Right",
    [5] = "Hip Weapon Left",
    [6] = "Hip Weapon Right",
    [7] = "Shield",
}

-- Material.dbc
Enums.ItemMaterial = {
    [-1] = "Consumable",
    [0] = "Undefined",
    [1] = "Metal",
    [2] = "Wood",
    [3] = "Liquid",
    [4] = "Jewelry",
    [5] = "Chain",
    [6] = "Plate",
    [7] = "Cloth",
    [8] = "Leather",
}

-- ItemTemplate.h SocketColor, 14 is the prismatic socket the client knows
Enums.ItemSocketColor = {
    [0] = "No Socket",
    [1] = "Meta",
    [2] = "Red",
    [4] = "Yellow",
    [8] = "Blue",
    [14] = "Prismatic",
    [16] = "Orange (Major)",
    [32] = "Purple (Minor)",
    [64] = "Green (Fatigue)",
}

Enums.ItemGenSocketColor = {
    [-1] = "Keep the base item socket",
    [0] = "No Socket",
    [1] = "Meta",
    [2] = "Red",
    [4] = "Yellow",
    [8] = "Blue",
    [14] = "Prismatic",
    [16] = "Orange (Major)",
    [32] = "Purple (Minor)",
    [64] = "Green (Fatigue)",
}

-- SharedDefines.h ReputationRank
Enums.ItemReputationRank = {
    [0] = "Hated",
    [1] = "Hostile",
    [2] = "Unfriendly",
    [3] = "Neutral",
    [4] = "Friendly",
    [5] = "Honored",
    [6] = "Revered",
    [7] = "Exalted",
}

-- SharedDefines.h PetDiet
Enums.ItemFoodType = {
    [0] = "None",
    [1] = "Meat",
    [2] = "Fish",
    [3] = "Cheese",
    [4] = "Bread",
    [5] = "Fungus",
    [6] = "Fruit",
    [7] = "Raw Meat",
    [8] = "Raw Fish",
}

Enums.ItemAmmoType = {
    [0] = "None",
    [2] = "Arrows",
    [3] = "Bullets",
}

Enums.ItemLocale = {
    koKR = "Korean",
    frFR = "French",
    deDE = "German",
    zhCN = "Chinese (simplified)",
    zhTW = "Chinese (traditional)",
    esES = "Spanish (Spain)",
    esMX = "Spanish (Mexico)",
    ruRU = "Russian",
}

-- item_generator_stat_group_stats.type, VirtualItemMgr.h StatGroupType
Enums.ItemGenStatListType = {
    [0] = "Primary stat",
    [1] = "Secondary stat",
    [2] = "Socket color",
}

-- item_generator_names.array_id, the word lists GenerateItemName picks from
Enums.ItemGenNameList = {
    [1] = "List 1: unique names (Malice, Mercy)",
    [2] = "List 2: prefixes (Arcane, Bloodied)",
    [3] = "List 3: materials (Bone, Copper)",
    [4] = "List 4: basic type names (Blade, Maul)",
    [5] = "List 5: exotic type names",
    [6] = "List 6: suffixes (of Agony)",
    [7] = "List 7: legendary suffix words",
    [8] = "List 8: normal quality words",
}

Enums.ItemGenAnyClass = {
    [-2] = "Any class, not trinkets",
    [-1] = "Any",
    [2] = "Weapon",
    [4] = "Armor",
}

Enums.ItemGenAnyQuality = {
    [-1] = "Any",
    [0] = "Poor",
    [1] = "Normal",
    [2] = "Uncommon",
    [3] = "Rare",
    [4] = "Epic",
    [5] = "Legendary",
    [6] = "Artifact",
    [7] = "Heirloom",
}

Enums.ItemGenAnyInventoryType = { [-1] = "Any" }
Enums.ItemGenStatGroupRandom = { [-1] = "Random" }

for value, name in pairs(Enums.InventoryType or {}) do
    Enums.ItemGenAnyInventoryType[value] = name
end

-- ItemTemplate.h ItemFlags
Flags.ItemFlags = {
    {bit = 0, name = "NO_PICKUP"},
    {bit = 1, name = "CONJURED", tooltip = "Conjured item, removed when the player logs out for a while"},
    {bit = 2, name = "HAS_LOOT", tooltip = "Right click opens it for loot"},
    {bit = 3, name = "HEROIC_TOOLTIP", tooltip = "Green Heroic line in the tooltip, also needed for the HoT tooltip labels of FlagsExtra"},
    {bit = 4, name = "DEPRECATED", tooltip = "Can not be equipped or used"},
    {bit = 5, name = "NO_USER_DESTROY", tooltip = "Can not be destroyed by the player"},
    {bit = 6, name = "PLAYERCAST", tooltip = "The item spells are cast by the player"},
    {bit = 7, name = "NO_EQUIP_COOLDOWN", tooltip = "No 30 second cooldown after equipping"},
    {bit = 8, name = "MULTI_LOOT_QUEST"},
    {bit = 9, name = "IS_WRAPPER", tooltip = "Can wrap other items"},
    {bit = 10, name = "USES_RESOURCES"},
    {bit = 11, name = "MULTI_DROP", tooltip = "Looting does not remove it from the loot"},
    {bit = 12, name = "ITEM_PURCHASE_RECORD", tooltip = "Can be returned to the vendor for its extended cost"},
    {bit = 13, name = "PETITION", tooltip = "Guild or arena charter"},
    {bit = 14, name = "HAS_TEXT", tooltip = "Readable item"},
    {bit = 15, name = "NO_DISENCHANT"},
    {bit = 16, name = "REAL_DURATION"},
    {bit = 17, name = "NO_CREATOR"},
    {bit = 18, name = "IS_PROSPECTABLE"},
    {bit = 19, name = "UNIQUE_EQUIPPABLE", tooltip = "Only one can be equipped"},
    {bit = 20, name = "IGNORE_FOR_AURAS"},
    {bit = 21, name = "IGNORE_DEFAULT_ARENA_RESTRICTIONS", tooltip = "Usable in arena matches"},
    {bit = 22, name = "NO_DURABILITY_LOSS"},
    {bit = 23, name = "USE_WHEN_SHAPESHIFTED"},
    {bit = 24, name = "HAS_QUEST_GLOW"},
    {bit = 25, name = "HIDE_UNUSABLE_RECIPE"},
    {bit = 26, name = "NOT_USEABLE_IN_ARENA"},
    {bit = 27, name = "IS_BOUND_TO_ACCOUNT", tooltip = "Binds to the account"},
    {bit = 28, name = "NO_REAGENT_COST", tooltip = "The item spell ignores reagents"},
    {bit = 29, name = "IS_MILLABLE"},
    {bit = 30, name = "REPORT_TO_GUILD_CHAT"},
    {bit = 31, name = "NO_PROGRESSIVE_LOOT"},
}

-- ItemTemplate.h ItemFlags2, item_template.FlagsExtra
Flags.ItemFlags2 = {
    {bit = 0, name = "FACTION_HORDE", tooltip = "Horde only"},
    {bit = 1, name = "FACTION_ALLIANCE", tooltip = "Alliance only"},
    {bit = 2, name = "DONT_IGNORE_BUY_PRICE", tooltip = "Gold is needed on top of the extended cost"},
    {bit = 3, name = "CLASSIFY_AS_CASTER"},
    {bit = 4, name = "CLASSIFY_AS_PHYSICAL"},
    {bit = 5, name = "EVERYONE_CAN_ROLL_NEED"},
    {bit = 6, name = "NO_TRADE_BIND_ON_ACQUIRE"},
    {bit = 7, name = "CAN_TRADE_BIND_ON_ACQUIRE"},
    {bit = 8, name = "CAN_ONLY_ROLL_GREED"},
    {bit = 9, name = "CASTER_WEAPON"},
    {bit = 10, name = "DELETE_ON_LOGIN"},
    {bit = 11, name = "INTERNAL_ITEM"},
    {bit = 12, name = "NO_VENDOR_VALUE"},
    {bit = 13, name = "SHOW_BEFORE_DISCOVERED"},
    {bit = 14, name = "OVERRIDE_GOLD_COST"},
    {bit = 15, name = "IGNORE_DEFAULT_RATED_BG_RESTRICTIONS"},
    {bit = 16, name = "NOT_USABLE_IN_RATED_BG"},
    {bit = 17, name = "BNET_ACCOUNT_TRADE_OK"},
    {bit = 18, name = "CONFIRM_BEFORE_USE"},
    {bit = 19, name = "REEVALUATE_BONDING_ON_TRANSFORM"},
    {bit = 20, name = "NO_TRANSFORM_ON_CHARGE_DEPLETION"},
    {bit = 21, name = "NO_ALTER_ITEM_VISUAL"},
    {bit = 22, name = "NO_SOURCE_FOR_ITEM_VISUAL"},
    {bit = 23, name = "IGNORE_QUALITY_FOR_ITEM_VISUAL_SOURCE"},
    {bit = 24, name = "NO_DURABILITY"},
    {bit = 25, name = "ROLE_TANK"},
    {bit = 26, name = "ROLE_HEALER"},
    {bit = 27, name = "ROLE_DAMAGE"},
    {bit = 28, name = "PARADOXICAL", tooltip = "HoT tooltip label, needs HEROIC_TOOLTIP in Flags"},
    {bit = 29, name = "HEIRLOOM", tooltip = "HoT tooltip label, needs HEROIC_TOOLTIP in Flags"},
    {bit = 30, name = "LEGENDARY", tooltip = "HoT tooltip label, needs HEROIC_TOOLTIP in Flags"},
    {bit = 31, name = "CORRUPTED", tooltip = "HoT tooltip label, needs HEROIC_TOOLTIP in Flags"},
}

-- ItemTemplate.h ItemFlagsCustom
Flags.ItemFlagsCustom = {
    {bit = 0, name = "DURATION_REAL_TIME", tooltip = "The duration also runs while the player is offline"},
    {bit = 1, name = "IGNORE_QUEST_STATUS", tooltip = "Drops without checking the quest status"},
    {bit = 2, name = "FOLLOW_LOOT_RULES", tooltip = "Always follows the group loot rules"},
    {bit = 3, name = "VIRTUAL_ITEM_BASE", tooltip = "Base of a generated item, adding it to a player rolls a new virtual item"},
    {bit = 4, name = "VIRT_CLASS_WARDEN", tooltip = "Generated items roll the stat group that claims this class flag"},
    {bit = 5, name = "VIRT_CLASS_HISTORIAN", tooltip = "Generated items roll the stat group that claims this class flag"},
    {bit = 6, name = "VIRT_CLASS_WEAVER", tooltip = "Generated items roll the stat group that claims this class flag"},
    {bit = 7, name = "VIRT_CLASS_WATCHER", tooltip = "Generated items roll the stat group that claims this class flag"},
    {bit = 8, name = "VIRT_CLASS_RANGER", tooltip = "Generated items roll the stat group that claims this class flag"},
    {bit = 9, name = "VIRT_RANDOM_TYPE", tooltip = "The base rolls its own class, subclass and inventory type from item_generator_item_types"},
}

-- ItemTemplate.h BAG_FAMILY_MASK
Flags.ItemBagFamily = {
    {bit = 0, name = "Arrows"},
    {bit = 1, name = "Bullets"},
    {bit = 2, name = "Soul Shards"},
    {bit = 3, name = "Leatherworking Supplies"},
    {bit = 4, name = "Inscription Supplies"},
    {bit = 5, name = "Herbs"},
    {bit = 6, name = "Enchanting Supplies"},
    {bit = 7, name = "Engineering Supplies"},
    {bit = 8, name = "Keys"},
    {bit = 9, name = "Gems"},
    {bit = 10, name = "Mining Supplies"},
    {bit = 11, name = "Soulbound Equipment"},
    {bit = 12, name = "Vanity Pets"},
    {bit = 13, name = "Currency Tokens", tooltip = "Shows in the currency tab"},
    {bit = 14, name = "Quest Items"},
}

-- SharedDefines.h Classes, bit = class id - 1
Flags.ItemClassMask = {
    {bit = 0, name = "Warrior"},
    {bit = 1, name = "Paladin"},
    {bit = 2, name = "Hunter"},
    {bit = 3, name = "Rogue"},
    {bit = 4, name = "Priest"},
    {bit = 5, name = "Death Knight"},
    {bit = 6, name = "Shaman"},
    {bit = 7, name = "Mage"},
    {bit = 8, name = "Warlock"},
    {bit = 10, name = "Druid"},
    {bit = 11, name = "Timewalker"},
    {bit = 12, name = "Warden"},
    {bit = 13, name = "Historian"},
    {bit = 14, name = "Weaver"},
    {bit = 15, name = "Watcher"},
    {bit = 16, name = "Ranger"},
    {bit = 17, name = "Savage"},
}

-- VirtualItemMgr.h StatGroupFlags
Flags.ItemGenStatGroupFlags = {
    {bit = 0, name = "ROLLABLE", tooltip = "A random roll may pick it, clearing this switches the group off"},
    {bit = 1, name = "TRINKET_ONLY", tooltip = "Only reached as the trinket group of another group"},
    {bit = 2, name = "POOL", tooltip = "Internal stat pool, never given to an item"},
}

-- item_generator_stat_group_mask names, VirtualItemMgr.h StatGroupGroup
Flags.ItemGenStatGroupMask = {
    {bit = 0, name = "Intellect Healer"},
    {bit = 1, name = "Intellect DPS"},
    {bit = 2, name = "Spirit DPS"},
    {bit = 3, name = "Spirit Healer"},
    {bit = 4, name = "Strength DPS"},
    {bit = 5, name = "Agility DPS"},
    {bit = 6, name = "Strength Parry Tank"},
    {bit = 7, name = "Strength Block Tank"},
    {bit = 8, name = "Agility Dodge Tank"},
    {bit = 9, name = "Agility Block Tank"},
    {bit = 10, name = "Intellect"},
    {bit = 11, name = "Spirit"},
    {bit = 13, name = "Strength"},
    {bit = 14, name = "Agility"},
    {bit = 15, name = "Caster"},
    {bit = 16, name = "Healing"},
    {bit = 17, name = "Melee DPS"},
    {bit = 18, name = "Tank"},
    {bit = 19, name = "Melee"},
    {bit = 20, name = "Ranged"},
    {bit = 21, name = "DPS"},
    {bit = 22, name = "Intellect Tank"},
    {bit = 23, name = "Spirit Tank"},
}

-- VirtualItemMgr.h UniqueItemFlags
Flags.ItemGenUniqueFlags = {
    {bit = 0, name = "NO_HONE", tooltip = "Honing leaves the values of this item alone"},
    {bit = 1, name = "NO_ADDITIONAL_STATS", tooltip = "Extra stat effects can not be applied to it"},
    {bit = 2, name = "IGNORE_STAT_CAPS", tooltip = "Authored values ignore even the hard stat ceiling"},
}

-- VirtualItemMgr.h LegendaryFlags
Flags.ItemGenLegendaryFlags = {
    {bit = 0, name = "CRAFTED_ONLY", tooltip = "Only rolls on crafted items"},
    {bit = 1, name = "DROP_DISABLED", tooltip = "Only gained through essences"},
    {bit = 2, name = "TIME_WARPED", tooltip = "Only gained by Time Warping an item, the warp then rolls nothing else"},
}

local Fields = {}

Fields.item_template = {
    entry = {label = "Entry", type = "int"},
    name = {label = "Name", type = "string"},
    description = {label = "Description", tooltip = "Yellow flavor text at the bottom of the tooltip", type = "string"},
    Quality = {label = "Quality", type = "int", enum = "ItemQuality"},
    class = {label = "Class", type = "int", enum = "ItemClass"},
    subclass = {label = "Subclass", tooltip = "The list follows the class", type = "int", enum = "ItemSubClass_0"},
    SoundOverrideSubclass = {label = "Sound Subclass", tooltip = "Weapon subclass whose sounds the item uses, -1 uses its own subclass", type = "int"},
    InventoryType = {label = "Inventory Type", tooltip = "Equipment slot, Non Equip for items that can not be worn", type = "int", enum = "InventoryType"},
    displayid = {label = "Display ID", tooltip = "ItemDisplayInfo.dbc id, it sets the icon and the model", type = "int", ref = "dbc:ItemDisplayInfo"},
    sheath = {label = "Sheath", tooltip = "Where the weapon hangs when sheathed", type = "int", enum = "ItemSheath"},
    Material = {label = "Material", tooltip = "Material.dbc, picks the sounds when the item is moved", type = "int", enum = "ItemMaterial"},
    ItemLevel = {label = "Item Level", type = "int"},
    RequiredLevel = {label = "Required Level", type = "int"},
    bonding = {label = "Binding", type = "int", enum = "ItemBonding"},
    Flags = {label = "Flags", type = "int", flags = "ItemFlags"},
    FlagsExtra = {label = "Flags Extra", tooltip = "ItemFlags2", type = "int", flags = "ItemFlags2"},
    flagsCustom = {label = "Custom Flags", tooltip = "Server side HoT flags", type = "int", flags = "ItemFlagsCustom"},
    uniqueId = {label = "Unique Item", tooltip = "item_generator_unique id, the authored item this template stands for. 0 is a normal item", type = "int", ref = "itemgen_unique"},
    BuyCount = {label = "Buy Count", tooltip = "Stack size a vendor sells at once", type = "int"},
    BuyPrice = {label = "Buy Price", tooltip = "Copper a vendor asks", type = "int"},
    SellPrice = {label = "Sell Price", tooltip = "Copper a vendor pays, 0 can not be sold", type = "int"},
    maxcount = {label = "Max Count", tooltip = "How many one player may own, 0 means no limit", type = "int"},
    stackable = {label = "Stack Size", tooltip = "Items per stack", type = "int"},
    ContainerSlots = {label = "Bag Slots", tooltip = "Slots of a bag", type = "int"},
    BagFamily = {label = "Bag Family", tooltip = "Special bags it fits in", type = "int", flags = "ItemBagFamily"},
    StatsCount = {label = "Stats Count", tooltip = "How many stat slots the client reads, set it to the last used slot", type = "int"},
    ScalingStatDistribution = {label = "Scaling Distribution", tooltip = "ScalingStatDistribution.dbc id of heirloom style items", type = "int"},
    ScalingStatValue = {label = "Scaling Value Mask", tooltip = "Column mask of ScalingStatValues.dbc", type = "int"},
    armor = {label = "Armor", type = "int"},
    block = {label = "Block Value", type = "int"},
    ArmorDamageModifier = {label = "Armor Damage Modifier", type = "float"},
    holy_res = {label = "Holy Resistance", type = "int"},
    fire_res = {label = "Fire Resistance", type = "int"},
    nature_res = {label = "Nature Resistance", type = "int"},
    frost_res = {label = "Frost Resistance", type = "int"},
    shadow_res = {label = "Shadow Resistance", type = "int"},
    arcane_res = {label = "Arcane Resistance", type = "int"},
    delay = {label = "Speed (ms)", tooltip = "Weapon swing time in milliseconds, 2600 is a 2.60 speed weapon", type = "int"},
    ammo_type = {label = "Ammo Type", tooltip = "Ammo a ranged weapon uses", type = "int", enum = "ItemAmmoType"},
    RangedModRange = {label = "Ranged Range Mod", tooltip = "Extra range of a ranged weapon, usually 100", type = "float"},
    dmg_min1 = {label = "Damage Min", type = "float"},
    dmg_max1 = {label = "Damage Max", type = "float"},
    dmg_type1 = {label = "Damage School", type = "int", enum = "SpellSchool"},
    dmg_min2 = {label = "Bonus Damage Min", tooltip = "Second damage line, shown as plus N to M school damage", type = "float"},
    dmg_max2 = {label = "Bonus Damage Max", type = "float"},
    dmg_type2 = {label = "Bonus Damage School", type = "int", enum = "SpellSchool"},
    AllowableClass = {label = "Allowed Classes", tooltip = "-1 allows every class", type = "int", flags = "ItemClassMask"},
    AllowableRace = {label = "Allowed Races", tooltip = "-1 allows every race", type = "int", flags = "RaceMask"},
    RequiredSkill = {label = "Required Skill", type = "int", ref = "dbc:SkillLine"},
    RequiredSkillRank = {label = "Required Skill Value", type = "int"},
    requiredspell = {label = "Required Spell", tooltip = "Spell the player must know", type = "int", ref = "spell"},
    requiredhonorrank = {label = "Required Honor Rank", type = "int"},
    RequiredCityRank = {label = "Required City Rank", type = "int"},
    RequiredReputationFaction = {label = "Required Faction", type = "int", ref = "dbc:Faction"},
    RequiredReputationRank = {label = "Required Reputation", type = "int", enum = "ItemReputationRank"},
    ItemLimitCategory = {label = "Limit Category", tooltip = "ItemLimitCategory.dbc id, limits how many of a group a player carries or equips", type = "int", ref = "dbc:ItemLimitCategory"},
    area = {label = "Area", tooltip = "AreaTable id the item can only be used in", type = "int", ref = "dbc:AreaTable"},
    Map = {label = "Map", tooltip = "Map the item can only be used in", type = "int", ref = "dbc:Map"},
    HolidayId = {label = "Holiday", tooltip = "Holidays.dbc id the item needs active", type = "int", ref = "dbc:Holidays"},
    RequiredDisenchantSkill = {label = "Disenchant Skill", tooltip = "Enchanting skill needed to disenchant it, -1 can not be disenchanted", type = "int"},
    DisenchantID = {label = "Disenchant Loot", tooltip = "disenchant_loot_template entry", type = "int"},
    socketBonus = {label = "Socket Bonus", tooltip = "SpellItemEnchantment id granted when every socket matches", type = "int", ref = "dbc:SpellItemEnchantment"},
    GemProperties = {label = "Gem Properties", tooltip = "GemProperties.dbc id when the item is a gem", type = "int", ref = "dbc:GemProperties"},
    PageText = {label = "Page Text", tooltip = "page_text id of a readable item", type = "int"},
    LanguageID = {label = "Language", type = "int", enum = "Language"},
    PageMaterial = {label = "Page Material", tooltip = "PageTextMaterial.dbc background of the text window", type = "int", ref = "dbc:PageTextMaterial"},
    startquest = {label = "Starts Quest", type = "int", ref = "quest"},
    lockid = {label = "Lock", tooltip = "Lock.dbc id of a locked container", type = "int", ref = "dbc:Lock"},
    RandomProperty = {label = "Random Property", tooltip = "item_enchantment_template entry for of the Bear style suffixes", type = "int"},
    RandomSuffix = {label = "Random Suffix", tooltip = "item_enchantment_template entry for scaled suffixes", type = "int"},
    itemset = {label = "Item Set", tooltip = "ItemSet.dbc id", type = "int", ref = "dbc:ItemSet"},
    MaxDurability = {label = "Max Durability", type = "int"},
    TotemCategory = {label = "Totem Category", type = "int", enum = "TotemCategory"},
    duration = {label = "Duration (sec)", tooltip = "The item is removed after this many seconds, 0 lasts forever", type = "int"},
    FoodType = {label = "Pet Food Type", type = "int", enum = "ItemFoodType"},
    minMoneyLoot = {label = "Min Money Loot", tooltip = "Copper inside a container", type = "int"},
    maxMoneyLoot = {label = "Max Money Loot", type = "int"},
    ScriptName = {label = "Script Name", tooltip = "C++ ItemScript name", type = "string"},
    VerifiedBuild = {label = "Verified Build", type = "int"},
}

for slot = 1, 10 do
    Fields.item_template["stat_type"..slot] = {label = "Stat "..slot, type = "int", enum = "ItemModType"}
    Fields.item_template["stat_value"..slot] = {label = "Value "..slot, type = "int"}
end

for slot = 1, 5 do
    Fields.item_template["spellid_"..slot] = {label = "Spell", type = "int", ref = "spell"}
    Fields.item_template["spelltrigger_"..slot] = {label = "Trigger", type = "int", enum = "ItemSpellTrigger"}
    Fields.item_template["spellcharges_"..slot] = {label = "Charges", tooltip = "0 is unlimited, a negative count destroys the item when the charges are used up", type = "int"}
    Fields.item_template["spellppmRate_"..slot] = {label = "Procs Per Minute", tooltip = "Chance on hit rate, 0 uses the spell proc chance", type = "float"}
    Fields.item_template["spellcooldown_"..slot] = {label = "Cooldown (ms)", tooltip = "-1 uses the spell cooldown", type = "int"}
    Fields.item_template["spellcategory_"..slot] = {label = "Category", tooltip = "SpellCategory.dbc id, items in one category share the cooldown", type = "int", ref = "dbc:SpellCategory"}
    Fields.item_template["spellcategorycooldown_"..slot] = {label = "Category Cooldown (ms)", tooltip = "-1 uses the spell category cooldown", type = "int"}
end

for slot = 1, 3 do
    Fields.item_template["socketColor_"..slot] = {label = "Socket "..slot.." Color", type = "int", enum = "ItemSocketColor"}
    Fields.item_template["socketContent_"..slot] = {label = "Socket "..slot.." Content", tooltip = "Not used by the client", type = "int"}
end

Fields.item_template_locale = {
    ID = {label = "Item", type = "int"},
    locale = {label = "Locale", tooltip = "koKR, frFR, deDE, zhCN, zhTW, esES, esMX or ruRU", type = "string"},
    Name = {label = "Name", type = "string"},
    Description = {label = "Description", type = "string"},
    VerifiedBuild = {label = "Verified Build", type = "int"},
}

local RATE = "Multiplier of the stat points, 1 changes nothing"

Fields.item_generator_stat_rate = {
    stat = {label = "Stat", type = "int", enum = "ItemModType"},
    rate = {label = "Rate", tooltip = "Points of this stat per stat budget point. "..RATE, type = "float"},
}

Fields.item_generator_quality_rate = {
    quality = {label = "Quality", type = "int", enum = "ItemQuality"},
    rate = {label = "Rate", tooltip = "Stat budget of this quality. "..RATE, type = "float"},
}

Fields.item_generator_slot_rate = {
    slot = {label = "Inventory Type", type = "int", enum = "InventoryType"},
    rate = {label = "Rate", tooltip = "Stat budget of this slot. "..RATE, type = "float"},
}

Fields.item_generator_stat_slots = {
    quality = {label = "Quality", type = "int", enum = "ItemQuality"},
    primary_slots = {label = "Primary Stats", tooltip = "Primary stats an item of this quality rolls", type = "int"},
    secondary_slots = {label = "Secondary Stats", tooltip = "Secondary stats an item of this quality rolls", type = "int"},
}

Fields.item_generator_item_types = {
    itemClass = {label = "Class", tooltip = "Only weapons and armor can be generated", type = "int", enum = "ItemClass"},
    subClass = {label = "Subclass", type = "int", enum = "ItemSubClass_4"},
    inventoryType = {label = "Inventory Type", type = "int", enum = "InventoryType"},
    sheath = {label = "Sheath", type = "int", enum = "ItemSheath"},
    material = {label = "Material", type = "int", enum = "ItemMaterial"},
    rangedModRange = {label = "Ranged Range Mod", type = "float"},
}

Fields.item_generator_spells = {
    spellId = {label = "Spell", type = "int", ref = "spell"},
    minQuality = {label = "Min Quality", type = "int", enum = "ItemQuality"},
    maxQuality = {label = "Max Quality", type = "int", enum = "ItemQuality"},
    itemClass = {label = "Class", tooltip = "-1 any class, -2 any class but never trinkets", type = "int", enum = "ItemGenAnyClass"},
    subClass = {label = "Subclass", tooltip = "-1 any subclass", type = "int"},
    inventoryType = {label = "Inventory Type", tooltip = "-1 any slot", type = "int", enum = "ItemGenAnyInventoryType"},
    statGroup = {label = "Stat Group", tooltip = "-1 any stat group", type = "int", ref = "itemgen_statgroup"},
    statGroupMask = {label = "Stat Group Mask", tooltip = "The item stat group needs one of these bits, 0 skips the check", type = "int", flags = "ItemGenStatGroupMask"},
    minItemLevel = {label = "Min Item Level", tooltip = "-1 no lower limit", type = "int"},
    maxItemLevel = {label = "Max Item Level", tooltip = "-1 no upper limit", type = "int"},
    SpellTrigger = {label = "Trigger", type = "int", enum = "ItemSpellTrigger"},
    SpellCharges = {label = "Charges", type = "int"},
    SpellPPMRate = {label = "Procs Per Minute", type = "float"},
    SpellCooldown = {label = "Cooldown (ms)", tooltip = "-1 uses the spell cooldown", type = "int"},
    SpellCategory = {label = "Category", type = "int", ref = "dbc:SpellCategory"},
    SpellCategoryCooldown = {label = "Category Cooldown (ms)", tooltip = "-1 uses the spell category cooldown", type = "int"},
}

Fields.item_generator_stat_group_mask = {
    bit = {label = "Bit", tooltip = "A single bit value such as 1, 2, 4 or 8", type = "int"},
    name = {label = "Name", type = "string"},
    comment = {label = "Comment", type = "string"},
}

Fields.item_generator_quality_type = {
    id = {label = "Quality", type = "int", enum = "ItemQuality"},
    name = {label = "Name", tooltip = "Reference label, the server does not read this table", type = "string"},
}

Fields.item_generator_slot_type = {
    id = {label = "Inventory Type", type = "int", enum = "InventoryType"},
    name = {label = "Name", tooltip = "Reference label, the server does not read this table", type = "string"},
}

Fields.item_generator_stat_type = {
    id = {label = "Stat", type = "int", enum = "ItemModType"},
    name = {label = "Name", tooltip = "Reference label, the server does not read this table", type = "string"},
}

Fields.item_generator_itemsets = {
    setid = {label = "Item Set", tooltip = "ItemSet.dbc id. The server does not load this table yet", type = "int", ref = "dbc:ItemSet"},
    class = {label = "Class", tooltip = "-1 any", type = "int", enum = "ItemGenAnyClass"},
    subclass = {label = "Subclass", tooltip = "-1 any", type = "int"},
    inventorytype = {label = "Inventory Type", tooltip = "-1 any", type = "int", enum = "ItemGenAnyInventoryType"},
    statgroup = {label = "Stat Group", tooltip = "-1 any", type = "int", ref = "itemgen_statgroup"},
    minlevel = {label = "Min Level", type = "int"},
    maxlevel = {label = "Max Level", type = "int"},
    minquality = {label = "Min Quality", type = "int", enum = "ItemQuality"},
    maxquality = {label = "Max Quality", type = "int", enum = "ItemQuality"},
    displayoverride = {label = "Display Override", type = "int"},
    flavoroverride = {label = "Flavor Override", type = "string"},
    comment = {label = "Comment", type = "string"},
}

Fields.item_generator_names = {
    itemtype = {label = "Item Class", tooltip = "2 weapons, 4 armor, -1 any", type = "int", enum = "ItemGenAnyClass"},
    subclass = {label = "Subclass", tooltip = "-1 any. Shields use the weapon name rules", type = "int"},
    inventoryType = {label = "Inventory Type", tooltip = "Armor only, weapons always use 0", type = "int", enum = "InventoryType"},
    array_id = {label = "Word List", type = "int", enum = "ItemGenNameList"},
    string = {label = "Word", type = "string"},
}

Fields.item_generator_displays = {
    quality = {label = "Quality", tooltip = "Legendaries also draw from the epic displays", type = "int", enum = "ItemQuality"},
    classType = {label = "Class", type = "int", enum = "ItemClass"},
    subclassType = {label = "Subclass", type = "int"},
    inventoryType = {label = "Inventory Type", type = "int", enum = "InventoryType"},
    displayId = {label = "Display ID", tooltip = "ItemDisplayInfo.dbc id", type = "int", ref = "dbc:ItemDisplayInfo"},
}

Fields.item_generator_stat_group = {
    id = {label = "Stat Group", tooltip = "17, 18 and 19 are internal pools the generator needs", type = "int"},
    name = {label = "Name", type = "string"},
    mask = {label = "Mask", tooltip = "Roles this group belongs to, spell and legendary filters test it", type = "int", flags = "ItemGenStatGroupMask"},
    flags = {label = "Flags", type = "int", flags = "ItemGenStatGroupFlags"},
    weight = {label = "Weight", tooltip = "Chance weight of a random roll, 0 never rolls", type = "int"},
    orderIdx = {label = "Order", tooltip = "Position in the random roll list, changing it changes what future items roll", type = "int"},
    trinketGroup = {label = "Trinket Group", tooltip = "Group trinkets of this group roll from, -1 keeps this group", type = "int", ref = "itemgen_statgroup"},
    catalystItemId = {label = "Catalyst Item", tooltip = "Item that turns an item into this group", type = "int", ref = "item"},
    classFlag = {label = "Class Flag", tooltip = "item_template custom flag that forces this group", type = "int", flags = "ItemFlagsCustom"},
    staminaMod = {label = "Stamina Mod", type = "float"},
    armorClothMod = {label = "Cloth Armor Mod", type = "float"},
    armorLeatherMod = {label = "Leather Armor Mod", type = "float"},
    armorMailMod = {label = "Mail Armor Mod", type = "float"},
    armorPlateMod = {label = "Plate Armor Mod", type = "float"},
    comment = {label = "Comment", type = "string"},
}

Fields.item_generator_stat_group_stats = {
    groupId = {label = "Stat Group", type = "int", ref = "itemgen_statgroup"},
    type = {label = "List", type = "int", enum = "ItemGenStatListType"},
    idx = {label = "Index", tooltip = "Order inside the list, the generator picks by index so reordering changes future rolls", type = "int"},
    value = {label = "Value", tooltip = "A stat type for the stat lists, a socket color for the socket list", type = "int", enum = "ItemModType"},
    comment = {label = "Comment", type = "string"},
}

Fields.item_generator_unique = {
    id = {label = "Unique ID", tooltip = "item_template.uniqueId points here", type = "int"},
    name = {label = "Name", tooltip = "Empty keeps the base template name", type = "string"},
    description = {label = "Description", tooltip = "Empty keeps the base template description", type = "string"},
    displayId = {label = "Display ID", tooltip = "0 keeps the base template display", type = "int", ref = "dbc:ItemDisplayInfo"},
    quality = {label = "Quality", type = "int", enum = "ItemQuality"},
    itemClass = {label = "Class", tooltip = "Weapon or armor", type = "int", enum = "ItemClass"},
    subClass = {label = "Subclass", type = "int", enum = "ItemSubClass_4"},
    inventoryType = {label = "Inventory Type", type = "int", enum = "InventoryType"},
    sheath = {label = "Sheath", type = "int", enum = "ItemSheath"},
    material = {label = "Material", type = "int", enum = "ItemMaterial"},
    rangedModRange = {label = "Ranged Range Mod", type = "float"},
    bonding = {label = "Binding", type = "int", enum = "ItemGenBonding"},
    requiredLevel = {label = "Required Level", tooltip = "-1 keeps the base template value", type = "int"},
    itemLimitCategory = {label = "Limit Category", tooltip = "-1 keeps the base template value", type = "int", ref = "dbc:ItemLimitCategory"},
    disenchantId = {label = "Disenchant Loot", tooltip = "0 picks one from the quality", type = "int"},
    statGroup = {label = "Stat Group", tooltip = "Pool for player added stats and tertiary stats, not the authored stats. -1 random", type = "int", ref = "itemgen_statgroup"},
    flags = {label = "Flags", type = "int", flags = "ItemGenUniqueFlags"},
    ilevelMin = {label = "Item Level Min", type = "int"},
    ilevelMax = {label = "Item Level Max", type = "int"},
    armorMin = {label = "Armor Min", tooltip = "-1 lets the generator derive it", type = "int"},
    armorMax = {label = "Armor Max", type = "int"},
    blockMin = {label = "Block Min", tooltip = "-1 lets the generator derive it", type = "int"},
    blockMax = {label = "Block Max", type = "int"},
    delayMin = {label = "Speed Min (ms)", tooltip = "-1 lets the generator derive it", type = "int"},
    delayMax = {label = "Speed Max (ms)", type = "int"},
    dmgMinLow = {label = "Damage Min Low", tooltip = "-1 lets the generator derive the damage", type = "float"},
    dmgMinHigh = {label = "Damage Min High", type = "float"},
    dmgMaxLow = {label = "Damage Max Low", type = "float"},
    dmgMaxHigh = {label = "Damage Max High", type = "float"},
    dmgType = {label = "Damage School", type = "int", enum = "SpellSchool"},
    dmg2MinLow = {label = "Bonus Damage Min Low", tooltip = "-1 means no second damage line", type = "float"},
    dmg2MinHigh = {label = "Bonus Damage Min High", type = "float"},
    dmg2MaxLow = {label = "Bonus Damage Max Low", type = "float"},
    dmg2MaxHigh = {label = "Bonus Damage Max High", type = "float"},
    dmg2Type = {label = "Bonus Damage School", type = "int", enum = "SpellSchool"},
    socketColor1 = {label = "Socket 1", type = "int", enum = "ItemGenSocketColor"},
    socketColor2 = {label = "Socket 2", type = "int", enum = "ItemGenSocketColor"},
    socketColor3 = {label = "Socket 3", type = "int", enum = "ItemGenSocketColor"},
    socketBonus = {label = "Socket Bonus", tooltip = "SpellItemEnchantment id, -1 keeps the base template bonus", type = "int", ref = "dbc:SpellItemEnchantment"},
    comment = {label = "Comment", type = "string"},
}

Fields.item_generator_unique_stat = {
    uniqueId = {label = "Unique ID", type = "int"},
    idx = {label = "Index", tooltip = "Roll stream of the stat, keep it when editing so items keep their values", type = "int"},
    statType = {label = "Stat", type = "int", enum = "ItemModType"},
    minValue = {label = "Min Value", type = "int"},
    maxValue = {label = "Max Value", type = "int"},
    comment = {label = "Comment", type = "string"},
}

Fields.item_generator_unique_spell = {
    uniqueId = {label = "Unique ID", type = "int"},
    idx = {label = "Spell Slot", tooltip = "0 to 4, the spell slot of the item", type = "int"},
    spellId = {label = "Spell", type = "int", ref = "spell"},
    spellTrigger = {label = "Trigger", type = "int", enum = "ItemSpellTrigger"},
    spellCharges = {label = "Charges", type = "int"},
    spellPPMRate = {label = "Procs Per Minute", type = "float"},
    spellCooldown = {label = "Cooldown (ms)", tooltip = "-1 uses the spell cooldown", type = "int"},
    spellCategory = {label = "Category", type = "int", ref = "dbc:SpellCategory"},
    spellCategoryCooldown = {label = "Category Cooldown (ms)", tooltip = "-1 uses the spell category cooldown", type = "int"},
    comment = {label = "Comment", type = "string"},
}

Fields.item_generator_legendary_template = {
    Id = {label = "Legendary ID", type = "int"},
    minItemLevel = {label = "Min Item Level", type = "int"},
    maxItemLevel = {label = "Max Item Level", type = "int"},
    itemClass = {label = "Class", tooltip = "-1 any", type = "int", enum = "ItemGenAnyClass"},
    itemSubClass = {label = "Subclass", tooltip = "-1 any", type = "int"},
    itemInventoryType = {label = "Inventory Type", tooltip = "-1 any", type = "int", enum = "ItemGenAnyInventoryType"},
    itemStatGroup = {label = "Stat Group Filter", tooltip = "-1 any stat group", type = "int", ref = "itemgen_statgroup"},
    statGroupMask = {label = "Stat Group Mask", tooltip = "The item stat group needs one of these bits, 0 skips the check", type = "int", flags = "ItemGenStatGroupMask"},
    primaryStatModifier = {label = "Primary Stat Mod", type = "float"},
    secondaryStatModifier = {label = "Secondary Stat Mod", type = "float"},
    socketMod = {label = "Extra Sockets", tooltip = "0 to 3", type = "int"},
    prismaticSocket = {label = "Prismatic Socket", type = "int", bool = true},
    primaryStatCountMod = {label = "Primary Stat Count Mod", type = "int"},
    secondaryStatCountMod = {label = "Secondary Stat Count Mod", type = "int"},
    statGroupOverride = {label = "Stat Group Override", tooltip = "-1 keeps the rolled group", type = "int", ref = "itemgen_statgroup"},
    itemLimitCatagory = {label = "Limit Category", type = "int"},
    essenceItemId = {label = "Essence Item", type = "int", ref = "item"},
    legendarySpellTemplateId = {label = "Spell Set", tooltip = "legendaryIndex of item_generator_legendary_spell_entry, several legendaries can share one set", type = "int"},
    displayOverride = {label = "Display Override", type = "int"},
    legendaryFlags = {label = "Flags", type = "int", flags = "ItemGenLegendaryFlags"},
    uniqueId = {label = "Unique Item", tooltip = "Turns the item into this item_generator_unique item, 0 keeps it", type = "int", ref = "itemgen_unique"},
    nameOverride = {label = "Name Override", type = "string"},
    loreOverride = {label = "Lore Override", type = "string"},
    Comment = {label = "Comment", type = "string"},
}

Fields.item_generator_legendary_spell_entry = {
    legendaryIndex = {label = "Spell Set", type = "int"},
    spellIndex = {label = "Spell Slot", tooltip = "Slot of the legendary spell", type = "int"},
    SpellId = {label = "Spell", type = "int", ref = "spell"},
    SpellTrigger = {label = "Trigger", type = "int", enum = "ItemSpellTrigger"},
    SpellCharges = {label = "Charges", type = "int"},
    SpellPPMRate = {label = "Procs Per Minute", type = "float"},
    SpellCooldown = {label = "Cooldown (ms)", tooltip = "-1 uses the spell cooldown", type = "int"},
    SpellCategory = {label = "Category", type = "int", ref = "dbc:SpellCategory"},
    SpellCategoryCooldown = {label = "Category Cooldown (ms)", tooltip = "-1 uses the spell category cooldown", type = "int"},
}

for name, fields in pairs(Fields) do
    GameEditorData.Fields[name] = fields
end

local function Slots(first, last, ...)
    local columns = {}
    local prefixes = {...}

    for slot = first, last do
        for _, prefix in ipairs(prefixes) do
            table.insert(columns, prefix..slot)
        end
    end

    return columns
end

local function SpellSlot(slot)
    return {"spellid_"..slot, "spelltrigger_"..slot, "spellcharges_"..slot, "spellppmRate_"..slot, "spellcooldown_"..slot, "spellcategory_"..slot, "spellcategorycooldown_"..slot}
end

-- Tabs of the item template editor, each one a group list of the same row.
GameEditorData.ItemTemplateTabs = {
    {
        title = "Basics",
        groups = {
            {title = "Item", columns = {"entry", "name", "description", "Quality", "class", "subclass", "InventoryType", "displayid", "ItemLevel", "RequiredLevel"}},
            {title = "Flags", columns = {"bonding", "Flags", "FlagsExtra", "flagsCustom", "uniqueId"}},
            {title = "Vendor", columns = {"BuyCount", "BuyPrice", "SellPrice"}},
            {title = "Stacking and Bags", columns = {"stackable", "maxcount", "ContainerSlots", "BagFamily"}},
            {title = "Look and Sound", columns = {"sheath", "Material", "SoundOverrideSubclass"}, collapsed = true},
        },
    },
    {
        title = "Stats",
        groups = {
            {title = "Stats", columns = {"StatsCount", "stat_type1", "stat_value1", "stat_type2", "stat_value2", "stat_type3", "stat_value3", "stat_type4", "stat_value4", "stat_type5", "stat_value5"}},
            {title = "Stats 6 to 10", columns = Slots(6, 10, "stat_type", "stat_value"), collapsed = true},
            {title = "Armor", columns = {"armor", "block", "ArmorDamageModifier"}},
            {title = "Weapon", columns = {"dmg_min1", "dmg_max1", "dmg_type1", "delay", "dmg_min2", "dmg_max2", "dmg_type2", "ammo_type", "RangedModRange"}},
            {title = "Resistances", columns = {"holy_res", "fire_res", "nature_res", "frost_res", "shadow_res", "arcane_res"}, collapsed = true},
            {title = "Scaling", columns = {"ScalingStatDistribution", "ScalingStatValue"}, collapsed = true},
        },
    },
    {
        title = "Spells",
        groups = {
            {title = "Spell 1", columns = SpellSlot(1)},
            {title = "Spell 2", columns = SpellSlot(2)},
            {title = "Spell 3", columns = SpellSlot(3), collapsed = true},
            {title = "Spell 4", columns = SpellSlot(4), collapsed = true},
            {title = "Spell 5", columns = SpellSlot(5), collapsed = true},
        },
    },
    {
        title = "Requirements",
        groups = {
            {title = "Who", columns = {"AllowableClass", "AllowableRace"}},
            {title = "Skill and Spell", columns = {"RequiredSkill", "RequiredSkillRank", "requiredspell"}},
            {title = "Reputation and Rank", columns = {"RequiredReputationFaction", "RequiredReputationRank", "requiredhonorrank", "RequiredCityRank"}},
            {title = "Where and When", columns = {"area", "Map", "HolidayId", "ItemLimitCategory"}},
        },
    },
    {
        title = "Sockets",
        groups = {
            {title = "Sockets", columns = Slots(1, 3, "socketColor_", "socketContent_")},
            {title = "Bonus", columns = {"socketBonus"}},
            {title = "Gem", columns = {"GemProperties"}},
        },
    },
    {
        title = "Misc",
        groups = {
            {title = "Use", columns = {"startquest", "lockid", "duration", "FoodType", "TotemCategory", "MaxDurability"}},
            {title = "Loot and Enchants", columns = {"RandomProperty", "RandomSuffix", "itemset", "RequiredDisenchantSkill", "DisenchantID", "minMoneyLoot", "maxMoneyLoot"}},
            {title = "Text", columns = {"PageText", "LanguageID", "PageMaterial"}},
            {title = "Server", columns = {"ScriptName", "VerifiedBuild"}},
        },
    },
}

GameEditorData.Groups.item_template_locale = {
    {columns = {"locale", "Name", "Description", "VerifiedBuild"}},
}

GameEditorData.Groups.item_generator_spells = {
    {title = "Spell", columns = {"spellId", "SpellTrigger", "SpellCharges", "SpellPPMRate", "SpellCooldown", "SpellCategory", "SpellCategoryCooldown"}},
    {title = "Rolls On", columns = {"minQuality", "maxQuality", "itemClass", "subClass", "inventoryType", "statGroup", "statGroupMask", "minItemLevel", "maxItemLevel"}},
}

GameEditorData.Groups.item_generator_stat_group = {
    {title = "Stat Group", columns = {"id", "name", "comment", "flags", "mask", "weight", "orderIdx"}},
    {title = "Links", columns = {"trinketGroup", "catalystItemId", "classFlag"}},
    {title = "Modifiers", columns = {"staminaMod", "armorClothMod", "armorLeatherMod", "armorMailMod", "armorPlateMod"}},
}

GameEditorData.Groups.item_generator_unique = {
    {title = "Item", columns = {"id", "name", "description", "comment", "quality", "itemClass", "subClass", "inventoryType", "displayId", "ilevelMin", "ilevelMax"}},
    {title = "Rules", columns = {"bonding", "requiredLevel", "itemLimitCategory", "disenchantId", "statGroup", "flags"}},
    {title = "Armor and Block", columns = {"armorMin", "armorMax", "blockMin", "blockMax"}, collapsed = true},
    {title = "Weapon", columns = {"delayMin", "delayMax", "dmgMinLow", "dmgMinHigh", "dmgMaxLow", "dmgMaxHigh", "dmgType", "dmg2MinLow", "dmg2MinHigh", "dmg2MaxLow", "dmg2MaxHigh", "dmg2Type", "rangedModRange"}, collapsed = true},
    {title = "Sockets", columns = {"socketColor1", "socketColor2", "socketColor3", "socketBonus"}, collapsed = true},
    {title = "Look", columns = {"sheath", "material"}, collapsed = true},
}

GameEditorData.Groups.item_generator_legendary_template = {
    {title = "Legendary", columns = {"Id", "Comment", "nameOverride", "loreOverride", "legendaryFlags", "uniqueId", "essenceItemId", "legendarySpellTemplateId"}},
    {title = "Rolls On", columns = {"minItemLevel", "maxItemLevel", "itemClass", "itemSubClass", "itemInventoryType", "itemStatGroup", "statGroupMask"}},
    {title = "Effect", columns = {"primaryStatModifier", "secondaryStatModifier", "primaryStatCountMod", "secondaryStatCountMod", "socketMod", "prismaticSocket", "statGroupOverride", "itemLimitCatagory", "displayOverride"}},
}

-- Single section groups of the row editors, in table order.
GameEditorData.Groups.item_generator_quality_rate = {
    {columns = {"quality", "rate"}},
}

GameEditorData.Groups.item_generator_stat_rate = {
    {columns = {"stat", "rate"}},
}

GameEditorData.Groups.item_generator_slot_rate = {
    {columns = {"slot", "rate"}},
}

GameEditorData.Groups.item_generator_stat_slots = {
    {columns = {"quality", "primary_slots", "secondary_slots"}},
}

GameEditorData.Groups.item_generator_item_types = {
    {columns = {"itemClass", "subClass", "inventoryType", "sheath", "material", "rangedModRange"}},
}

GameEditorData.Groups.item_generator_stat_group_mask = {
    {columns = {"bit", "name", "comment"}},
}

GameEditorData.Groups.item_generator_quality_type = {
    {columns = {"id", "name"}},
}

GameEditorData.Groups.item_generator_slot_type = {
    {columns = {"id", "name"}},
}

GameEditorData.Groups.item_generator_stat_type = {
    {columns = {"id", "name"}},
}

GameEditorData.Groups.item_generator_itemsets = {
    {columns = {"setid", "comment", "class", "subclass", "inventorytype", "statgroup", "minlevel", "maxlevel", "minquality", "maxquality", "displayoverride", "flavoroverride"}},
}

GameEditorData.Groups.item_generator_names = {
    {columns = {"array_id", "string", "itemtype", "subclass", "inventoryType"}},
}

GameEditorData.Groups.item_generator_displays = {
    {columns = {"inventoryType", "displayId", "quality", "classType", "subclassType"}},
}

GameEditorData.Groups.item_generator_stat_group_stats = {
    {columns = {"groupId", "type", "idx", "value", "comment"}},
}

GameEditorData.Groups.item_generator_unique_stat = {
    {columns = {"uniqueId", "idx", "statType", "minValue", "maxValue", "comment"}},
}

GameEditorData.Groups.item_generator_unique_spell = {
    {columns = {"uniqueId", "idx", "spellId", "spellTrigger", "spellCharges", "spellPPMRate", "spellCooldown", "spellCategory", "spellCategoryCooldown", "comment"}},
}

GameEditorData.Groups.item_generator_legendary_spell_entry = {
    {columns = {"legendaryIndex", "spellIndex", "SpellId", "SpellTrigger", "SpellCharges", "SpellPPMRate", "SpellCooldown", "SpellCategory", "SpellCategoryCooldown"}},
}
