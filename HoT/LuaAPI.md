# HoT Lua API

This document lists the Lua functions and events that the HoT client adds to the stock 3.3.5 API, along with the stock functions whose behavior has changed.

## General notes

- **Events are always registered.** Every custom event can be registered at load time, even if the feature that fires it is not active yet.
- **Request and cache pattern.** Most features follow the same flow. A `Request` function sends a packet to the server and returns nothing. When the server responds, the client fills a local cache and fires a `HOT_` event. The `Get` functions only read that cache, so calling them before the event fires returns zero or nil.
- **Indexes are 1-based** unless a function says otherwise. Functions that use 0-based slots are marked.
- **Argument handling.** Most number arguments are read loosely, so a missing or non-numeric value is treated as 0. Pass valid values rather than relying on this.
- **Event arguments.** Numbers arrive as numbers and strings as strings. Boolean values are sent as 0 or 1.

---

## General utilities

| Function | Returns | Description |
|---|---|---|
| `FlashGameWindow()` | nothing | Flashes the game window's taskbar entry. |
| `GetShapeshiftFormID()` | number | Current shapeshift form id, or 0 if none. |
| `GetSpellDescription(spellId)` | string or nil | The spell's tooltip description with all `$` tokens resolved. nil if the spell does not exist. |
| `GetSpellNameById(spellId)` | name, subtext | Spell name and rank or subtext. nil, nil if the spell does not exist. |
| `GetMapNameById(mapId)` | string or nil | Map name from Map.dbc. |
| `GetCurrentMapId()` | number | Id of the current map. |
| `GetGemSpellInfo(itemId)` | resolved, spellId, icon | See below. |
| `GetGemType(itemId)` | string or nil | One of "Prismatic", "Red", "Blue", "Yellow", "Green", or "Gem" for any other subclass. nil if the item is not cached yet. |
| `ConvertCoordsToScreenSpace(x, y, z)` | sx, sy, depth | Projects a world position to screen coordinates. Returns 0, 0, 0 if there is no world frame. The result does not indicate whether the point is on screen. |
| `LockBagItem(...)` | nothing | Locks a bag item on the client, which greys it out. See below for arguments. |
| `UnlockBagItem(...)` | nothing | Reverses `LockBagItem`. Takes the same arguments. |

**GetGemSpellInfo(itemId)**
- Returns `resolved` (boolean), `spellId` (number or nil) and `icon` (texture path or nil).
- `resolved` is false if the item is not cached yet or has no on-socket spell. An uncached item is queried automatically, so call the function again later.
- If the socket spell teaches another spell, the id of the taught spell is returned.

**LockBagItem / UnlockBagItem**
- No arguments: the item on the cursor.
- `(bag, slot)`: standard bag and slot numbers.
- `(itemId)` or `(itemName)`: the first matching item in the player's bags.

### Discord

| Function | Returns | Description |
|---|---|---|
| `UpdateDiscordPresence(details, state [, largeImage, largeText, smallImage, smallText])` | nothing | Sets Discord Rich Presence. `details` is the top line and `state` is the second line. `largeImage` defaults to `"icon_transparent_"` and the other optional arguments default to empty strings. Optional arguments that are not strings are ignored. |
| `ToggleDiscord(enabled)` | nothing | Pass `1` to enable Rich Presence. Any other value disables it. The setting is saved in the `enableDiscord` cvar. |

---

## Changes to stock functions

| Function | Change |
|---|---|
| `UnitLevel(unit)` | Returns the scaled level provided by the server: item level for players and dungeon level for creatures. Returns the stock value until the server responds. Only bosses return -1. |
| `TrueLevel(unit)` | New. Returns the unscaled level, as stock `UnitLevel` did. Only bosses return -1. |
| `UnitClass(unit)` | For loaded units other than "player", the first return value (display name) becomes the subclass name once it is known. The second return value (class file token) is still the base class. |
| `GetComboPoints(unit [, target])` | Works for every class. The player's combo points no longer depend on the current target. |
| `GetSpellBonusHealing([school])` | Accepts an optional `school` from 1 to 7 and returns the healing bonus for that school. Values outside that range raise an error. With no argument it behaves as stock. |
| `CastSpellByName(name, target)` | `target` also accepts `"cursor"`, which places a ground-targeted spell at the cursor position, and `"self"`, which places it at the player's position. Other values behave as stock. Ground target placement is also clamped to the spell's maximum range. |
| `UseContainerItem(bag, slot)` | While the crafting window is open, using a matching item places it in a crafting slot. Gems with a socket spell fire `HOT_GEM_SOCKET_USE` instead of being used. Merchant, bank, mail, trade, auction house and guild bank windows still take priority. |
| `IsUnitOnQuest(index, unit)` | Supports party members. Their quest list is provided by the server, so the function returns nil until `UNIT_QUEST_LOG_CHANGED` fires for that member. |
| Quest log functions | The quest log has 100 slots instead of 25. |
| `CheckInbox()` | The 60 second throttle has been removed. |
| `GetBankSlotCost()` | Always returns 0. Barber shop and guild tabard costs are also 0. |

Other behavior changes:
- The loot window no longer closes when the player moves. It closes when the player is more than 45 yards from the loot target. This is controlled by the `lootStayOpenWhileMoving` cvar.
- Item tooltips show the HoT labels (Paradoxical, Corrupted, Legendary, Heirloom, Time Warped and their combinations) in place of "Heroic" when the item has the matching flags.
- Cooldowns can progress faster or slower than real time. See `GetCooldownRate`. Stock cooldown functions report the adjusted values.

### Custom stats

These functions read stats provided by the server. `HOT_STAT_UPDATE` fires when they change, followed by the stock `COMBAT_RATING_UPDATE`.

| Function | Returns |
|---|---|
| `GetSpellPen(school)` | number. Spell penetration for school 0 to 6. Other values raise an error. |
| `GetTotalSpellPen()` | number |
| `GetMagicFind()` | number |
| `GetHealthLeech()` | number |
| `GetManaLeech()` | number |
| `GetHealthRegen()` | regen, regenInCombat |
| `GetCritDamageMod()` | number |
| `GetCritHealingMod()` | number |

### Texture atlases

Retail-style atlas support. These globals are defined before FrameXML loads, so they are available to all addons.

