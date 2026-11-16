#include "CalendarEvent.h"
#include <cstdlib>
#include "WoW/Source/Ui/Calendar.h"
#include "Engine/Source/Base/CDataStore.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Storm/H/storm.h"
#include "WoW/Source/DB/DBClient/DBCacheInstances.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/ChrClassesRec.h"
#include "WoW/Source/Game/GameClient/NameCache.h"
#include "WoW/Source/Game/GameTime.h"
#include "WoW/Source/Game/ValidateName.h"
#include "WoW/Source/Net/Types.h"
#include "WoW/Source/Object/Object.h"
#include "WoW/Source/Object/Player.h"
#include "WoW/Source/Object/Unit.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/RaidInfo.h"
#include "WoW/Source/Ui/Types.h"
#include "WoW/Source/WowSvcs/WowSvcsClient/ClientServices.h"

// STATIC: 0x00AD1684
static const char* s_eventRepeatStrings[4] = { "CALENDAR_REPEAT_NEVER", "CALENDAR_REPEAT_WEEKLY", "CALENDAR_REPEAT_BIWEEKLY", "CALENDAR_REPEAT_MONTHLY" };

// STATIC: 0x00AD1694
static const char* s_eventTypeStrings[5] = { "CALENDAR_TYPE_RAID", "CALENDAR_TYPE_DUNGEON", "CALENDAR_TYPE_PVP", "CALENDAR_TYPE_MEETING", "CALENDAR_TYPE_OTHER" };

// STATIC: 0x00AD16A8
static const char* s_eventStatusStrings[9] = { "CALENDAR_STATUS_INVITED", "CALENDAR_STATUS_ACCEPTED", "CALENDAR_STATUS_DECLINED", "CALENDAR_STATUS_CONFIRMED", "CALENDAR_STATUS_OUT", "CALENDAR_STATUS_STANDBY", "CALENDAR_STATUS_SIGNEDUP", "CALENDAR_STATUS_NOT_SIGNEDUP", "CALENDAR_STATUS_TENTATIVE" };

// STATIC: 0x00AD16CC
static const int32_t s_inviteStatusOrder[9] = { 9, 3, 6, 1, 7, 4, 2, 8, 5 };

// STATIC: 0x00AD16F0
static uint32_t s_inviteSortType;

// STATIC: 0x00C243B4
static InviteSortCriterion s_inviteSortCriteria[6];

// VERIFY: 92e9c97a auto
// OFFSET: 0x005F1000
const char* GetEventStatusString(uint32_t status) {
    if (status == 7 || status >= 9) {
        return nullptr;
    }

    return s_eventStatusStrings[status];
}

// VERIFY: 4baec31b manual
// OFFSET: 0x005F1020
const char* GetEventTypeString(uint32_t type) {
    return type < 5 ? s_eventTypeStrings[type] : nullptr;
}

// VERIFY: 92b48cfb manual
// OFFSET: 0x005F1040
const char* GetEventRepeatString(uint32_t repeat) {
    return repeat < 4 ? s_eventRepeatStrings[repeat] : nullptr;
}

// VERIFY: d2656ad2 manual
// OFFSET: 0x005F1060
void CGCalendarEvent::Clear() {
    this->m_eventID = 0;
    this->m_numInvites = 0;
    this->m_selectedInviteID = 0;
    this->m_moderatorInviteID = 0;
    this->m_title[0] = '\0';
    this->m_description[0] = '\0';
    this->m_type = 0;
    this->m_repeatOption = 0;
    this->m_maxSize = 100;
    this->m_flags = 0;
    this->m_new = 0;
    this->m_changed = 0;
    this->m_creatorGUID = WOWGUID();
    this->m_lockoutDate.m_minute = 0;
    this->m_lockoutDate.m_hour = 0;
    this->m_lockoutDate.m_weekDay = 0;
    this->m_lockoutDate.m_monthDay = 0;
    this->m_lockoutDate.m_month = 0;
    this->m_lockoutDate.m_year = 0;
    this->m_lockoutDate.m_flags = 0;
    this->m_lockoutDate.m_tzHours = 0;
    this->m_textureID = -1;
    this->m_pendingNames = 0;
    this->m_signalType = 1;
    this->m_sortType = 3;

    for (uint32_t i = 0; i < 6; i++) {
        this->m_sortCriteria[i].m_criterion = i;
        this->m_sortCriteria[i].m_reverse = 0;
    }
}

// VERIFY: 29a5cc3a auto
// OFFSET: 0x005F1130
void CGCalendarEvent::SetTitle(const char* title) {
    if (title && SStrCmp(title, this->m_title, STORM_MAX_STR)) {
        ScrubString(title, this->m_title, 32, sizeof(this->m_title), 1);
        this->m_changed = 1;
    }
}

// VERIFY: e8f3857e auto
// OFFSET: 0x005F1180
void CGCalendarEvent::SetDescription(const char* description) {
    if (description && SStrCmp(description, this->m_description, STORM_MAX_STR)) {
        ScrubString(description, this->m_description, 256, sizeof(this->m_description), 0);
        this->m_changed = 1;
    }
}

// VERIFY: d2c0193e auto
// OFFSET: 0x005F11D0
void CGCalendarEvent::SetType(uint32_t type) {
    if (type < 5 && type != this->m_type) {
        this->m_type = type;
        this->m_changed = 1;
    }
}

