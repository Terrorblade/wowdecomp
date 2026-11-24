#include <cstdint>
#include <cstring>
#include "Engine/Source/Base/TExtraInstanceRecycler.h"
#include "Engine/Source/Event/EvtContext.h"
#include "Engine/Source/Event/EvtQueue.h"
#include "Engine/Source/Event/EvtSched.h"
#include "Engine/Source/Storm/H/storm.h"

// STATIC: 0x00B41850
static TExtraInstanceRecycler<EvtMessage> s_messageRecycler(64, 64, 256);

void IEvtQueueInitialize() {
}

// VERIFY: 9c1e12ce auto
// OFFSET: 0x004804B0
void IEvtQueueDestroy() {
    s_messageRecycler.Clear();
}

// VERIFY: b7f44034 auto
// OFFSET: 0x004804C0
int32_t IEvtQueueCheckSyncMouseState(EvtContext* context, MOUSEBUTTON button) {
    if (!context) {
        SErrSetLastError(STORM_ERROR_INVALID_PARAMETER);
        return 0;
    }

    return (context->m_queueSyncButtonState & button) != 0;
}

// VERIFY: 18d2b766 manual
// OFFSET: 0x004804F0
void IEvtQueueRegister(EvtContext* context, EVENTID id, EVENTHANDLERFUNC handler, void* param, float priority) {
    if (!context) {
        DebugValidate("context");
        SErrSetLastError(STORM_ERROR_INVALID_PARAMETER);
        return;
    }

    TSExplicitList<EvtHandler, offsetof(EvtHandler, m_link)>& list = context->m_queueHandlerList[id];

    EvtHandler* evtHandler = list.NewNode(TSLIST_NEWNODE_UNLINKED, 0, 0);
    evtHandler->m_priority = priority;
    evtHandler->m_param = param;
    evtHandler->m_func = handler;
    evtHandler->m_marker = 0;

    EvtHandler* existing = list.Head();

    while (existing && (priority < existing->m_priority || existing->m_marker)) {
        existing = list.Next(existing);
    }

    list.LinkNode(evtHandler, TSLIST_LINK_BEFORE, existing);
}

// VERIFY: ee77d145 auto
// OFFSET: 0x004806A0
void IEvtQueuePost(EvtContext* context, EVENTID id, const void* data, uint32_t bytes) {
    if (!context) {
        DebugValidate("context");
        SErrSetLastError(STORM_ERROR_INVALID_PARAMETER);
        return;
    }

    EvtMessage* message = s_messageRecycler.New((bytes > 4 ? bytes : 4) - 4);
    message->m_id = id;

    if (data) {
        memcpy(message->m_data, data, bytes);
    }

    context->m_critsect.Enter();
    context->m_queueMessageList.LinkToTail(message);
    context->m_critsect.Leave();
}

// VERIFY: aa0a8976 manual
// OFFSET: 0x004808A0
static void UpdateSyncKeyState(EvtContext* context, KEY key, EVENTID& id) {
    int32_t found = 0;

    context->m_critsect.Enter();

    TSExplicitList<EvtKeyDown, offsetof(EvtKeyDown, m_link)>& list = context->m_queueSyncKeyDownList;
    EvtKeyDown* keyDown = list.Head();

    while (keyDown) {
        if (keyDown->m_key == key) {
            found = 1;
            keyDown = list.DeleteNode(keyDown);
        } else {
            keyDown = list.Next(keyDown);
        }
    }

    if (id == EVENT_ID_KEYDOWN) {
        if (found) {
            id = EVENT_ID_KEYDOWN_REPEATING;
        }

        EvtKeyDown* newKeyDown = list.NewNode(TSLIST_LINK_BEFORE, 0, 0);
        newKeyDown->m_key = key;
    }

    context->m_critsect.Leave();
}