**C_Texture.LoadAtlasData(data)**
- `data` has the form `{ ["Interface\\Path\\file.blp"] = { ["atlasName"] = { w, h, left, right, top, bottom, tileH, tileV } } }`.
- Atlas names are case-insensitive. Loading an existing name replaces it.
- Returns the number of entries added.

**C_Texture.GetAtlasInfo(name)**
- Returns nil, or a table with `width`, `height`, `left`, `right`, `top`, `bottom`, `leftTexCoord`, `rightTexCoord`, `topTexCoord`, `bottomTexCoord`, `tilesHorizontally`, `tilesVertically` and `file`.

**SetAtlas(texture, name [, useAtlasSize])**, also available as `C_Texture.SetAtlas`
- Applies the atlas texture, texture coordinates and tiling to `texture`. Also sets the size when `useAtlasSize` is true.
- Returns false if `texture` is nil or the atlas does not exist, otherwise true.
- This is a global function, not a texture method. Call it as `SetAtlas(tex, "name")`.

---

## Feature flags

The server reports which optional systems are enabled.

| Function | Returns | Description |
|---|---|---|
| `RequestFeatureFlags()` | nothing | Requests the feature flags. Fires `HOT_FEATURE_UPDATE`. |
| `HasFeatureFlags()` | boolean | True once the flags have been received. |
| `IsFeatureEnabled(featureId)` | boolean | True if the feature is enabled. Returns false before the flags are received, so use `HasFeatureFlags` to tell the two cases apart. |

Feature ids: `0` gem system, `1` AoE loot, `2` transmog, `3` pet auto loot.

| Event | Args | Description |
|---|---|---|
| `HOT_FEATURE_UPDATE` | none | Feature flags received. |

## Player settings

Per-character settings stored on the server.

| Function | Returns | Description |
|---|---|---|
| `GetPlayerSetting(settingId)` | number or nil | Cached value. nil if no settings have been received or the id is invalid. |
| `SetPlayerSetting(settingId, value)` | nothing | Updates the local cache immediately and sends the change to the server. Does not fire an event. |

Setting ids: `0` AoE loot, `1` auto item level, `2` pet auto loot. Ids of 3 or higher are ignored.

| Event | Args | Description |
|---|---|---|
| `HOT_PLAYER_SETTINGS_UPDATE` | none | Settings received from the server. Not fired by `SetPlayerSetting`. |

---

## Units: absorbs, heal prediction, levels

### Absorbs and incoming heals

These functions share their names with the retail API, but the implementation is specific to HoT.

| Function | Returns | Description |
|---|---|---|
| `UnitGetTotalAbsorbs(unit)` | number or nil | Total damage absorb on the unit, including mana shields. nil if the unit token is invalid. May briefly return 0 while the client requests the values from the server. |
| `UnitGetTotalHealAbsorbs(unit)` | number or nil | Total heal absorb on the unit. |
| `UnitGetIncomingHeals(unit [, healer])` | number or nil | Predicted heals currently being cast on the unit, from all casters or only from `healer`. nil if `unit` is invalid, 0 if `healer` is invalid. |

| Event | Args | Description |
|---|---|---|
| `UNIT_ABSORB_AMOUNT_CHANGED` | unit | Absorb values received for the unit. Fires once per matching unit token, like stock `UNIT_` events. May fire without a change in value. |
| `UNIT_HEAL_ABSORB_AMOUNT_CHANGED` | unit | Heal absorb values received for the unit. |
| `UNIT_HEAL_PREDICTION` | unit | Incoming heal prediction changed for the unit. |
| `HOT_AURA_VALUES_UPDATED` | none | Live values received for the aura tooltip currently shown. Refresh the tooltip to display them. |

Health bar display is controlled by the cvars `healPredictionAbsorb`, `healPredictionAbsorbFullHealth`, `healPredictionAbsorbText` and `healPredictionHealText`, and by the color cvars `healPredictionAbsorbColor`, `healPredictionHealAbsorbColor`, `healPredictionMyHealColor`, `healPredictionOtherHealColor`, `healPredictionAbsorbTextColor` and `healPredictionHealTextColor` (hex rrggbb).

### Unit levels and subclass

| Function | Returns | Description |
|---|---|---|
| `GetUnitItemLevel(unit)` | number or nil | Item level of a player unit. Returns nil and requests the value if it is not known yet. |
| `GetUnitDungeonLevel(unit)` | number or nil | Item level for players, dungeon level for creatures, pets and vehicles. nil while unknown. |
| `GetUnitSubclass(unit)` | number or nil | The player's subclass (ChrClasses id). `unit` defaults to "player". Only available for players. |
| `GetOurItemLevel()` | number or nil | The player's own item level. |

Unanswered requests are retried a limited number of times. The cache is cleared when entering the world.

| Event | Args | Description |
|---|---|---|
| `UNIT_LEVEL` | unit | Stock event, also fired when a scaled level is received. It is also fired for target, focus, targettarget and focustarget, so a handler may receive duplicates. |
| `HOT_PLAYER_ITEM_LEVEL` | itemLevel | The player's own item level was received. |

---

## Cooldown rate

| Function | Returns | Description |
|---|---|---|
| `GetCooldownRate()` | rate, includesGcd | Cooldown speed multiplier set by the server. A `rate` of 1.0 is normal, 2.0 makes cooldowns finish twice as fast and 0.5 twice as slow. The value is clamped between 0.02 and 10. `includesGcd` is true if the global cooldown is also affected. Only cooldowns with one hour or less remaining are affected. |

No event fires when the rate changes. Poll this function if the value is needed.

## Action bar

| Function | Returns | Description |
|---|---|---|
| `FindSpellActionBarSlots(spellId)` | slot, slot, ... or nil | Every action slot containing the spell, as separate return values. **Slots are 0-based.** |
| `ReplaceActionBarSpell(oldSpellId, newSpellId)` | nothing | Replaces every instance of a spell on the action bars with another spell. |
| `SetSpellInActionBarSlot(spellId, slot)` | nothing | Places a spell in an action slot. **The slot is 0-based.** |

| Event | Args | Description |
|---|---|---|
| `HOT_PLACE_ACTION` | spellId, slot | A new active spell was learned and a free slot is available. The spell is on the cursor only while this event is being handled, so call `PlaceAction(slot + 1)` inside the handler. `slot` is 0-based. Not fired for passive spells, totems, or spells already on the action bars. |