// VERIFY: 36d2d1c6 auto
// OFFSET: 0x005F1200
void CGCalendarEvent::SetRepeatOption(uint32_t repeatOption) {
    if (repeatOption < 4 && repeatOption != this->m_repeatOption) {
        this->m_repeatOption = repeatOption;
        this->m_changed = 1;
    }
}

// VERIFY: 88a5717b auto
// OFFSET: 0x005F1230
void CGCalendarEvent::SetSize(uint32_t size) {
    if (!(this->m_flags & 0x440) && size > 1 && size != this->m_maxSize) {
        this->m_maxSize = size;
        this->m_changed = 1;
    }
}

// VERIFY: 6d782fef auto
// OFFSET: 0x005F1260
bool CGCalendarEvent::SetDate(uint32_t month, uint32_t monthDay, uint32_t year) {
    if (month != this->m_date.m_month || monthDay != this->m_date.m_monthDay || year != this->m_date.m_year) {
        this->m_changed = 1;
    }

    return this->m_date.SetDate(month, monthDay, year);
}

// VERIFY: 0faa2884 auto
// OFFSET: 0x005F12A0
bool CGCalendarEvent::SetTime(uint32_t hour, uint32_t minute) {
    if (hour != this->m_date.m_hour || minute != this->m_date.m_minute) {
        this->m_changed = 1;
    }

    return this->m_date.SetHourAndMinutes(hour, minute);
}

// VERIFY: 908baeb8 auto
// OFFSET: 0x005F12E0
bool CGCalendarEvent::SetLockoutDate(uint32_t month, uint32_t monthDay, uint32_t year) {
    if (this->m_flags & 0x440) {
        return false;
    }

    if (month != this->m_lockoutDate.m_month || monthDay != this->m_lockoutDate.m_monthDay || year != this->m_lockoutDate.m_year) {
        this->m_changed = 1;
    }

    return this->m_lockoutDate.SetDate(month, monthDay, year);
}

// VERIFY: f9a4b158 auto
// OFFSET: 0x005F1340
bool CGCalendarEvent::SetLockoutTime(uint32_t hour, uint32_t minute) {
    if (this->m_flags & 0x440) {
        return false;
    }

    if (hour != this->m_lockoutDate.m_hour || minute != this->m_lockoutDate.m_minute) {
        this->m_changed = 1;
    }

    return this->m_lockoutDate.SetHourAndMinutes(hour, minute);
}

// VERIFY: fe57aa62 auto
// OFFSET: 0x005F1390
void CGCalendarEvent::SetTextureID(int32_t textureID) {
    if (textureID != this->m_textureID) {
        this->m_textureID = textureID;
        this->m_changed = 1;
    }
}

// VERIFY: dd189781 auto
// OFFSET: 0x005F13B0
void CGCalendarEvent::SetFlag(uint32_t flag) {
    if (!(this->m_flags & flag)) {
        this->m_flags |= flag;
        this->m_changed = 1;
    }
}

// VERIFY: 86c68769 auto
// OFFSET: 0x005F13E0
void CGCalendarEvent::ClearFlag(uint32_t flag) {
    if (this->m_flags & flag) {
        this->m_flags &= ~flag;
        this->m_changed = 1;
    }
}

// VERIFY: eec371c3 auto
// OFFSET: 0x005F1410
void CGCalendarEvent::SetSignalType(uint32_t signalType) {
    this->m_signalType = signalType;
}

// VERIFY: 6949882a auto
// OFFSET: 0x005F14B0
bool CGCalendarEvent::ProcessUpdateEvent(CDataStore* msg) {
    msg->Get(this->m_flags);
    uint32_t date;
    msg->Get(date);
    uint8_t type;
    msg->Get(type);
    this->m_type = type;
    msg->Get(reinterpret_cast<uint32_t&>(this->m_textureID));
    msg->GetString(this->m_title, sizeof(this->m_title));
    msg->GetString(this->m_description, sizeof(this->m_description));
    uint8_t repeatOption;
    msg->Get(repeatOption);
    this->m_repeatOption = repeatOption;
    msg->Get(this->m_maxSize);
    uint32_t lockoutDate;
    msg->Get(lockoutDate);
    WowTime::WowDecodeTime(date, &this->m_date);
    WowTime::WowDecodeTime(lockoutDate, &this->m_lockoutDate);
    return true;
}

// VERIFY: 614ecd10 manual
// OFFSET: 0x005F1580
void CGCalendarEvent::InviteRemoved(WOWGUID guid) {
    uint32_t count = this->m_numInvites;

    if (!count) {
        return;
    }

    uint32_t index = 0;
    EventInviteInfo* invite;

    for (;;) {
        if (this->m_invites.m_data[index]->m_guid == guid) {
            invite = this->m_invites.m_data[index];
            break;
        }

        if (++index >= count) {
            invite = nullptr;
            break;
        }
    }

    if (index >= count) {
        return;
    }

    for (this->m_numInvites = count - 1; index < this->m_numInvites; index++) {
        this->m_invites.m_data[index] = this->m_invites.m_data[index + 1];
    }

    this->m_invites.m_data[index] = invite;
}

// VERIFY: 8174b978 manual
// OFFSET: 0x005F1600
void CGCalendarEvent::UpdateInviteStatus(WOWGUID guid, uint8_t status, uint32_t responseTime) {
    for (uint32_t i = 0; i < this->m_numInvites; i++) {
        if (this->m_invites.m_data[i]->m_guid == guid) {
            this->m_invites.m_data[i]->m_status = status;

            if (responseTime) {
                this->m_invites.m_data[i]->m_responseTime = responseTime;
            }

            FrameScript_SignalEvent(SCRIPT_EVENT_CALENDAR_UPDATE_INVITE_LIST, nullptr);
            return;
        }
    }
}

