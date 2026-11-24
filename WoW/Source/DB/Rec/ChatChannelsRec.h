#ifndef WOW_SOURCE_DB_REC_CHATCHANNELSREC_H
#define WOW_SOURCE_DB_REC_CHATCHANNELSREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

enum CHAT_CHANNEL_FLAGS : int32_t {
    CHAT_CHANNEL_FLAG_NONE               = 0x00000000,
    CHAT_CHANNEL_FLAG_AUTO_JOIN          = 0x00000001,
    CHAT_CHANNEL_FLAG_ZONE_BASED         = 0x00000002,
    CHAT_CHANNEL_FLAG_ONLY_IN_CITIES     = 0x00000010,
    CHAT_CHANNEL_FLAG_LINKED_CHANNEL     = 0x00000020,
    CHAT_CHANNEL_FLAG_ZONE_ATTACK_ALERTS = 0x00010000,
    CHAT_CHANNEL_FLAG_GUILD_RECRUITMENT  = 0x00020000,
};

class ChatChannelsRec {
    public:
    static const int32_t s_numColumns = 37;
    static const int32_t s_rowSize = 148;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_flags;
    /* 0008 */ int32_t m_factionGroup;
    /* 000C */ const char* m_name;
    /* 0010 */ const char* m_shortcut;

    ChatChannelsRec();
    ~ChatChannelsRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