Holding an action key can repeat the action. This is controlled by `actionButtonHoldRepeat` (0 off, 1 until the spell casts, 2 continuous), `actionButtonHoldRepeatDelay` (ms, default 500) and `actionButtonHoldRepeatInterval` (ms, default 100).

---

## Talents

The custom talent tree. `GetCached` functions read the local cache and `Custom` functions send requests to the server. The caches are not cleared when entering the world.

### Reading talent data

| Function | Returns | Description |
|---|---|---|
| `GetCustomTalentStorage()` | table | The full talent tree, keyed by node index. The table is sparse, so iterate it with `pairs`. Each value is `{ spellId, x, y, { childNodeIndex, ... }, mutex, buttonType, flagMask }`. Empty until `HOT_TALENT_CACHE` fires. |
| `GetTalentTreeVersion()` | number | Tree version. 0 until the small cache is received. |
| `GetCachedTalentFreePoints()` | number | Unspent talent points. |
| `GetCachedTalentLevel()` | number | Talent level. |
| `GetCachedXPTalentLevel()` | number | XP talent level. |
| `GetCachedMaxTalentLevel()` | number | Maximum talent level. |
| `GetCachedLearntTalents()` | table | Array of learned node ids. |
| `GetCachedItemGrantedTalents()` | table | Array of node ids granted by equipped items. |
| `GetCachedInspectLearntTalents()` | table | Learned node ids from the most recent inspect. |
| `GetCachedInspectFreePoints()` | number | Unspent points of the inspected player. |
| `GetCachedInspectTalentLevel()` | number | Talent level of the inspected player. |
| `GetCachedTalentLoadouts()` | table | Array of `{ id = number, name = string, count = number }`, where `count` is the number of nodes in the loadout. |
| `GetActiveTalentLoadout()` | number | Id of the active loadout. |
| `GetMaxTalentLoadouts()` | number | Maximum number of loadouts allowed. |

### Talent actions

| Function | Returns | Description |
|---|---|---|
| `CustomRequestTalentCache()` | nothing | Requests the full tree. Fires `HOT_TALENT_CACHE`. |
| `CustomRequestTalentSmallCache()` | nothing | Requests points, level, and the learned and item-granted lists. Fires `HOT_TALENT_SMALL_CACHE`. |
| `CustomRequestTalentPoints()` | nothing | Requests unspent points. Fires `HOT_TALENT_POINTS`. |
| `CustomLearnTalent(nodeId)` | nothing | Learns a node. Fires `HOT_TALENT_LEARN_RESPONSE`. |
| `CustomUnlearnTalent(nodeId)` | nothing | Unlearns a node. Fires `HOT_TALENT_UNLEARN_RESPONSE`. |
| `CustomResetTalents()` | nothing | Resets all talents. Fires `HOT_TALENT_RESET`. |
| `CustomInspectTalents(playerName)` | nothing | Requests another player's talents. Ignored if the name is empty. Fires `HOT_TALENT_INSPECT_RESPONSE`. |
| `CustomRequestTalentLoadouts()` | nothing | Requests the loadout list. Fires `HOT_TALENT_LOADOUTS`. |
| `CustomSwitchTalentLoadout(loadoutId)` | nothing | Switches to a loadout. |
| `CustomCreateTalentLoadout(name)` | nothing | Creates a loadout. Ignored if the name is empty. |
| `CustomDeleteTalentLoadout(loadoutId)` | nothing | Deletes a loadout. |
| `CustomRenameTalentLoadout(loadoutId, name)` | nothing | Renames a loadout. Ignored if the name is empty. |
| `CustomImportTalentBuild(nodeIds)` | nothing | Sends an array of node ids as a talent build. The argument must be a table. Only the first 1000 entries are used. |

The loadout functions respond with `HOT_TALENT_LOADOUT_RESULT`.

### Events

| Event | Args | Description |
|---|---|---|
| `HOT_TALENT_CACHE` | none | Full tree received. |
| `HOT_TALENT_SMALL_CACHE` | none | Points, level, and the learned and item-granted lists received. |
| `HOT_TALENT_ITEM_GRANTED_UPDATE` | none | Item-granted talents changed. |
| `HOT_TALENT_POINTS` | freePoints | Unspent points received. |
| `HOT_TALENT_LEVEL` | talentLevel | Talent level changed. |
| `HOT_TALENT_LEARN_RESPONSE` | nodeId, result | Response to `CustomLearnTalent`. **A `result` of 0 indicates success.** |
| `HOT_TALENT_UNLEARN_RESPONSE` | nodeId, result | Response to `CustomUnlearnTalent`. **A non-zero `result` indicates success.** This is the reverse of the learn response. |
| `HOT_TALENT_RESET` | freePoints | Talents were reset. |
| `HOT_TALENT_INSPECT_RESPONSE` | none | Inspect data received. |
| `HOT_TALENT_NEW` | nodeId | A new talent node is available. |
| `HOT_TALENT_LEARNT_UPDATE` | freePoints | The server replaced the learned talent list. |
| `HOT_TALENT_LOADOUTS` | activeLoadoutId | Loadout list received. |
| `HOT_TALENT_LOADOUT_RESULT` | op, result, loadoutId | Response to a loadout action. |

Loadout `op`: `1` switch, `2` create, `3` delete, `4` rename.

Loadout `result`: `0` success, `1` invalid, `2` not in Continuum, `3` busy, `4` too many loadouts, `5` invalid name, `6` loadout is active, `7` already active, `8` cast failed.

---

## Gem sockets and gem loadouts

The gem cache is not cleared on logout. Request fresh data after changing characters.

### Sockets

| Function | Returns | Description |
|---|---|---|
| `RequestGemSocketList()` | nothing | Requests the socket list. Fires `HOT_GEM_SOCKET_UPDATE`. |
| `GetGemSocketCount()` | number | Number of active sockets. |
| `GetGemSocketEntryCount()` | number | Number of socket entries received, active and deactivated. |
| `GetActiveFilledCount()` | number | Number of active sockets containing a gem. |
| `GetGemSocketInfo(slot)` | itemId, state | `slot` is the socket's slot id, not a list index. nil if no socket has that slot id. |
| `InsertGem(slot, itemId)` | nothing | Inserts a gem into a socket. Errors are reported through `HOT_GEM_SOCKET_ERROR`. |
| `RemoveGem(slot [, shatter])` | nothing | Removes a gem. If `shatter` is true the gem is destroyed instead of being returned for a fee. |
| `PurchaseGemSocket()` | nothing | Purchases an additional socket. |
| `RequestGemSocketCost()` | nothing | Requests the socket cost. Fires `HOT_GEM_SOCKET_COST_UPDATE`. |
| `GetGemSocketCost()` | costItemId, cost, purchased, canPurchase | Currency item, cost of the next socket, number of sockets purchased, and whether another can be purchased. nil before the cost is received. |