// VERIFY: 81059cc6 manual
// OFFSET: 0x005F1670
void CGCalendarEvent::UpdateInviteNotes(WOWGUID guid, const char* notes) {
    for (uint32_t i = 0; i < this->m_numInvites; i++) {
        if (this->m_invites.m_data[i]->m_guid == guid) {
            SStrCopy(this->m_invites.m_data[i]->m_notes, notes, sizeof(this->m_invites.m_data[i]->m_notes));
            FrameScript_SignalEvent(SCRIPT_EVENT_CALENDAR_UPDATE_INVITE_LIST, nullptr);
            return;
        }
    }
}

// VERIFY: 465fbdb7 manual
// OFFSET: 0x005F16E0
bool CGCalendarEvent::CanModerate(uint32_t index) {
    if (index >= this->m_numInvites) {
        return false;
    }

    if ((this->m_flags & 0x400) && !this->m_invites.m_data[index]->m_inviteType) {
        CGCalendar::CalendarError("CALENDAR_ERROR_NO_MODERATOR", 0);
        return false;
    }

    return true;
}

// VERIFY: 84687b6b manual
// OFFSET: 0x005F1720
static int32_t QSortInvites(const void* a, const void* b) {
    const EventInviteInfo* left = *static_cast<EventInviteInfo* const*>(a);
    const EventInviteInfo* right = *static_cast<EventInviteInfo* const*>(b);
    int32_t result = 0;

    switch (s_inviteSortType) {
        case 0: {
            WOWGUID leftKey = WOWGUID();
            NameCache* leftName = g_nameDBCache.GetRecord(left->m_guid.m_guid, leftKey, nullptr, nullptr, false);
            WOWGUID rightKey = WOWGUID();
            NameCache* rightName = g_nameDBCache.GetRecord(right->m_guid.m_guid, rightKey, nullptr, nullptr, false);

            if (leftName && rightName) {
                result = SStrCmpUTF8I(leftName->m_name, rightName->m_name, STORM_MAX_STR);
            }

            break;
        }

        case 1:
            result = left->m_level - right->m_level;
            break;

        case 2: {
            WOWGUID leftKey = WOWGUID();
            NameCache* leftName = g_nameDBCache.GetRecord(left->m_guid.m_guid, leftKey, nullptr, nullptr, false);
            WOWGUID rightKey = WOWGUID();
            NameCache* rightName = g_nameDBCache.GetRecord(right->m_guid.m_guid, rightKey, nullptr, nullptr, false);

            if (leftName && rightName) {
                ChrClassesRec* leftClass = g_chrClassesDB.GetRecord(leftName->m_class);
                ChrClassesRec* rightClass = g_chrClassesDB.GetRecord(rightName->m_class);

                if (leftClass && rightClass) {
                    result = SStrSortUTF8I(leftClass->m_name, rightClass->m_name, STORM_MAX_STR);
                }
            }

            break;
        }

        case 3: {
            const uint32_t numStatuses = sizeof(s_inviteStatusOrder) / sizeof(s_inviteStatusOrder[0]);
            int32_t leftOrder = left->m_status < numStatuses ? s_inviteStatusOrder[left->m_status] : 0;
            int32_t rightOrder = right->m_status < numStatuses ? s_inviteStatusOrder[right->m_status] : 0;
            result = leftOrder - rightOrder;
            break;
        }

        case 4:
            if (CGRaidInfo::FindMemberByGuid(left->m_guid)) {
                if (!CGRaidInfo::FindMemberByGuid(right->m_guid)) {
                    result = -1;
                }
            } else if (CGRaidInfo::FindMemberByGuid(right->m_guid)) {
                result = 1;
            }

            break;

        case 5:
            result = SStrSortUTF8I(left->m_notes, right->m_notes, STORM_MAX_STR);
            break;
    }

    if (s_inviteSortCriteria[s_inviteSortType].m_reverse) {
        result = -result;
    }

    if (s_inviteSortType && !result) {
        WOWGUID leftKey = WOWGUID();
        NameCache* leftName = g_nameDBCache.GetRecord(left->m_guid.m_guid, leftKey, nullptr, nullptr, false);
        WOWGUID rightKey = WOWGUID();
        NameCache* rightName = g_nameDBCache.GetRecord(right->m_guid.m_guid, rightKey, nullptr, nullptr, false);

        if (leftName && rightName) {
            return SStrCmpUTF8I(leftName->m_name, rightName->m_name, STORM_MAX_STR);
        }

        return 0;
    }

    return result;
}

// VERIFY: 90a7ac86 manual
// OFFSET: 0x005F1980
bool CGCalendarEvent::SortInvites(uint32_t criterion, bool toggle) {
    for (uint32_t i = 0; i < 6; i++) {
        if (i == criterion) {
            if (toggle) {
                this->m_sortCriteria[i].m_reverse = !this->m_sortCriteria[i].m_reverse;
            }

            this->m_sortType = i;
        } else {
            this->m_sortCriteria[i].m_reverse = 0;
        }

        s_inviteSortCriteria[i].m_criterion = this->m_sortCriteria[i].m_criterion;
        s_inviteSortCriteria[i].m_reverse = this->m_sortCriteria[i].m_reverse;
    }

    s_inviteSortType = this->m_sortType;
    qsort(this->m_invites.m_data, this->m_numInvites, sizeof(EventInviteInfo*), QSortInvites);
    return true;
}

