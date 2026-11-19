#ifndef HOT_DB_REFRESH_H
#define HOT_DB_REFRESH_H

namespace HoT {
class Module;
}

// Rebuilds the client lookups derived from a table after HoT::DB::ReloadTable swaps its rows.
namespace HoT::DB {

Module* GetRefreshModule();

// Grows the known spell bit array when Spell.dbc gains ids past the old maximum.
void GrowKnownSpellBits();

}

#endif
