#ifndef HOT_FEATURES_ITEMS_H
#define HOT_FEATURES_ITEMS_H

#include <cstdint>

class ItemRec;
class ItemStats_C;

namespace HoT {
class Module;
}

// Item records for ids the client Item.dbc does not know, filled from the server and the item cache.
namespace HoT::Items {

// The item cache first, then server pushed records, then the Item.dbc row.
ItemRec* Resolve(int32_t id, ItemRec* dbcRecord);

// Queries the item cache for a unit virtual item that resolved to nothing, and redresses the units holding it once the record arrives.
void RequestVirtualItem(int32_t id);

// The heroic tooltip line key, replaced by the HoT legendary, heirloom, paradoxical or corrupted label from the item's second flags.
const char* GetHeroicLabelKey(const ItemStats_C* stats, const char* defaultKey);

Module* GetModule();

}

#endif