// VERIFY: 73a7a685 auto
// OFFSET: 0x005F1A00
void CGCalendarEvent::SelectInvite(uint32_t index) {
    if (index > this->m_numInvites) {
        return;
    }

    for (uint32_t i = 0; i < this->m_numInvites; i++) {
        if (i == index) {
            this->m_selectedInviteID = this->m_invites.m_data[i]->m_inviteID;
            return;
        }
    }
}

// VERIFY: 23acd6ad auto
// OFFSET: 0x005F1B10
static void InviteNameCacheCallback(int32_t id, const WOWGUID& guid, void* param, bool success) {
    CGCalendarEvent* event = static_cast<CGCalendarEvent*>(param);

    if (!event) {
        return;
    }

    if (event->m_pendingNames) {
        event->m_pendingNames--;
    }

    if (event->m_pendingNames) {
        return;
    }

    event->SortInvites(event->m_sortType, false);

    switch (event->m_signalType) {
        case 0:
            FrameScript_SignalEvent(SCRIPT_EVENT_CALENDAR_UPDATE_INVITE_LIST, nullptr);
            break;

        case 1:
            FrameScript_SignalEvent(SCRIPT_EVENT_CALENDAR_UPDATE_INVITE_LIST, "%b", 1);
            break;

        case 2:
            FrameScript_SignalEvent(SCRIPT_EVENT_CALENDAR_NEW_EVENT, "%b", 0);
            break;

        case 3:
            FrameScript_SignalEvent(SCRIPT_EVENT_CALENDAR_NEW_EVENT, "%b", 1);
            break;

        case 4:
            FrameScript_SignalEvent(SCRIPT_EVENT_CALENDAR_OPEN_EVENT, "%s", CGCalendar::GetCalendarTypeString(event->m_flags));
            break;
    }
}

// VERIFY: ee9836e6 manual
// OFFSET: 0x005F1B30
bool CGCalendarEvent::IsModerator() {
    CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
    WOWGUID guid = player ? player->m_obj->m_guid : WOWGUID();
    return this->m_moderatorInviteID || guid == this->m_creatorGUID;
}

// VERIFY: f351fca2 manual
// OFFSET: 0x005F1B90
bool CGCalendarEvent::AddEvent() {
    if (!this->m_new) {
        return false;
    }

    if (!this->m_title[0]) {
        CGCalendar::CalendarError("CALENDAR_ERROR_NEEDS_TITLE", 0);
        return false;
    }

    if (this->m_flags & 0x440) {
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

        if (player && !player->m_player->m_playerGuildid) {
            CGCalendar::CalendarError("ERR_GUILD_PLAYER_NOT_IN_GUILD", 0);
            return false;
        }
    } else {
        if (!this->m_maxSize) {
            return false;
        }

        if (this->m_maxSize > 100) {
            CGCalendar::CalendarError("CALENDAR_ERROR_INVITES_EXCEEDED", 100);
            return false;
        }
    }

    if (this->m_date.m_hour == -1 || this->m_date.m_minute == -1) {
        CGCalendar::CalendarError("CALENDAR_ERROR_INVALID_TIME", 0);
        return false;
    }

    if (this->m_date.m_month == -1 || this->m_date.m_monthDay == -1 || this->m_date.m_year == -1) {
        CGCalendar::CalendarError("CALENDAR_ERROR_INVALID_DATE", 0);
        return false;
    }

    if (this->m_date <= g_clientGameTime) {
        CGCalendar::CalendarError("CALENDAR_ERROR_EVENT_TIME_PASSED", 0);
        return false;
    }

    uint32_t date;
    WowTime::WowEncodeTime(date, &this->m_date);
    uint32_t lockoutDate;
    WowTime::WowEncodeTime(lockoutDate, &this->m_lockoutDate);

    CDataStore msg;
    msg.Put(static_cast<uint32_t>(CMSG_CALENDAR_ADD_EVENT));
    msg.PutString(this->m_title);
    msg.PutString(this->m_description);
    msg.Put(static_cast<uint8_t>(this->m_type));
    msg.Put(static_cast<uint8_t>(this->m_repeatOption));
    msg.Put(this->m_maxSize);
    msg.Put(static_cast<uint32_t>(this->m_textureID));
    msg.Put(date);
    msg.Put(lockoutDate);
    msg.Put(this->m_flags);

    if (!(this->m_flags & 0x40)) {
        msg.Put(this->m_numInvites);

        for (uint32_t i = 0; i < this->m_numInvites; i++) {
            SmartGUID guid;
            guid.m_guid = this->m_invites.m_data[i]->m_guid.m_guid;
            msg << guid;
            msg.Put(this->m_invites.m_data[i]->m_status);
            msg.Put(this->m_invites.m_data[i]->m_moderatorStatus);
        }
    }

    msg.Finalize();
    ClientServices::Send2(&msg);
    return true;
}

// VERIFY: 8a244fb3 manual
// OFFSET: 0x005F1E30
bool CGCalendarEvent::RemoveEvent() {
    if (!this->IsModerator()) {
        CGCalendar::CalendarError("CALENDAR_ERROR_PERMISSIONS", 0);
        return false;
    }

    CDataStore msg;
    msg.Put(static_cast<uint32_t>(CMSG_CALENDAR_REMOVE_EVENT));
    msg.Put(this->m_eventID);
    msg.Put(this->m_moderatorInviteID);
    msg.Put(static_cast<uint8_t>((this->m_flags & 0x440) != 0));
    msg.Finalize();
    ClientServices::Send2(&msg);
    return true;
}

