#ifndef ENGINE_SOURCE_OS_LINUX_OSTCPCONN_H
#define ENGINE_SOURCE_OS_LINUX_OSTCPCONN_H

#include <cstddef>
#include <cstdint>
#include <sys/select.h>
#include "Engine/Source/Os/OsTcp.h"
#include "Engine/Source/Storm/H/SCritSect.h"
#include "Engine/Source/Storm/H/stpl.h"
#include "Engine/Source/Storm/H/Types.h"

class SEvent;

// BSD sockets in place of Winsock. There is no I/O completion port, so connections are always the select-driven
// SLTCPCONN kind; the IOCP-only pieces (IOTCPCONN, AcceptEx, IoPump, the WSARecv/WSASend pointers) have no Linux
// counterpart and are left out.
typedef int SOCKET;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)

namespace OsNet {
extern const char* OSNETERR_INTERNAL;
extern const char* OSNETERR_WSASTARTUPFAILED;
extern const char* OSNETERR_WSASTARTUPVERSION;
extern const char* OSNETERR_SENDFAILED;
extern const char* OSNETERR_CREATETHREADFAILED;
extern const char* OSNETERR_CREATEEVENTFAILED;
extern const char* OSNETERR_BINDFAILED;
extern const char* OSNETERR_LISTENFAILED;
extern const char* OSNETERR_SOCKETFAILED;
extern const char* OSNETERR_SELECTFAILED;
extern const char* OSNETERR_ACCEPTFAILED;
extern const char* OSNETERR_PORTFAILED;
extern const char* OSNETERR_OVERLAPTYPE;
extern const char* OSNETERR_LISTENCLOSED;
extern const char* OSNETERR_ACCEPTEXFAILED;

enum SELECTSET {
    SELECTSET_FIRST = 0,
    SELECTSET_R = 0,
    SELECTSET_W = 1,
    SELECTSET_E = 2,
    SELECT_MAX = 3,
};

enum OVERLAPTYPE {
    OVERLAPTYPE_READ = 0,
    OVERLAPTYPE_WRITE = 1,
    OVERLAPTYPE_ACCEPT = 2,
};

enum OUTPUTSTATE {
    OUTPUTSTATE_WAITING = 0,
    OUTPUTSTATE_WRITING = 1,
    OUTPUTSTATE_COMPLETED = 2,
};

enum CONNLIST {
    CONNLIST_LOOP_CONNECTED = 0,
    CONNLIST_TCP_CONNECTED = 1,
    CONNLIST_UDP_CONNECTED = 2,
    CONNLIST_FILE_CONNECTED = 3,
    CONNLISTS = 4,
    CONNLIST_NONE = 4,
};

struct NETSELECTSETS;
struct TCPACCEPT;
struct TCPNET;

struct LOCKEDLONG {
    volatile LONG m_value;
};

struct NETOVERLAP {
    OVERLAPPED m_overlapped;
    OVERLAPTYPE m_type;
};

struct OUTPUT : public TSLinkedNode<OUTPUT> {
    NETOVERLAP m_overlap;
    OUTPUTSTATE m_state;
    uint32_t m_time;
    uint32_t m_bytes;
    uint32_t m_dataBytes;
    uint8_t* m_data;
    SEvent* m_completionEvent;

    OUTPUT();
    ~OUTPUT();
};

struct NETSELSOCK {
    SOCKET m_sock;

    virtual void Selected(TCPNET* net, SELECTSET selectSet) = 0;
    virtual int32_t IsClosed();
    virtual void AddToSelectSets(NETSELECTSETS* selectSets) = 0;
};

struct NETSELSOCKPTR : public TSHashObject<NETSELSOCKPTR, HASHKEY_NONE> {
    NETSELSOCK* ptr;
};

struct NETSELECTSETS {
    TCPNET* m_net;
    fd_set m_sets[SELECT_MAX];
    TSHashTableReuse<NETSELSOCKPTR, HASHKEY_NONE> m_selsockTable;

    void AddToSet(NETSELSOCK* selsock, SELECTSET selectSet);
};

struct alignas(8) NETCONN : public NETSELSOCK {
    TSLink<NETCONN> m_link;
    uint8_t m_list;
    uint8_t m_listSlot;
    uint16_t m_reserved;
    LOCKEDLONG m_refCount;
    NETCONNADDR m_connAddr;
    int32_t m_eventProcUserLock;
    NETEVENTPROC m_eventProc;
    void* m_user;
    CCritSect m_lock;
    uint32_t m_time;
    TCPNET* m_net;