Socket `state`: `0` active and empty, `1` active and filled, `2` deactivated.

### Loadouts

| Function | Returns | Description |
|---|---|---|
| `RequestGemLoadouts()` | nothing | Requests the loadout list. Fires `HOT_GEM_LOADOUTS`. |
| `GetGemLoadouts()` | table | Array of `{ id, name, sockets, filled, purchased }`. Empty table if no data has been received. |
| `GetActiveGemLoadout()` | activeId, pendingId | `pendingId` is nil unless a loadout switch is being cast. |
| `GetMaxGemLoadouts()` | number | Maximum number of loadouts the character can own. |
| `GetGemLoadoutUnlockCost()` | itemId, cost, canUnlock | Cost of the next loadout. nil before data is received. |
| `SwitchGemLoadout(id)` | nothing | Starts the loadout switch cast. |
| `UnlockGemLoadout([name])` | nothing | Purchases a new loadout. If `name` is empty the server assigns a default name. |
| `RenameGemLoadout(id, name)` | nothing | Renames a loadout. Ignored if the name is empty. Maximum 24 characters. |

### Events

| Event | Args | Description |
|---|---|---|
| `HOT_GEM_SOCKET_UPDATE` | none | Socket list received. |
| `HOT_GEM_SOCKET_ERROR` | code | A socket operation failed. |
| `HOT_GEM_SOCKET_COST_UPDATE` | none | Socket cost received. |
| `HOT_GEM_SOCKET_OPEN` | none | The server requested that the gem panel be opened. |
| `HOT_GEM_SOCKET_USE` | itemId | The player used a gem with a socket spell from their bags while the gem system is enabled, the crafting window is closed and the item is not locked. The normal item use does not happen. |
| `HOT_GEM_LOADOUTS` | activeLoadoutId | Loadout list received. |
| `HOT_GEM_LOADOUT_RESULT` | op, result, loadoutId | Response to a switch, unlock or rename. |

Socket error `code`: `0` not found, `1` wrong slot, `2` inventory full, `3` not owned, `4` duplicate, `5` at cap, `6` cannot afford.

Loadout `op`: `1` switch, `2` unlock, `3` rename.

Loadout `result`: `0` success, `1` invalid, `2` not in Continuum, `3` busy, `4` too many loadouts, `5` invalid name, `6` already active, `7` cast failed, `8` cannot afford.

---

## Crafting

The crafting station window has two modes. Crafting mode combines a pattern, a material and up to 2 catalysts. Scrapping mode takes up to 9 items. Slot arguments are 1-based. Items placed in slots are locked in the player's bags until removed.

The window closes automatically if the player moves more than interaction range plus 1.5 yards from the station.

### State and data

| Function | Returns | Description |
|---|---|---|
| `CraftingIsOpen()` | boolean | True if the crafting window is open. |
| `CraftingClose()` | nothing | Closes the window, clears all slots and notifies the server. |
| `CraftingCanCraft()` | boolean | True if the window is open and the station allows crafting. |
| `CraftingGetMode()` | number | `0` crafting, `1` scrapping. |
| `CraftingSetMode(mode)` | nothing | Sets the mode on the client. Opening the window resets it to 0. |
| `CraftingIsBusy()` | boolean | True while a craft or scrap request is awaiting a response. |
| `CraftingCancelPending()` | nothing | Clears the pending state on the client only. The server is not notified. |
| `CraftingGetDataVersion()` | number | Version of the recipe data. |
| `CraftingGetScrapCurrency()` | number | The scrap currency id used by the server. |
| `CraftingGetNumPatterns()` | number | Number of patterns. |
| `CraftingGetPatternInfo(index)` | entry, category | Pattern item entry and category. nil if the index is invalid. |
| `CraftingGetNumMaterials()` | number | Number of materials. |
| `CraftingGetMaterialInfo(index)` | entry, category | Material item entry and category. |
| `CraftingGetNumRecipes()` | number | Number of recipes. |
| `CraftingGetRecipeInfo(index)` | patternEntry, materialEntry, resultEntry, scrapCost | Recipe details by index. |
| `CraftingGetRecipe(patternEntry, materialEntry)` | resultEntry, scrapCost | Result of a pattern and material combination. nil if the combination is not a recipe. |
| `CraftingGetCatalystInfo(itemEntry)` | type, value1, value2 | Takes an item entry, not a slot. `value1` and `value2` are raw server values whose meaning depends on the catalyst type. nil if the item is not a catalyst. |

Pattern, material and recipe data remains available while the window is closed.

### Selection and slots

| Function | Returns | Description |
|---|---|---|
| `CraftingSetPattern(patternEntry)` | nothing | Selects a pattern. An unknown entry or nil clears the selection. |
| `CraftingSetMaterial(materialEntry)` | nothing | Selects a material. An unknown entry or nil clears the selection. |
| `CraftingGetSelection()` | patternEntry, materialEntry | The current selection. Either value can be nil. |
| `CraftingGetCatalyst(slot)` | itemEntry, count, bag, bagSlot | Catalyst slot 1 or 2. `bag` and `bagSlot` are standard container numbers (`bagSlot` is 1-based), or nil if the item is not in the player's bags. Returns nil if the slot is empty. |
| `CraftingAddCursorCatalyst([slot])` | boolean | Places the cursor item in a catalyst slot. Without `slot`, the first free slot is used. Specifying an occupied slot replaces its item. |
| `CraftingRemoveCatalyst(slot)` | nothing | Clears a catalyst slot. |
| `CraftingGetScrapItem(slot)` | itemEntry, count, bag, bagSlot | Scrap slot 1 to 9. Same return values as `CraftingGetCatalyst`. |
| `CraftingGetNumScrapItems()` | number | Number of filled scrap slots. |
| `CraftingAddCursorScrapItem([slot])` | boolean | Places the cursor item in a scrap slot. |
| `CraftingRemoveScrapItem(slot)` | nothing | Clears a scrap slot. |
| `CraftingClearSlots([mode])` | nothing | Clears both slot groups by default, or only `0` catalysts or `1` scrap. A group with a pending request is skipped. |
| `CraftingCraft()` | boolean | Sends the craft request. Returns false if the window is closed, a request is pending, no pattern or material is selected, crafting is not allowed, or the combination is not a recipe. The response is `HOT_CRAFTING_RESULT`. |
| `CraftingScrap()` | boolean | Scraps all items in the scrap slots. Returns false if no items are slotted. The response is `HOT_CRAFTING_SCRAP_RESULT`. |