// VERIFY: 03c42d9d auto
// OFFSET: 0x004809A0
static void UpdateSyncState(EvtContext* context, EVENTID& id, const void* data) {
    if (!context) {
        DebugValidate("context");
        SErrSetLastError(STORM_ERROR_INVALID_PARAMETER);
        return;
    }

    switch (id) {
        case EVENT_ID_FOCUS:
            context->m_queueSyncButtonState = 0;
            context->m_critsect.Enter();
            context->m_queueSyncKeyDownList.Clear();
            context->m_critsect.Leave();
            break;

        case EVENT_ID_KEYDOWN:
        case EVENT_ID_KEYUP:
            UpdateSyncKeyState(context, static_cast<const EVENT_DATA_KEY*>(data)->m_key, id);
            break;

        case EVENT_ID_MOUSEDOWN:
        case EVENT_ID_MOUSEUP: {
            uint32_t button = static_cast<const EVENT_DATA_MOUSE*>(data)->m_button;

            if (id == EVENT_ID_MOUSEDOWN) {
                context->m_queueSyncButtonState |= button;
            } else {
                context->m_queueSyncButtonState &= ~button;
            }

            break;
        }

        default:
            break;
    }
}

// VERIFY: aeeac6cd manual
// OFFSET: 0x00480A50
int32_t IEvtQueueCheckSyncKeyState(EvtContext* context, KEY key) {
    if (!context) {
        DebugValidate("context");
        SErrSetLastError(STORM_ERROR_INVALID_PARAMETER);
        return 0;
    }

    int32_t found = 0;

    context->m_critsect.Enter();

    for (EvtKeyDown* keyDown = context->m_queueSyncKeyDownList.Head(); keyDown; keyDown = context->m_queueSyncKeyDownList.Next(keyDown)) {
        if (keyDown->m_key == key) {
            found = 1;
            break;
        }
    }

    context->m_critsect.Leave();

    return found;
}

// VERIFY: 500523d9 manual
// OFFSET: 0x00480AD0
void IEvtQueueDispatch(EvtContext* context, EVENTID id, const void* data) {
    if (!context) {
        DebugValidate("context");
        SErrSetLastError(STORM_ERROR_INVALID_PARAMETER);
        return;
    }

    UpdateSyncState(context, id, data);

    if (SErrIsDisplayingError()) {
        return;
    }

    IEvtSchedulerIsContextInteractive(context);

    TSExplicitList<EvtHandler, offsetof(EvtHandler, m_link)>& list = context->m_queueHandlerList[id];

    EvtHandler marker;
    marker.m_marker = 1;
    list.LinkToHead(&marker);

    EvtHandler* handler;

    while ((handler = list.Next(&marker)) != nullptr) {
        list.LinkNode(&marker, TSLIST_LINK_AFTER, handler);

        if (handler->m_marker) {
            continue;
        }

        if (!handler->m_func(data, handler->m_param)) {
            break;
        }
    }

    list.UnlinkNode(&marker);
}

// VERIFY: 4f5a3a2c auto
// OFFSET: 0x00480BD0
void IEvtQueueDispatchAll(EvtContext* context) {
    if (!context) {
        DebugValidate("context");
        SErrSetLastError(STORM_ERROR_INVALID_PARAMETER);
        return;
    }

    TSExplicitList<EvtMessage, offsetof(EvtMessage, m_link)> messages;

    context->m_critsect.Enter();
    messages.Combine(&context->m_queueMessageList, 2, nullptr);
    context->m_critsect.Leave();

    EvtMessage* message;

    while ((message = messages.Head()) != nullptr) {
        IEvtQueueDispatch(context, message->m_id, message->m_data);
        s_messageRecycler.Delete(message);
    }
}

// VERIFY: 8fe70b40 manual
// OFFSET: 0x00480D30
void IEvtQueueUnregister(EvtContext* context, EVENTID id, EVENTHANDLERFUNC handler, void* param, uint32_t flags) {
    if (!context) {
        DebugValidate("context");
        SErrSetLastError(STORM_ERROR_INVALID_PARAMETER);
        return;
    }

    for (int32_t i = 0; i < EVENTIDS; i++) {
        if ((flags & 1) && i != id) {
            continue;
        }

        TSExplicitList<EvtHandler, offsetof(EvtHandler, m_link)>& list = context->m_queueHandlerList[i];
        EvtHandler* evtHandler = list.Head();

        while (evtHandler) {
            if ((evtHandler->m_func != handler && (flags & 2)) || (evtHandler->m_param != param && (flags & 4)) || evtHandler->m_marker) {
                evtHandler = list.Next(evtHandler);
            } else {
                evtHandler = list.DeleteNode(evtHandler);
            }
        }
    }
}