    NETCONN(TCPNET* net, SOCKET sock, NETEVENTPROC eventProc, void* user, const NETCONNADDR* connAddr);
    virtual void CloseAndUnlock() = 0;
    virtual ~NETCONN();
    virtual void IncIo();
    virtual void DecIo();
    virtual void CompleteWrite(NETOVERLAP* overlap, uint32_t bytes);
    virtual void CompleteRead(NETOVERLAP* overlap, uint32_t bytes);
    virtual void Close();
    void DecRef();
    void Disconnect(int32_t notify);
    void GetEventProcAndUser(NETEVENTPROC& eventProc, void*& user);
    int32_t NoteConnect();
    int32_t NoteData(void* data, uint32_t bytes, uint32_t* bytesProcessed, const NETCONNADDR* connAddr);
    int32_t NoteDisconnect();
};

struct NETCONNFULL : public NETCONN {
    NETCONNFULL(TCPNET* net, SOCKET sock, NETEVENTPROC eventProc, void* user, const NETCONNADDR* connAddr);
    virtual void Send(const void* data, uint32_t bytes) = 0;
    virtual OS_SEND SendSync(const void* data, uint32_t bytes, uint32_t* bytesSent, uint32_t timeout) = 0;
    virtual void SetNagle(int32_t enable) = 0;
    virtual int32_t SetWindow(uint32_t size) = 0;
    virtual void SetRecvTimeout(uint32_t timeout) = 0;
};

struct TCPCONN : public NETCONNFULL {
    TSList<OUTPUT, TSGetLink<OUTPUT> > m_outputList;
    uint32_t m_bytes;
    uint8_t m_data[1460];

    TCPCONN(TCPNET* net, SOCKET sock, NETEVENTPROC eventProc, void* user, const NETCONNADDR* connAddr);
    virtual void CloseAndUnlock();
    virtual ~TCPCONN();
    virtual void CompleteWrite(NETOVERLAP* overlap, uint32_t bytes);
    virtual void CompleteRead(NETOVERLAP* overlap, uint32_t bytes);
    virtual void Send(const void* data, uint32_t bytes);
    virtual OS_SEND SendSync(const void* data, uint32_t bytes, uint32_t* bytesSent, uint32_t timeout);
    virtual void SetNagle(int32_t enable);
    virtual int32_t SetWindow(uint32_t size);
    virtual void SetRecvTimeout(uint32_t timeout);
    virtual void StartWriteAndLeaveLock(OUTPUT* output) = 0;
    virtual void StartRead() = 0;
    OUTPUT* LockedEnqueue(const void* data, uint32_t bytes);
};

struct SLTCPCONN : public TCPCONN {
    SLTCPCONN(TCPNET* net, SOCKET sock, NETEVENTPROC eventProc, void* user, const NETCONNADDR* connAddr, const void* data, uint32_t bytes);
    virtual void Selected(TCPNET* net, SELECTSET selectSet);
    virtual void AddToSelectSets(NETSELECTSETS* selectSets);
    virtual void StartWriteAndLeaveLock(OUTPUT* output);
    virtual void StartRead();
    void ContinueRead();
    void ContinueWrite();
};

struct TCPLISTEN : public NETSELSOCK {
    TSLink<TCPLISTEN> m_link;
    uint32_t m_portAddr;
    NETEVENTPROC m_eventProc;
    void* m_user;
    int32_t m_enabled;
    TSList<TCPACCEPT, TSGetLink<TCPACCEPT> > m_acceptList;
};

struct TCPACCEPT : public TSLinkedNode<TCPACCEPT> {
    NETOVERLAP m_overlap;
    CCritSect m_lock;
    uint8_t m_addr[64];
    TCPLISTEN* m_listen;
    SOCKET m_sock;

    SOCKET Complete(NETCONNADDR* connAddr, int32_t makeSock);
    void Init();
};

struct NETCONNECT : public NETSELSOCK {
    TSLink<NETCONNECT> m_link;
    void* m_user;
    void* m_data;
    uint32_t m_bytes;
};

template <class T, int LINKOFFSET, int SLOTS>
struct TSSlottedListEx {
    TSExplicitList<T, LINKOFFSET> m_lists[SLOTS];
    CCritSect m_locks[SLOTS];
    LOCKEDLONG m_linkSlot;
    LOCKEDLONG m_count;

    virtual ~TSSlottedListEx();
    uint8_t LinkNode(T* ptr);
    void UnlinkNode(T* ptr, uint8_t slot);
};

struct TCPNET {
    static TCPNET* s_pnet;
    static HSLOG s_log;
    static int32_t s_qpcexists;
    static float s_qpctoms;
    static int32_t s_pumpShutdown;

    static void LogWrite(const char* format, ...);

    LOCKEDLONG m_refCount;
    uint32_t m_pumpThreadCount;
    TSGrowableArray<void*> m_pumpThreads;
    void* m_udpPumpThread;
    void* m_udpPumpEvent;
    CCritSect m_loopLock;
    uint8_t m_loopLists[0x24];
    TSSlottedListEx<NETCONN, 8, 8> m_connList[CONNLISTS];
    void* m_listenThread;
    TSSlottedListEx<TCPLISTEN, 8, 1> m_listenList;
    void* m_baseThread;
    void* m_baseEvent;
    int32_t m_baseTcpShutdown;
    void* m_baseTcpShutdownEvent;
    TSSlottedListEx<NETCONNECT, 8, 1> m_connectList[CONNLISTS];
    HANDLE m_port;
    LOCKEDLONG m_hostAddrInfoCount;
    void* m_hostAddrInfoLock;
    uint32_t m_hostAddrInfoId;
    uint8_t m_hostAddrInfoList[0xC];

    void CompleteAcceptEx(TCPACCEPT* accept, int32_t makeConn);
    void LinkConn(NETCONN* conn, CONNLIST toList);
    void Pump(uint32_t timeout);
    void TcpMakeConn(SOCKET sock, NETEVENTPROC eventProc, void* user, const NETCONNADDR* connAddr, const void* data, uint32_t bytes);
};
}

#endif