The add functions return false without firing an event if the window is closed or the cursor is empty. Other failures fire `HOT_CRAFTING_ERROR`. Two catalysts of the same type conflict, except level boost catalysts, which can be combined.

While the window is open, using an item from the bags has the same effect as the add function for the current mode.

### Events

| Event | Args | Description |
|---|---|---|
| `HOT_CRAFTING_DATA_UPDATE` | dataVersion | Recipe data received. |
| `HOT_CRAFTING_OPEN` | flags | Window opened. Bit `0x01` of `flags` indicates that crafting is allowed. Slots, mode and selection are reset before this event fires. |
| `HOT_CRAFTING_CLOSE` | none | Window closed. |
| `HOT_CRAFTING_SLOTS_UPDATE` | group | Slot contents changed. `0` catalysts, `1` scrap. |
| `HOT_CRAFTING_ERROR` | code | A client-side check failed. |
| `HOT_CRAFTING_RESULT` | code, resultEntry | Craft completed. On success the catalyst slots have already been cleared. |
| `HOT_CRAFTING_SCRAP_RESULT` | code, scrapped, rejected | Scrap completed. Skipped items remain in their slots and all other items are removed. |

If the server does not respond within 15 seconds, `HOT_CRAFTING_RESULT` or `HOT_CRAFTING_SCRAP_RESULT` fires with code `255`.

### Codes

`HOT_CRAFTING_ERROR` code: `1` slots full, `2` already added, `3` not a catalyst, `4` cannot be scrapped, `5` item locked, `6` busy, `7` catalyst conflict.

`HOT_CRAFTING_RESULT` code: `0` success, `1` not at station, `2` wrong game mode, `3` no recipe, `4` not enough scrap, `5` invalid catalyst, `6` inventory full, `7` busy, `8` internal error, `9` invalid item, `10` catalyst conflict, `11` catalyst not valid for this item type, `255` timed out.

Catalyst type: `1` minimum quality, `2` stat group, `3` level boost, `4` talent notable.

Pattern category: `1` weapon, `2` armor, `3` jewelry.

Material category: `1` cloth, `2` leather, `3` mail, `4` plate, `5` weapon, `6` jewelry.

---

## Transmog

Appearances are identified by the pair `(displayId, inventoryType)`. Slot arguments in this section are **0-based equipment slots**, one less than the stock inventory slot ids. Slot 15 is the main hand and 16 is the off hand. Applying an appearance consumes it and removes it from the collection.

| Function | Returns | Description |
|---|---|---|
| `TransmogRequestCollection([flags])` | nothing | Bit `0x01` requests the collection and bit `0x02` requests the current slot state. Without `flags`, both are requested. |
| `TransmogGetNumAppearances()` | number | Number of collected appearances. |
| `TransmogGetAppearanceInfo(index)` | 9 values | See below. Sorted by inventory type, then display id. |
| `TransmogGetAppearance(displayId, inventoryType)` | 9 values | Same as `TransmogGetAppearanceInfo` for a specific appearance. nil if it is not collected. |
| `TransmogGetSlotInfo(slot)` | currentDisplayId, currentSheath, originalDisplayId, originalSheath, isTransmogged | nil if the server did not send data for the slot. |
| `TransmogIsOpen()` | boolean | True if the transmog window is open. |
| `TransmogGetOpenInfo()` | boolean | True if appearances can be applied. |
| `TransmogApply(slot, displayId, inventoryType, ...)` | boolean | Accepts any number of slot, display id and inventory type triples. A `displayId` of 0 restores the original appearance. Processing stops at the first invalid triple. Returns false if the window is not open or no entries are valid. The response is `HOT_TRANSMOG_RESULT`. |
| `TransmogClose()` | nothing | Closes the window. `HOT_TRANSMOG_CLOSE` fires when the server confirms. |
| `TransmogGetDisplayIcon(displayId)` | string or nil | Icon path for a display id. |
| `DressUpModelTryOnDisplay(frame, displayId, inventoryType, sheath [, slot])` | true, or false and reason | Shows a display id on a DressUpModel without requiring an item. See below. |
| `DressUpModelTryOnSlot(frame, itemEntry [, slot])` | boolean | Equivalent to `frame:TryOn(itemEntry)`, with the option to force the main hand (`15`) or off hand (`16`). |

`TransmogGetAppearanceInfo` returns `displayId`, `inventoryType`, `sheath`, `itemClass`, `itemSubClass`, `quality`, `name`, `addedTime` and `icon`. `icon` can be nil.

**DressUpModelTryOnDisplay**
- `slot` is 15 (main hand), 16 (off hand), or omitted for automatic placement. Automatic placement puts shields, off-hand items, holdables and ranged weapons in the off hand, and everything else in the main hand.
- A two-handed weapon clears the other hand.
- On failure the reason is one of `"args"`, `"noframe"`, `"type"`, `"range"`, `"invtype"`, `"nocomponent"`, `"nomodel"`, `"nodisplay"`, `"nodisplay-removed"` or `"unknown"`.

| Event | Args | Description |
|---|---|---|
| `HOT_TRANSMOG_COLLECTION_UPDATE` | none | Collection replaced, or an appearance was removed. |
| `HOT_TRANSMOG_COLLECTION_ITEM_UPDATE` | displayId, inventoryType | An appearance was added. |
| `HOT_TRANSMOG_ACTIVE_UPDATE` | none | Slot state received. |
| `HOT_TRANSMOG_OPEN` | canApply | Window opened. `canApply` is 0 or 1. |
| `HOT_TRANSMOG_CLOSE` | none | Window closed. |
| `HOT_TRANSMOG_RESULT` | code, slot | Response to `TransmogApply`. `slot` is 255 when the result does not apply to a single slot. This event has no client-side timeout. |