// VERIFY: e23e4de8 manual
// OFFSET: 0x005F1EF0
bool CGCalendarEvent::Update() {
    if (!this->m_changed) {
        return false;
    }

    this->m_changed = 0;

    if (this->m_date <= g_clientGameTime) {
        CGCalendar::CalendarError("CALENDAR_ERROR_EVENT_PASSED", 0);
        return false;
    }

    CDataStore msg;
    msg.Put(static_cast<uint32_t>(CMSG_CALENDAR_UPDATE_EVENT));
    msg.Put(this->m_eventID);
    msg.Put(this->m_moderatorInviteID);
    msg.PutString(this->m_title);
    msg.PutString(this->m_description);
    msg.Put(static_cast<uint8_t>(this->m_type));
    msg.Put(static_cast<uint8_t>(this->m_repeatOption));
    msg.Put(this->m_maxSize);
    msg.Put(static_cast<uint32_t>(this->m_textureID));
    uint32_t date;
    WowTime::WowEncodeTime(date, &this->m_date);
    msg.Put(date);
    WowTime::WowEncodeTime(date, &this->m_lockoutDate);
    msg.Put(date);
    msg.Put(this->m_flags);
    msg.Finalize();
    ClientServices::Send2(&msg);
    return true;
}

// VERIFY: d6e6845f manual
// OFFSET: 0x005F2070
bool CGCalendarEvent::Invite(const char* name) {
    if (this->m_flags & 0x40) {
        CGCalendar::CalendarError("CALENDAR_ERROR_INVITES_DISABLED", 0);
        return false;
    }

    if (this->m_numInvites >= 100) {
        CGCalendar::CalendarError("CALENDAR_ERROR_INVITES_EXCEEDED", 100);
        return false;
    }

    char fixedName[48];
    FixCharacterNameCase(fixedName, name, sizeof(fixedName));

    for (uint32_t i = 0; i < this->m_numInvites; i++) {
        WOWGUID key = WOWGUID();
        NameCache* record = g_nameDBCache.GetRecord(this->m_invites.m_data[i]->m_guid.m_guid, key, nullptr, nullptr, false);

        if (record && !SStrCmp(fixedName, record->m_name, STORM_MAX_STR)) {
            CGCalendar::CalendarError("CALENDAR_ERROR_ALREADY_INVITED_TO_EVENT_S", reinterpret_cast<uintptr_t>(fixedName));
            return false;
        }
    }

    if (!this->IsModerator()) {
        CGCalendar::CalendarError("CALENDAR_ERROR_PERMISSIONS", 0);
        return false;
    }

    CDataStore msg;
    msg.Put(static_cast<uint32_t>(CMSG_CALENDAR_EVENT_INVITE));
    msg.Put(this->m_eventID);
    msg.Put(this->m_moderatorInviteID);
    msg.PutString(fixedName);
    msg.Put(this->m_new);
    msg.Put(static_cast<uint8_t>((this->m_flags & 0x400) != 0));
    msg.Finalize();
    ClientServices::Send2(&msg);
    return true;
}

// VERIFY: dd21d29e manual
// OFFSET: 0x005F2220
bool CGCalendarEvent::RemoveInvite(uint32_t index) {
    if (index >= this->m_numInvites) {
        return false;
    }

    if (this->m_invites.m_data[index]->m_moderatorStatus == 2) {
        CGCalendar::CalendarError("CALENDAR_DELETE_CREATOR_FAILED", 0);
        return false;
    }

    SmartGUID guid;
    guid.m_guid = this->m_invites.m_data[index]->m_guid.m_guid;
    CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

    if (player && this->m_invites.m_data[index]->m_guid != player->m_obj->m_guid && !this->IsModerator()) {
        CGCalendar::CalendarError("CALENDAR_ERROR_PERMISSIONS", 0);
        return false;
    }

    if (this->m_new) {
        EventInviteInfo* invite = this->m_invites.m_data[index];
        this->m_numInvites--;

        for (uint32_t i = index; i < this->m_numInvites; i++) {
            this->m_invites.m_data[i] = this->m_invites.m_data[i + 1];
        }

        this->m_invites.m_data[this->m_numInvites] = invite;
        return true;
    }

    CDataStore msg;
    msg.Put(static_cast<uint32_t>(CMSG_CALENDAR_EVENT_REMOVE_INVITE));
    msg << guid;
    msg.Put(this->m_invites.m_data[index]->m_inviteID);
    msg.Put(this->m_moderatorInviteID);
    msg.Put(this->m_eventID);
    msg.Finalize();
    ClientServices::Send2(&msg);
    return true;
}

// VERIFY: 0ee03c7a manual
// OFFSET: 0x005F23B0
bool CGCalendarEvent::RemoveInviteByGUID(WOWGUID guid) {
    if (!guid) {
        return false;
    }

    int32_t index = this->GetInviteIndex(guid);

    if (index < 0) {
        return false;
    }

    return this->RemoveInvite(index);
}

