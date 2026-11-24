#include "LFGHandler.h"
#include "Engine/Source/Base/CDataStore.h"
#include "WoW/Source/Ui/LFGLIST.h"

// VERIFY: cb07f000 auto
// OFFSET: 0x00553620
void LFGHandler::ReadGroupInfo(CDataStore* msg, LFGListParty* party) {
    uint32_t flags = 0;
    msg->Get(flags);

    if (flags & LFG_LIST_UPDATE_COMMENT) {
        msg->GetString(party->m_comment, sizeof(party->m_comment));
    }

    if (flags & LFG_LIST_UPDATE_ROLES) {
        for (uint32_t i = 0; i < 3; i++) {
            msg->Get(party->m_roles[i]);
        }
    }

    if (flags & LFG_LIST_UPDATE_ENCOUNTERS) {
        msg->Get(party->unk_0128);
        msg->Get(party->m_encounterMask);
    }
}

// VERIFY: 41fc65c5 manual
// OFFSET: 0x00559050
void LFGHandler::ReadPlayerInfo(CDataStore* msg, LFGLIST* list, LFGListPlayer* player) {
    uint32_t flags = 0;
    msg->Get(flags);

    if (flags & LFG_LIST_UPDATE_CHARACTER_INFO) {
        msg->Get(player->m_level);
        msg->Get(player->m_class);
        msg->Get(player->m_race);

        for (uint32_t i = 0; i < MAX_TALENT_TABS; i++) {
            msg->Get(player->m_talents[i]);
        }

        msg->Get(player->m_armor);
        msg->Get(player->m_spellDamage);
        msg->Get(player->m_spellHeal);
        msg->Get(player->m_critMelee);
        msg->Get(player->m_critRanged);
        msg->Get(player->m_critSpell);
        msg->Get(player->m_mp5);
        msg->Get(player->m_mp5Combat);
        msg->Get(player->m_attackPower);
        msg->Get(player->m_agility);
        msg->Get(player->m_maxHealth);
        msg->Get(player->m_maxMana);
        msg->Get(player->unk_0064);
        msg->Get(player->m_avgItemLevel);
        msg->Get(player->m_defenseSkill);
        msg->Get(player->m_dodge);
        msg->Get(player->m_block);
        msg->Get(player->m_parry);
        msg->Get(player->m_haste);
        msg->Get(player->m_expertise);
    }

    if (flags & LFG_LIST_UPDATE_COMMENT) {
        msg->GetString(player->m_comment, sizeof(player->m_comment));
    }

    if (flags & LFG_LIST_UPDATE_LEADER) {
        uint32_t wasLeader = player->m_leader;
        uint8_t leader = 0;
        msg->Get(leader);
        player->m_leader = leader;

        if ((wasLeader != 0) != (leader != 0) && player->m_party) {
            player->m_party->RemovePlayer(player);
            player->m_party->AddPlayer(player);
        }
    }

    if (flags & LFG_LIST_UPDATE_GROUP_GUID) {
        uint64_t current = player->m_party ? player->m_party->m_key.m_guid.m_guid : 0;
        uint64_t guid = 0;
        msg->Get(guid);

        if (current != guid) {
            list->RemovePlayer(player);
            LFGListParty* party = guid ? list->m_parties.Ptr(static_cast<uint32_t>(guid), CHashKeyGUID(guid)) : nullptr;
            player->m_party = party;

            if (party) {
                party->AddPlayer(player);
            } else if (player->m_index < 0) {
                uint32_t count = list->m_playerList.Count();
                list->m_playerList.SetCount(count + 1);
                list->m_playerList.m_data[count] = player;
                player->m_index = count;
            }
        }
    }

    if (flags & LFG_LIST_UPDATE_ROLES) {
        msg->Get(player->m_roles);
    }

    if (flags & LFG_LIST_UPDATE_AREA) {
        msg->Get(reinterpret_cast<uint32_t&>(player->m_areaID));
    }

    if (flags & LFG_LIST_UPDATE_STATUS) {
        msg->Get(player->unk_0088);
    }

    if (flags & LFG_LIST_UPDATE_ENCOUNTERS) {
        msg->Get(player->unk_0198);
        msg->Get(player->m_encounterMask);
    }
}