Result code: `0` success, `1` window not open, `2` too far away (the window also closes), `3` invalid slot, `4` nothing equipped in the slot, `5` not collected or used twice in one request, `6` inventory type not valid for the slot, `7` nothing to apply, `8` disabled.

---

## Group Finder

The premade group finder. Missing number arguments are treated as 0, missing strings as empty, and missing booleans as false. Arguments named `index` are 1-based positions in the local cache, and arguments named `Id` are server ids. A search result id is the same as the listing id.

Server limits: titles 63 characters, comments 191 characters, 5 active applications, 100 search results.

### Activities

| Function | Returns | Description |
|---|---|---|
| `LFGListRequestActivities()` | nothing | Requests the activity list. Fires `HOT_LFG_LIST_ACTIVITIES_UPDATE`. Data is only sent if the cached version is out of date. |
| `LFGListGetActivityVersion()` | number | Cached activity data version. |
| `LFGListGetNumActivityGroups()` | number | Number of activity groups. |
| `LFGListGetActivityGroupInfo(index)` | groupId, categoryId, name, orderIndex | Activity group details. |
| `LFGListGetNumActivities()` | number | Number of activities. |
| `LFGListGetActivityInfo(index)` | 12 values | See below. |
| `LFGListGetActivityInfoByID(activityId)` | 12 values | Same as `LFGListGetActivityInfo`, looked up by id. |

Activity info returns `activityId`, `categoryId`, `groupId`, `fullName`, `shortName`, `minLevel`, `maxPlayers`, `itemLevelSuggestion`, `mapId`, `difficultyId`, `orderIndex` and `flags`.