// VERIFY: 0c93d50a manual
// OFFSET: 0x005F23F0
bool CGCalendarEvent::InviteRSVP(uint32_t status) {
    if (this->m_new || (status != 1 && status != 2 && status != 8)) {
        return false;
    }

    CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

    if (!player) {
        return false;
    }

    uint32_t index = 0;

    if (!this->m_numInvites) {
        return false;
    }

    WOWGUID guid = player->m_obj->m_guid;

    while (this->m_invites.m_data[index]->m_guid != guid) {
        if (++index >= this->m_numInvites) {
            return false;
        }
    }

    if ((this->m_flags & 0x400) && this->m_invites.m_data[index]->m_inviteType == 1) {
        return false;
    }

    CDataStore msg;
    msg.Put(static_cast<uint32_t>(CMSG_CALENDAR_EVENT_RSVP));
    msg.Put(this->m_eventID);
    msg.Put(this->m_invites.m_data[index]->m_inviteID);
    msg.Put(status);
    msg.Finalize();
    ClientServices::Send2(&msg);
    return true;
}

// VERIFY: dbc2b3c0 manual
// OFFSET: 0x005F2530
bool CGCalendarEvent::SignUp(uint8_t tentative) {
    if (this->m_new) {
        return false;
    }

    CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

    if (player) {
        WOWGUID guid = player->m_obj->m_guid;

        for (uint32_t i = 0; i < this->m_numInvites; i++) {
            if (this->m_invites.m_data[i]->m_guid == guid) {
                return false;
            }
        }
    }

    CDataStore msg;
    msg.Put(static_cast<uint32_t>(CMSG_CALENDAR_EVENT_SIGN_UP));
    msg.Put(this->m_eventID);
    msg.Put(tentative);
    msg.Finalize();
    ClientServices::Send2(&msg);
    return true;
}

// VERIFY: 62b551de manual
// OFFSET: 0x005F2640
void CGCalendarEvent::UpdateInviteModeratorStatus(WOWGUID guid, uint8_t status) {
    for (uint32_t i = 0; i < this->m_numInvites; i++) {
        if (this->m_invites.m_data[i]->m_guid != guid) {
            continue;
        }

        this->m_invites.m_data[i]->m_moderatorStatus = status;
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

        if (player && player->m_obj->m_guid == guid) {
            if (status == 1) {
                this->m_moderatorInviteID = this->m_invites.m_data[i]->m_inviteID;
            } else {
                this->m_moderatorInviteID = 0;
            }
        }

        FrameScript_SignalEvent(SCRIPT_EVENT_CALENDAR_UPDATE_INVITE_LIST, nullptr);
        return;
    }
}

// VERIFY: 1bbde5a8 manual
// OFFSET: 0x005F26F0
bool CGCalendarEvent::CanSetInviteStatus(uint32_t index, uint32_t status) {
    if (!this->IsModerator() || index >= this->m_numInvites || status >= 9 || !status || status == 6 || status == 7) {
        return false;
    }

    if ((this->m_flags & 0x400) && (status == 1 || status == 2)) {
        return false;
    }

    return status != this->m_invites.m_data[index]->m_status;
}

// VERIFY: b4129b9f manual
// OFFSET: 0x005F2750
void CGCalendarEvent::SetInviteStatus(uint32_t index, uint32_t status) {
    if (!this->CanSetInviteStatus(index, status)) {
        return;
    }

    if (this->m_new) {
        this->m_invites.m_data[index]->m_status = status;
        FrameScript_SignalEvent(SCRIPT_EVENT_CALENDAR_UPDATE_INVITE_LIST, nullptr);
        return;
    }

    CDataStore msg;
    msg.Put(static_cast<uint32_t>(CMSG_CALENDAR_EVENT_STATUS));
    SmartGUID guid;
    guid.m_guid = this->m_invites.m_data[index]->m_guid.m_guid;
    msg << guid;
    msg.Put(this->m_eventID);
    msg.Put(this->m_invites.m_data[index]->m_inviteID);
    msg.Put(this->m_moderatorInviteID);
    msg.Put(status);
    msg.Finalize();
    ClientServices::Send2(&msg);
}

// VERIFY: a301db27 auto
// OFFSET: 0x005F2860
void CGCalendarEvent::SetModeratorStatus(uint32_t index, uint32_t status) {
    if (!this->IsModerator()) {
        CGCalendar::CalendarError("CALENDAR_ERROR_PERMISSIONS", 0);
        return;
    }

    if (index >= this->m_numInvites || status >= 2 || !this->CanModerate(index)) {
        return;
    }

    if (status == this->m_invites.m_data[index]->m_moderatorStatus) {
        return;
    }

    if (this->m_new) {
        this->m_invites.m_data[index]->m_moderatorStatus = status;
        FrameScript_SignalEvent(SCRIPT_EVENT_CALENDAR_UPDATE_INVITE_LIST, nullptr);
        return;
    }

    CDataStore msg;
    msg.Put(static_cast<uint32_t>(CMSG_CALENDAR_EVENT_MODERATOR_STATUS));
    SmartGUID guid;
    guid.m_guid = this->m_invites.m_data[index]->m_guid.m_guid;
    msg << guid;
    msg.Put(this->m_eventID);
    msg.Put(this->m_invites.m_data[index]->m_inviteID);
    msg.Put(this->m_moderatorInviteID);
    msg.Put(status);
    msg.Finalize();
    ClientServices::Send2(&msg);
}

// VERIFY: 8f99c58f auto
// OFFSET: 0x005F29E0
CGCalendarEvent::CGCalendarEvent() {
    this->Clear();
}