Category: `1` 3 player, `2` 5 player, `3` raid, `4` other. Activity flags: `0x01` hidden (not offered when creating a listing), `0x02` automatic title (the title is fixed to the activity's full name).

### Listings

| Function | Returns | Description |
|---|---|---|
| `LFGListCreateListing(activityId, title, comment, requiredItemLevel, autoAccept)` | nothing | Creates a listing. The response is `HOT_LFG_LIST_ENTRY_RESULT`. |
| `LFGListUpdateListing(activityId, title, comment, requiredItemLevel, autoAccept)` | nothing | Updates the current listing. |
| `LFGListRemoveListing()` | nothing | Removes the current listing. |
| `LFGListGetActiveEntryInfo()` | 8 values or nil | Returns `listingId`, `activityId`, `title`, `comment`, `requiredItemLevel`, `autoAccept`, `ageSeconds` and `numApplications`, or nil if there is no listing. `ageSeconds` reflects the time the data was sent and does not update. |

Entry result: `0` success, `1` not group leader, `2` already listed, `3` unknown activity, `4` group too large, `5` level too low, `6` invalid title, `7` in the dungeon finder queue.

### Searching

| Function | Returns | Description |
|---|---|---|
| `LFGListSearch(categoryId, activityId, searchText, filterMask)` | nothing | Searches for listings. An `activityId` of 0 searches the whole category. The response is `HOT_LFG_LIST_SEARCH_RESULTS_RECEIVED`. |
| `LFGListGetNumSearchResults()` | number | Number of search results. |
| `LFGListGetSearchResultInfo(index)` | 11 values | See below. |
| `LFGListGetSearchResultMemberCounts(index)` | numMembers, maxMembers, numTanks, numHealers, numDamage | Group composition. |
| `LFGListGetNumSearchResultSubClasses(index)` | number | Number of distinct subclasses in the group. Returns 0 for an invalid index. |
| `LFGListGetSearchResultSubClassInfo(index, subClassIndex)` | subClass, count | A subclass and the number of members with it. |

Search result info returns `searchResultId`, `activityId`, `name`, `comment`, `leaderName`, `leaderSubClass`, `requiredItemLevel`, `ageSeconds`, `autoAccept`, `hasSelf` and `applicationStatus`.

Filter mask: `0x01` has tank, `0x02` has healer, `0x04` has damage, `0x08` needs the player's role, `0x10` player meets the item level requirement.

### Applying

| Function | Returns | Description |
|---|---|---|
| `LFGListApplyToGroup(searchResultId, comment, roleMask)` | nothing | Applies to a listing. |
| `LFGListCancelApplication(searchResultId)` | nothing | Cancels an application. |
| `LFGListGetApplicationStatus(searchResultId)` | number | Application status for a listing, looked up by id. 0 if there is no application. |
| `LFGListGetNumApplications()` | number | Number of active applications. |
| `LFGListGetApplicationInfo(index)` | searchResultId, status | Application details by index. |
| `LFGListAnswerInvite(listingId, accept)` | nothing | Accepts or declines an invitation. Accepting adds the player to the group directly. |

Role mask: `0x01` leader, `0x02` tank, `0x04` healer, `0x08` damage.

### Managing applicants

| Function | Returns | Description |
|---|---|---|
| `LFGListRefreshApplicants()` | nothing | Requests the applicant list. Fires `HOT_LFG_LIST_APPLICANT_LIST_UPDATED`. |
| `LFGListGetNumApplicants()` | number | Number of applicants. |
| `LFGListGetApplicantInfo(index)` | applicantId, status, comment, numMembers | Applicant details by index. |
| `LFGListGetApplicantMemberInfo(applicantIndex, memberIndex)` | name, level, subClass, roleMask, assignedRole, itemLevel | Details for one member of an applicant group. `applicantIndex` is the list index, not the applicant id. |
| `LFGListInviteApplicant(applicantId [, role1, role2, ...])` | nothing | Invites an applicant. Optionally assigns one role per member. Omitted roles keep the role the member applied with. |
| `LFGListDeclineApplicant(applicantId)` | nothing | Declines an applicant. |

Application status (used for both applications and applicants): `0` none, `1` applied, `2` invited, `3` invite accepted, `4` invite declined, `5` declined, `6` declined (group full), `7` declined (listing removed), `8` timed out, `9` cancelled, `10` failed.

Applications expire after 2 minutes, invitations after 1 minute and listings after 2 hours.

### Events

| Event | Args | Description |
|---|---|---|
| `HOT_LFG_LIST_ACTIVITIES_UPDATE` | none | Activity list received. |
| `HOT_LFG_LIST_ACTIVE_ENTRY_UPDATE` | none | The player's listing changed or was removed. When removed, the applicant list is also cleared. |
| `HOT_LFG_LIST_ENTRY_RESULT` | result | Response to creating or updating a listing. |
| `HOT_LFG_LIST_SEARCH_RESULTS_RECEIVED` | count | Search results received. |
| `HOT_LFG_LIST_SEARCH_RESULT_UPDATED` | searchResultId, delisted | A search result changed (`delisted` 0) or was removed (`delisted` 1). |
| `HOT_LFG_LIST_APPLICATION_STATUS_UPDATED` | searchResultId, status, listingRemoved | One of the player's applications changed. |
| `HOT_LFG_LIST_APPLICANT_LIST_UPDATED` | count | Applicant list received. |
| `HOT_LFG_LIST_APPLICANT_UPDATED` | applicantId, status | An applicant changed. |

The group finder cache is cleared when returning to character select.

### Dungeon finder queue

| Function | Returns | Description |
|---|---|---|
| `CustomLfgQueue(role, selections, premadeWait, affix1, affix2, affix3, affix4)` | nothing | Joins the HoT dungeon finder queue. `role` is a number, `selections` is a string, `premadeWait` is enabled only when set to `1`, followed by four affix ids. The format of `selections` is defined by the server. |

---

## Item generation data

Read-only data describing how items are generated. Counts return 0 before data is received.

| Function | Returns | Description |
|---|---|---|
| `RequestStatGroups()` | nothing | Requests stat groups. Fires `HOT_STAT_GROUPS`. |
| `GetStatGroupCount()` | number | Number of stat groups. |
| `GetStatGroupInfo(index)` | id, name, mask, flags, weight, trinketGroup | Stat group details. |
| `GetStatGroupMaskCount()` | number | Number of named mask bits. |
| `GetStatGroupMaskInfo(index)` | bit, name | Mask bit details. |
| `RequestLegendaries()` | nothing | Requests legendary definitions. Fires `HOT_LEGENDARIES`. |
| `GetLegendaryCount()` | number | Number of legendaries. |
| `GetLegendaryInfo(index)` | 13 values | `id`, `name`, `nameOverride`, `flags`, `minItemLevel`, `maxItemLevel`, `itemClass`, `itemSubClass`, `inventoryType`, `statGroup`, `statGroupMask`, `statGroupOverride`, `uniqueId`. |
| `RequestUniqueItems()` | nothing | Requests unique item definitions. Fires `HOT_UNIQUE_ITEMS`. |
| `GetUniqueItemCount()` | number | Number of unique items. |
| `GetUniqueItemInfo(index)` | 12 values | `id`, `name`, `description`, `quality`, `itemClass`, `subClass`, `inventoryType`, `statGroup`, `flags`, `itemLevelMin`, `itemLevelMax`, `spawnable`. |

| Event | Args | Description |
|---|---|---|
| `HOT_STAT_GROUPS` | none | Stat groups received. |
| `HOT_LEGENDARIES` | none | Legendaries received. |
| `HOT_UNIQUE_ITEMS` | none | Unique items received. |

## Item level breakdown

Per-slot item levels for the character panel. The data is cleared when entering the world, so request it again after a loading screen.

| Function | Returns | Description |
|---|---|---|
| `RequestItemLevelBreakdown()` | nothing | Requests the breakdown. Fires `HOT_ITEM_LEVEL_UPDATE`. |
| `HasItemLevelBreakdown()` | boolean | True once the breakdown has been received. |
| `GetItemLevelSubClass()` | number | The player's subclass, or 0 if no data. |
| `GetItemLevelBreakdownSlot(slot)` | number or nil | Item level of the item in the slot. **The slot is a 0-based equipment slot.** |
| `GetItemLevelBreakdownAverage()` | number or nil | Average item level calculated by the server. This value is not capped and can differ from item levels shown elsewhere. |

| Event | Args | Description |
|---|---|---|
| `HOT_ITEM_LEVEL_UPDATE` | none | Breakdown received. |

---

## Teleport locations

| Function | Returns | Description |
|---|---|---|
| `RequestGameTeles()` | nothing | Requests the teleport list. Fires `HOT_GAME_TELES`. |
| `GetGameTeleCount()` | number | Number of teleport locations. |
| `GetGameTeleInfo(index)` | teleId, name, mapId | Teleport location details. |

| Event | Args | Description |
|---|---|---|
| `HOT_GAME_TELES` | none | Teleport list received. |

## Tower defence

The caches are cleared when entering the world.

| Function | Returns | Description |
|---|---|---|
| `RequestTowerDefenceScoreboard(mode)` | nothing | Requests the scoreboard for a mode. Fires `HOT_TOWERDEFENCE_SCOREBOARD`. |
| `GetTowerDefenceScoreboard()` | mode, rows, or nil | `rows` is an array of `{ name, score, wave, money, lives, difficulty, playDate, highlighted }`. `highlighted` is true for the player's own run and nil otherwise. Up to 64 rows. |
| `RequestTowerDefenceSavedGames()` | nothing | Requests saved games. Fires `HOT_TOWERDEFENCE_SAVED_GAMES`. |
| `GetTowerDefenceSavedGames()` | table or nil | Array of `{ saveId, mode, wave, lives, difficulty }`. Up to 32 entries. |
| `RequestTowerDefenceTowers()` | nothing | Requests the tower list. Fires `HOT_TOWERDEFENCE_TOWERS`. |
| `GetTowerDefenceTowers(towerSet)` | table or nil | Array of `{ towerId, name, cost, icon }` for the given set. Sets are 1 and 2. |
| `TowerDefenceStartGame(difficulty, mode)` | nothing | Starts a new game. |
| `TowerDefenceLoadGame(saveId)` | nothing | Loads a saved game. |
| `TowerDefenceDeleteGame(saveId)` | nothing | Deletes a saved game. |
| `TowerDefenceEndGame()` | nothing | Ends the current game. |
| `TowerDefenceStartWave()` | nothing | Starts the next wave. |
| `TowerDefenceSpawnTower(towerId)` | nothing | Begins placing a tower. |
| `TowerDefenceConfirmTower()` | nothing | Confirms tower placement. |
| `TowerDefenceCancelTower()` | nothing | Cancels tower placement. |

Mode: `1` Beginners Tale, `2` Acolytes Feat, `3` Adepts Ruin. Difficulty: `1` Easy (50 lives), `2` Moderate (25 lives), `3` Hard (10 lives), `4` Insanity (1 life).

| Event | Args | Description |
|---|---|---|
| `HOT_TOWERDEFENCE_SCOREBOARD` | none | Scoreboard received. |
| `HOT_TOWERDEFENCE_SAVED_GAMES` | openUI | Saved games received. An `openUI` of 1 means the server requested that the window be opened. |
| `HOT_TOWERDEFENCE_TOWERS` | none | Tower list received. |
| `HOT_TOWERDEFENCE_MONEY` | amount | Money changed. |
| `HOT_TOWERDEFENCE_WAVE` | wave | Wave changed. |
| `HOT_TOWERDEFENCE_LIVES` | lives | Lives changed. |
| `HOT_TOWERDEFENCE_SCORE` | score | Score changed. |
| `HOT_TOWERDEFENCE_GAME_END` | none | The game ended. |

## Minigames

A generic channel between the UI and server-side minigame scripts. The meaning of each command, key and value is defined by the minigame script.

| Function | Returns | Description |
|---|---|---|
| `MinigameCommand(command [, arg1, arg2, arg3])` | nothing | Sends a command and up to three arguments to the server. Missing arguments are sent as 0. |

| Event | Args | Description |
|---|---|---|
| `HOT_MINIGAME_VALUE` | key, value | Fires once for each key and value pair sent by the server. `value` can be negative. |
| `HOT_MINIGAME_TEXT` | key, text | A text value sent by the server. |

## Test frame

A server-driven test frame.

| Function | Returns | Description |
|---|---|---|
| `GetTestFrameData()` | name, shortDesc, longDesc, talentLevel, avgItemLevel | nil before data is received. |
| `GetTestFrameAllowedStatGroups()` | table | Array of stat group ids. Empty before data is received. |
| `SendTestFrameReply(v1, v2, v3, v4, v5)` | nothing | Sends five numbers to the server. Their meaning is defined by the server. |

| Event | Args | Description |
|---|---|---|
| `HOT_TEST_FRAME` | none | Test frame data received. |

---

## Streaming, launcher settings and HD models

Functions for the background downloader and the settings shared with the launcher.

| Function | Returns | Description |
|---|---|---|
| `IsStreaming()` | boolean | True while a download pass is running. |
| `IsStreamingBusy()` | boolean | True while a pass is running or the first pass has not finished. |
| `GetStreamingProgress()` | filesDone, filesTotal, bytesDone, bytesTotal, currentFile | Download progress. `currentFile` can be empty. |
| `IsStreamingRestartPending()` | boolean | True if a client restart is required to apply downloaded files. |
| `IsStreamingUIRefreshPending()` | boolean | True if interface files changed and a UI reload would apply them. |
| `StreamingRefreshUI()` | nothing | Reloads the UI to apply new interface files. |
| `IsHDPatchEnabled()` | boolean | True if the HD patch is enabled. |
| `SetHDPatchEnabled(enabled)` | nothing | Enables or disables the HD patch, saves the setting, starts any needed downloads, and switches models if the assets are present. Fires `UNIT_MODEL_CHANGED` for "player" when models change. |
| `AreHDCharacterModelsAvailable()` | boolean | True if the HD character assets are installed. |
| `GetDownloadSpeedLimit()` | number | Download limit in MB/s. 0 means unlimited. |
| `SetDownloadSpeedLimit(mbps)` | nothing | Sets the download limit in MB/s. Negative values are treated as 0. |
| `GetMpqList()` | nothing | Prints the installed MPQ files and their digests to chat once the scan completes. Useful for support requests. |
| `GetPatchChannel()` | string | The data channel currently mounted, `"release"` or `"dev"`. The connected realm picks it, Lua can only read it. |
| `IsPatchChannelReady()` | boolean | False while the data the realm asked for is still downloading or being swapped in. Enter World is refused while this is false, and the streaming progress functions above show the download. |

| Event | Args | Description |
|---|---|---|
| `HOT_STREAMING_STARTED` | none | A download pass started. |
| `HOT_STREAMING_STOPPED` | none | A download pass stopped. |
| `HOT_PATCH_CHANNEL_CHANGED` | channel | The mounted data switched to another channel and the refresh has run. `channel` is `"release"` or `"dev"`. |
| `UNIT_MODEL_CHANGED` | "player" | Stock event, also fired after the HD patch setting changes the loaded models. |

## World resume

If the connection to the game server drops, the client keeps the world loaded and attempts to reconnect before returning to the login screen.

| Function | Returns | Description |
|---|---|---|
| `WorldResumeIsActive()` | boolean | True while the client is holding the world and reconnecting. |
| `WorldResumeSecondsLeft()` | number | Seconds remaining before the reconnect attempt ends. 0 if not active. |
| `WorldResumeCancel()` | true | Stops reconnecting and returns to the login screen on the next frame. |

Settings: `worldResumeKeepWorld` (default 1) and `worldResumeWindow` (seconds, default 60).

---

## Other events

| Event | Args | Description |
|---|---|---|
| `HOT_STAT_UPDATE` | none | Custom stats changed. See Custom stats. |
| `HOT_COMBO_POINT_ANCHOR` | value | The `comboPointAnchor` cvar changed. `value` is `"player"` or `"target"`. |

## Console variables

| Cvar | Default | Description |
|---|---|---|
| `enableDiscord` | 1 | Enables Discord Rich Presence. |
| `comboPointAnchor` | player | Where combo points are displayed, `player` or `target`. |
| `lootStayOpenWhileMoving` | 1 | Keeps the loot window open while moving. It closes beyond 45 yards instead. |
| `actionButtonHoldRepeat` | 0 | Repeat actions while the key is held. 0 off, 1 until the spell casts, 2 continuous. |
| `actionButtonHoldRepeatDelay` | 500 | Delay in ms before repeating starts. |
| `actionButtonHoldRepeatInterval` | 100 | Interval in ms between repeats. |
| `healPredictionAbsorb` | 1 | Shows absorb shields on health bars. |
| `healPredictionAbsorbFullHealth` | 0 | Shows absorb shields when the unit is at full health. |
| `healPredictionAbsorbText` | 0 | Shows the absorb amount as text. |
| `healPredictionHealText` | 0 | Shows incoming heals as text. |
| `worldResumeKeepWorld` | 1 | Keeps the world loaded when the connection drops. |
| `worldResumeWindow` | 60 | Seconds to attempt reconnecting. |