// VERIFY: 4a90c632 manual
// OFFSET: 0x005F2A20
CGCalendarEvent::CGCalendarEvent(WOWGUID guid, uint8_t level, int32_t calendarType) {
    this->Clear();
    CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

    if (!player) {
        return;
    }

    this->m_creatorGUID = player->m_obj->m_guid;
    uint8_t inviteType = 0;

    if (calendarType == 0) {
        this->m_flags |= 0x1;
    } else if (calendarType == 1) {
        this->m_flags |= 0x40;
    } else if (calendarType == 2) {
        this->m_flags |= 0x400;
        inviteType = 1;
    }

    if (guid) {
        this->m_numInvites = 1;

        if (!this->m_invites.Count()) {
            void* mem = SMemAlloc(sizeof(EventInviteInfo), __FILE__, __LINE__, 0);
            EventInviteInfo* invite = mem ? new (mem) EventInviteInfo() : nullptr;
            this->m_invites.New(invite);
        }

        EventInviteInfo* invite = this->m_invites.m_data[0];
        invite->m_inviteID = 0;
        invite->m_guid = guid;
        invite->m_level = level;
        invite->m_status = 3;
        invite->m_moderatorStatus = 2;
        invite->m_inviteType = inviteType;
        invite->m_responseTime = 0;
    }

    this->m_moderatorInviteID = 1;
    this->m_new = 1;
}

// VERIFY: c5c54e9d manual
// OFFSET: 0x005F2B70
CGCalendarEvent::~CGCalendarEvent() {
    uint32_t count = this->m_invites.Count();

    for (uint32_t i = 0; i < count; i++) {
        g_nameDBCache.CancelCallback(this->m_invites.m_data[i]->m_guid.m_guid, InviteNameCacheCallback, this);
        delete this->m_invites.m_data[i];
    }

    this->m_invites.Clear();
}

// VERIFY: 28fcc58f manual
// OFFSET: 0x005F2C00
bool CGCalendarEvent::ProcessGetEvent(CDataStore* msg, int32_t* status) {
    CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
    WOWGUID playerGuid = player ? player->m_obj->m_guid : WOWGUID();

    SmartGUID creator;
    *msg >> creator;
    this->m_creatorGUID = creator;
    msg->Get(this->m_eventID);
    msg->GetString(this->m_title, sizeof(this->m_title));
    msg->GetString(this->m_description, sizeof(this->m_description));
    uint8_t type;
    msg->Get(type);
    this->m_type = type;
    uint8_t repeatOption;
    msg->Get(repeatOption);
    this->m_repeatOption = repeatOption;
    msg->Get(this->m_maxSize);
    msg->Get(reinterpret_cast<uint32_t&>(this->m_textureID));
    msg->Get(this->m_flags);
    uint32_t date;
    msg->Get(date);
    uint32_t lockoutDate;
    msg->Get(lockoutDate);
    WowTime::WowDecodeTime(date, &this->m_date);
    WowTime::WowDecodeTime(lockoutDate, &this->m_lockoutDate);
    msg->Get(this->m_guildID);
    msg->Get(this->m_numInvites);

    bool found = false;

    for (uint32_t i = 0; i < this->m_numInvites; i++) {
        SmartGUID guid;
        *msg >> guid;
        uint8_t level;
        msg->Get(level);
        uint8_t inviteStatus;
        msg->Get(inviteStatus);
        uint8_t moderatorStatus;
        msg->Get(moderatorStatus);
        uint8_t inviteType;
        msg->Get(inviteType);
        uint64_t inviteID;
        msg->Get(inviteID);
        uint32_t responseTime;
        msg->Get(responseTime);
        char notes[sizeof(EventInviteInfo::m_notes)];
        msg->GetString(notes, sizeof(notes));

        if (msg->m_read > msg->m_size) {
            this->m_numInvites = i;
            break;
        }

        WOWGUID key = guid;

        if (!g_nameDBCache.GetRecord(guid.m_guid, key, InviteNameCacheCallback, this, false)) {
            this->m_pendingNames++;
        }

        if (i >= this->m_invites.Count()) {
            void* mem = SMemAlloc(sizeof(EventInviteInfo), __FILE__, __LINE__, 0);
            EventInviteInfo* invite = mem ? new (mem) EventInviteInfo() : nullptr;
            this->m_invites.New(invite);
        }

        if (this->m_new) {
            inviteStatus = 0;
        }

        if (guid == playerGuid) {
            found = true;
            *status = inviteStatus;

            if (moderatorStatus) {
                this->m_moderatorInviteID = inviteID;
            }
        }

        EventInviteInfo* invite = this->m_invites.m_data[i];
        invite->m_inviteID = inviteID;
        invite->m_guid = guid;
        invite->m_level = level;
        invite->m_status = inviteStatus;
        invite->m_moderatorStatus = moderatorStatus;
        invite->m_responseTime = responseTime;
        invite->m_inviteType = inviteType;
        SStrCopy(invite->m_notes, notes, sizeof(invite->m_notes));
    }

    if (!found && (this->m_flags & 0x400)) {
        *status = 7;
    }

    this->m_new = 0;
    this->m_changed = 0;
    this->m_selectedInviteID = 0;

    if (!this->m_pendingNames) {
        this->SortInvites(3, false);
    }

    return true;
}

// VERIFY: 5e4995f1 manual
// OFFSET: 0x005F2F90
bool CGCalendarEvent::ProcessInitialInviteList(CDataStore* msg) {
    CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
    WOWGUID playerGuid = player ? player->m_obj->m_guid : WOWGUID();

    msg->Get(this->m_numInvites);

    bool found = false;

    for (uint32_t i = 0; i < this->m_numInvites; i++) {
        SmartGUID guid;
        *msg >> guid;
        uint8_t level;
        msg->Get(level);

        if (msg->m_read > msg->m_size) {
            this->m_numInvites = i;
            break;
        }

        WOWGUID key = guid;

        if (!g_nameDBCache.GetRecord(guid.m_guid, key, InviteNameCacheCallback, this, false)) {
            this->m_pendingNames++;
        }

        if (i >= this->m_invites.Count()) {
            void* mem = SMemAlloc(sizeof(EventInviteInfo), __FILE__, __LINE__, 0);
            EventInviteInfo* invite = mem ? new (mem) EventInviteInfo() : nullptr;
            this->m_invites.New(invite);
        }

        uint8_t moderatorStatus;
        uint8_t inviteStatus;

        if (guid == playerGuid) {
            moderatorStatus = 2;
            inviteStatus = 3;
            found = true;
        } else {
            moderatorStatus = 0;
            inviteStatus = 0;
        }

        EventInviteInfo* invite = this->m_invites.m_data[i];
        invite->m_inviteID = 0;
        invite->m_guid = guid;
        invite->m_level = level;
        invite->m_status = inviteStatus;
        invite->m_moderatorStatus = moderatorStatus;
        invite->m_inviteType = 0;
        invite->m_responseTime = 0;
    }

    if (!found && player) {
        uint32_t index = this->m_numInvites;
        this->m_numInvites = index + 1;

        if (this->m_numInvites > this->m_invites.Count()) {
            void* mem = SMemAlloc(sizeof(EventInviteInfo), __FILE__, __LINE__, 0);
            EventInviteInfo* invite = mem ? new (mem) EventInviteInfo() : nullptr;
            this->m_invites.New(invite);
        }

        EventInviteInfo* invite = this->m_invites.m_data[index];
        invite->m_inviteID = 0;
        invite->m_guid = playerGuid;
        invite->m_level = player->m_unit->m_unitFieldLevel;
        invite->m_status = 3;
        invite->m_moderatorStatus = 2;
        invite->m_inviteType = 0;
        invite->m_responseTime = 0;
    }

    this->m_moderatorInviteID = 1;
    this->m_new = 1;
    this->m_selectedInviteID = 0;

    if (this->m_pendingNames) {
        this->m_signalType = 1;
        return true;
    }

    this->SortInvites(3, false);
    FrameScript_SignalEvent(SCRIPT_EVENT_CALENDAR_UPDATE_INVITE_LIST, "%b", 1);
    return true;
}

// VERIFY: c8c66049 manual
// OFFSET: 0x005F32B0
void CGCalendarEvent::AddInvite(uint64_t inviteID, WOWGUID guid, uint8_t level, uint8_t status, uint8_t inviteType, uint32_t responseTime) {
    CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
    WOWGUID playerGuid = player ? player->m_obj->m_guid : WOWGUID();

    uint32_t index = this->m_numInvites;
    uint8_t moderatorStatus = 0;
    this->m_numInvites = index + 1;

    if (this->m_numInvites > this->m_invites.Count()) {
        void* mem = SMemAlloc(sizeof(EventInviteInfo), __FILE__, __LINE__, 0);
        EventInviteInfo* invite = mem ? new (mem) EventInviteInfo() : nullptr;
        this->m_invites.New(invite);
    }

    if (!g_nameDBCache.GetRecord(guid.m_guid, guid, InviteNameCacheCallback, this, false)) {
        this->m_pendingNames++;
    }

    if (guid == playerGuid && inviteType != 1) {
        moderatorStatus = 2;
    }

    EventInviteInfo* invite = this->m_invites.m_data[index];
    invite->m_inviteID = inviteID;
    invite->m_guid = guid;
    invite->m_level = level;
    invite->m_status = status;
    invite->m_moderatorStatus = moderatorStatus;
    invite->m_inviteType = inviteType;
    invite->m_responseTime = responseTime;

    if (this->m_pendingNames) {
        this->m_signalType = 0;
        return;
    }

    this->SortInvites(this->m_sortType, false);
    FrameScript_SignalEvent(SCRIPT_EVENT_CALENDAR_UPDATE_INVITE_LIST, nullptr);
}

// VERIFY: 2583e5f4 manual
// OFFSET: 0x005B8D60
EventInviteInfo* CGCalendarEvent::GetInvite(WOWGUID guid) {
    for (uint32_t i = 0; i < this->m_numInvites; i++) {
        if (this->m_invites.m_data[i]->m_guid == guid) {
            return this->m_invites.m_data[i];
        }
    }

    return nullptr;
}

// VERIFY: d87c9eac manual
// OFFSET: 0x005B8DC0
int32_t CGCalendarEvent::GetInviteIndex(WOWGUID guid) {
    for (uint32_t i = 0; i < this->m_numInvites; i++) {
        if (this->m_invites.m_data[i]->m_guid == guid) {
            return i;
        }
    }

    return -1;
}

// VERIFY: db0dd3af manual
// OFFSET: 0x005B8E00
int32_t CGCalendarEvent::GetSelectedInviteIndex() {
    for (uint32_t i = 0; i < this->m_numInvites; i++) {
        if (this->m_invites.m_data[i]->m_inviteID == this->m_selectedInviteID) {
            return i;
        }
    }

    return -1;
}
