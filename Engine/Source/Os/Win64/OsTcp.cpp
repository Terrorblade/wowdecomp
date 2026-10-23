
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <winsock2.h>
#include <mswsock.h>
#include <windows.h>
#include "Engine/Source/Os/Win64/OsTcpConn.h"
#include "Engine/Source/Os/TimeManager.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Storm/Source/SLock.h"

// STATIC: 0x00B1D3F4
const char* OsNet::OSNETERR_INTERNAL = "Internal error";

// STATIC: 0x00B1D3F8
const char* OsNet::OSNETERR_WSASTARTUPFAILED = "WSAStartup failed";

// STATIC: 0x00B1D3FC
const char* OsNet::OSNETERR_WSASTARTUPVERSION = "WSAStartup wrong version";

// STATIC: 0x00B1D400
const char* OsNet::OSNETERR_SENDFAILED = "Send failed";

// STATIC: 0x00B1D404
const char* OsNet::OSNETERR_CREATETHREADFAILED = "CreateThread failed";

// STATIC: 0x00B1D408
const char* OsNet::OSNETERR_CREATEEVENTFAILED = "CreateEvent failed";

// STATIC: 0x00B1D40C
const char* OsNet::OSNETERR_BINDFAILED = "Bind failed";

// STATIC: 0x00B1D410
const char* OsNet::OSNETERR_LISTENFAILED = "Listen failed";

// STATIC: 0x00B1D414
const char* OsNet::OSNETERR_SOCKETFAILED = "Socket failed";

// STATIC: 0x00B1D418
const char* OsNet::OSNETERR_SELECTFAILED = "Select failed";

// STATIC: 0x00B1D41C
const char* OsNet::OSNETERR_ACCEPTFAILED = "Accept failed";

// STATIC: 0x00B1D420
const char* OsNet::OSNETERR_PORTFAILED = "CreateIoCompletionPort failed";

// STATIC: 0x00B1D424
const char* OsNet::OSNETERR_OVERLAPTYPE = "Unknown overlap type";

// STATIC: 0x00B1D428
const char* OsNet::OSNETERR_LISTENCLOSED = "Listen closed";

// STATIC: 0x00B1D42C
const char* OsNet::OSNETERR_ACCEPTEXFAILED = "AcceptEx failed";

// STATIC: 0x00D4166C
float OsNet::TCPNET::s_qpctoms;

// STATIC: 0x00D41670
int32_t OsNet::TCPNET::s_qpcexists;

// STATIC: 0x00D41674
HSLOG OsNet::TCPNET::s_log;

// STATIC: 0x00D41678
OsNet::WSARECVPROC OsNet::TCPNET::s_WSARecv;

// STATIC: 0x00D4167C
OsNet::WSASENDPROC OsNet::TCPNET::s_WSASend;

// STATIC: 0x00D41680
LPFN_GETACCEPTEXSOCKADDRS OsNet::TCPNET::s_GetAcceptExSockaddrs;

// STATIC: 0x00D41684
LPFN_ACCEPTEX OsNet::TCPNET::s_AcceptEx;

// STATIC: 0x00D41828
int32_t OsNet::TCPNET::s_pumpShutdown;

// STATIC: 0x00D41830
OsNet::TCPNET* OsNet::TCPNET::s_pnet;

// INLINED
OsNet::OUTPUT::OUTPUT() {
    memset(&this->m_overlap.m_overlapped, 0, sizeof(this->m_overlap.m_overlapped));
    this->m_overlap.m_type = OVERLAPTYPE_WRITE;
    this->m_completionEvent = nullptr;
}

// INLINED
OsNet::OUTPUT::~OUTPUT() {
    if (this->m_dataBytes && this->m_data) {
        SMemFree(this->m_data, __FILE__, __LINE__, 0);
    }

    if (this->m_completionEvent) {
        this->m_completionEvent->Set();
    }
}

// INLINED
OsNet::NETCONNFULL::NETCONNFULL(TCPNET* net, SOCKET sock, NETEVENTPROC eventProc, void* user, const NETCONNADDR* connAddr) : NETCONN(net, sock, eventProc, user, connAddr) {
}

// INLINED
void OsNet::NETCONN::GetEventProcAndUser(NETEVENTPROC& eventProc, void*& user) {
    int32_t lock = this->m_eventProcUserLock;
    int32_t prev;

    do {
        do {
            prev = lock;
        } while (lock & 1);

        eventProc = this->m_eventProc;
        user = this->m_user;
        lock = this->m_eventProcUserLock;
    } while (prev != lock);
}

// INLINED
int32_t OsNet::NETCONN::NoteConnect() {
    NETEVENTPROC eventProc;
    void* user;
    this->GetEventProcAndUser(eventProc, user);

    if (!eventProc) {
        return 0;
    }

    uint32_t bytesProcessed;
    return eventProc(reinterpret_cast<HNETCONN>(this), &this->m_connAddr, NETNOTE_CONNECT, user, nullptr, 0, &bytesProcessed);
}

// INLINED
int32_t OsNet::NETCONN::NoteData(void* data, uint32_t bytes, uint32_t* bytesProcessed, const NETCONNADDR* connAddr) {
    NETEVENTPROC eventProc;
    void* user;
    this->GetEventProcAndUser(eventProc, user);

    if (!eventProc) {
        return 0;
    }

    return eventProc(reinterpret_cast<HNETCONN>(this), connAddr ? connAddr : &this->m_connAddr, NETNOTE_DATA, user, data, bytes, bytesProcessed);
}

// INLINED
int32_t OsNet::NETCONN::NoteDisconnect() {
    NETEVENTPROC eventProc;
    void* user;
    this->GetEventProcAndUser(eventProc, user);

    if (!eventProc) {
        return 0;
    }

    uint32_t bytesProcessed;
    return eventProc(reinterpret_cast<HNETCONN>(this), &this->m_connAddr, NETNOTE_DISCONNECT, user, nullptr, 0, &bytesProcessed);
}

// INLINED
void OsNet::NETCONN::DecRef() {
    if (!InterlockedDecrement(&this->m_refCount.m_value)) {
        delete this;
    }
}

void OsNet::NETCONN::IncIo() {
}

void OsNet::NETCONN::DecIo() {
}

void OsNet::NETCONN::CompleteWrite(NETOVERLAP* overlap, uint32_t bytes) {
}

void OsNet::NETCONN::CompleteRead(NETOVERLAP* overlap, uint32_t bytes) {
}

void OsNet::SLTCPCONN::StartRead() {
}

// OFFSET: 0x0086D700
int32_t OsNet::NETSELSOCK::IsClosed() {
    return this->m_sock == INVALID_SOCKET;
}

// OFFSET: 0x0086D710
void OsNet::NETSELECTSETS::AddToSet(NETSELSOCK* selsock, SELECTSET selectSet) {
    FD_SET(selsock->m_sock, &this->m_sets[selectSet]);
}

// OFFSET: 0x0086D770
void OsNet::TCPNET::LogWrite(const char* format, ...) {
    char line[1024];

    DWORD lastError = GetLastError();
    int wsaError = WSAGetLastError();
    uint32_t length = static_cast<uint32_t>(SStrPrintf(line, sizeof(line), "t[0x%08x] se[%u] we[%u] ", GetCurrentThreadId(), wsaError, lastError));

    va_list arglist;
    va_start(arglist, format);
    SStrVPrintf(&line[length], sizeof(line) - length, format, arglist);

    DebugLog("OsNet: (INFO): %s\n", line);

    if (OsNet::TCPNET::s_log) {
        SLogWrite(OsNet::TCPNET::s_log, line);
        SLogFlush(OsNet::TCPNET::s_log);
    }
}

// OFFSET: 0x0086D800
void OsNet::TCPCONN::CompleteRead(NETOVERLAP* overlap, uint32_t bytes) {
    if (!bytes) {
        return;
    }

    this->m_bytes += bytes;
    uint32_t bytesProcessed = 0;

    if (!this->NoteData(this->m_data, this->m_bytes, &bytesProcessed, nullptr)) {
        this->Close();
        return;
    }

    if (bytesProcessed >= this->m_bytes) {
        this->m_bytes = 0;
        this->StartRead();
        return;
    }

    if (bytesProcessed) {
        memmove(this->m_data, &this->m_data[bytesProcessed], this->m_bytes - bytesProcessed);
        this->m_bytes -= bytesProcessed;
        this->StartRead();
        return;
    }

    if (this->m_bytes >= sizeof(this->m_data)) {
        this->Close();
        return;
    }

    this->StartRead();
}

// OFFSET: 0x0086D8C0
void OsNet::TCPCONN::CloseAndUnlock() {
    if (this->m_sock != INVALID_SOCKET) {
        closesocket(this->m_sock);
        this->m_sock = INVALID_SOCKET;
    }

    this->m_lock.Leave();
}

// OFFSET: 0x0086D8F0
void OsNet::TCPCONN::SetNagle(int32_t enable) {
    BOOL noDelay = !enable;
    setsockopt(this->m_sock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&noDelay), sizeof(noDelay));
}

// OFFSET: 0x0086D920
int32_t OsNet::TCPCONN::SetWindow(uint32_t size) {
    if (setsockopt(this->m_sock, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&size), sizeof(size))) {
        return 0;
    }

    return setsockopt(this->m_sock, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&size), sizeof(size)) == 0;
}

// OFFSET: 0x0086D980
void OsNet::TCPCONN::SetRecvTimeout(uint32_t timeout) {
    setsockopt(this->m_sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
}

// OFFSET: 0x0086D9B0
void OsNet::IOTCPCONN::IncIo() {
    InterlockedIncrement(&this->m_ioCount.m_value);
}

// OFFSET: 0x0086D9C0
void OsNet::IOTCPCONN::StartWriteAndLeaveLock(OUTPUT* output) {
    output->m_state = OUTPUTSTATE_WRITING;
    this->IncIo();
    this->m_lock.Leave();

    if (OsNet::TCPNET::s_WSASend) {
        WSABUF buffer;
        buffer.len = output->m_bytes;
        buffer.buf = reinterpret_cast<char*>(output->m_data);
        DWORD bytesSent = 0;

        if (OsNet::TCPNET::s_WSASend(this->m_sock, &buffer, 1, &bytesSent, 0, &output->m_overlap.m_overlapped, nullptr) != SOCKET_ERROR || WSAGetLastError() == WSA_IO_PENDING) {
            return;
        }
    } else if (WriteFile(reinterpret_cast<HANDLE>(this->m_sock), output->m_data, output->m_bytes, nullptr, &output->m_overlap.m_overlapped) || GetLastError() == ERROR_IO_PENDING) {
        return;
    }

    this->Close();
    this->DecIo();
}

// OFFSET: 0x0086DA80
void OsNet::IOTCPCONN::StartRead() {
    this->IncIo();

    memset(&this->m_readOverlap.m_overlapped, 0, sizeof(this->m_readOverlap.m_overlapped));
    this->m_readOverlap.m_type = OVERLAPTYPE_READ;

    uint32_t bytes = this->m_bytes;

    if (OsNet::TCPNET::s_WSARecv) {
        WSABUF buffer;
        buffer.buf = reinterpret_cast<char*>(&this->m_data[bytes]);
        buffer.len = sizeof(this->m_data) - bytes;
        DWORD bytesReceived = 0;
        DWORD flags = 0;

        if (OsNet::TCPNET::s_WSARecv(this->m_sock, &buffer, 1, &bytesReceived, &flags, &this->m_readOverlap.m_overlapped, nullptr) != SOCKET_ERROR || WSAGetLastError() == WSA_IO_PENDING) {
            return;
        }
    } else if (ReadFile(reinterpret_cast<HANDLE>(this->m_sock), &this->m_data[bytes], sizeof(this->m_data) - bytes, nullptr, &this->m_readOverlap.m_overlapped) || GetLastError() == ERROR_IO_PENDING) {
        return;
    }

    this->Close();
    this->DecIo();
}

// OFFSET: 0x0086DB40
void OsNet::IOTCPCONN::AddToSelectSets(NETSELECTSETS* selectSets) {
    OsNet::TCPNET::LogWrite("%s 11", OsNet::OSNETERR_INTERNAL);
}

// OFFSET: 0x0086DB60
void OsNet::IOTCPCONN::Selected(TCPNET* net, SELECTSET selectSet) {
    OsNet::TCPNET::LogWrite("%s 12", OsNet::OSNETERR_INTERNAL);
}

// OFFSET: 0x0086DB80
void OsNet::SLTCPCONN::StartWriteAndLeaveLock(OUTPUT* output) {
    this->m_lock.Leave();
}

// OFFSET: 0x0086DB90
void OsNet::SLTCPCONN::ContinueRead() {
    uint32_t bytes = sizeof(this->m_data) - this->m_bytes;

    if (bytes >= sizeof(this->m_data)) {
        bytes = sizeof(this->m_data);
    }

    int32_t received = recv(this->m_sock, reinterpret_cast<char*>(&this->m_data[this->m_bytes]), bytes, 0);

    if (received == SOCKET_ERROR) {
        if (WSAGetLastError() != WSAEWOULDBLOCK) {
            this->Close();
        }
    } else if (received) {
        this->CompleteRead(nullptr, received);
    } else {
        this->Close();
    }
}

// OFFSET: 0x0086DBF0
void OsNetAddrMake(uint32_t address, uint16_t port, NETADDR* addr) {
    sockaddr_in* in = reinterpret_cast<sockaddr_in*>(addr);
    in->sin_family = AF_INET;
    in->sin_port = htons(port);
    in->sin_addr.s_addr = address;
    *reinterpret_cast<uint32_t*>(&in->sin_zero[0]) = 0;
    *reinterpret_cast<uint32_t*>(&in->sin_zero[4]) = 0;
}

// OFFSET: 0x0086DC20
uint32_t OsNetAddrGetAddress(const NETADDR* addr, uint16_t* port) {
    const sockaddr_in* in = reinterpret_cast<const sockaddr_in*>(addr);

    if (port) {
        *port = ntohs(in->sin_port);
    }

    return in->sin_addr.s_addr;
}

// OFFSET: 0x0086DC50
const char* OsNetAddrToStr(const NETADDR* addr, char* buffer, uint32_t size) {
    const sockaddr_in* in = reinterpret_cast<const sockaddr_in*>(addr);
    SStrPrintf(buffer, size, "%s:%u", inet_ntoa(in->sin_addr), ntohs(in->sin_port));
    return buffer;
}

// OFFSET: 0x0086DC90
void OsNet::TCPACCEPT::Init() {
    this->m_lock.Enter();

    if (this->m_listen->m_enabled) {
        if (this->m_sock == INVALID_SOCKET) {
            this->m_sock = socket(AF_INET, SOCK_STREAM, 0);

            if (this->m_sock == INVALID_SOCKET) {
                OsNet::TCPNET::LogWrite("%s 1", OsNet::OSNETERR_SOCKETFAILED);
                this->m_lock.Leave();
                return;
            }

            DWORD bytes;

            if (!OsNet::TCPNET::s_AcceptEx(this->m_listen->m_sock, this->m_sock, this->m_addr, 0, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, &bytes, &this->m_overlap.m_overlapped) && WSAGetLastError() != WSA_IO_PENDING) {
                OsNet::TCPNET::LogWrite(OsNet::OSNETERR_ACCEPTEXFAILED);
                this->m_lock.Leave();
                return;
            }
        }
    } else if (this->m_sock != INVALID_SOCKET) {
        closesocket(this->m_sock);
        this->m_sock = INVALID_SOCKET;
    }

    this->m_lock.Leave();
}

// OFFSET: 0x0086DD60
SOCKET OsNet::TCPACCEPT::Complete(NETCONNADDR* connAddr, int32_t makeSock) {
    this->m_lock.Enter();

    if (!this->m_listen->m_enabled) {
        closesocket(this->m_sock);
        this->m_sock = INVALID_SOCKET;
        this->m_lock.Leave();
        return INVALID_SOCKET;
    }

    SOCKET sock = this->m_sock;

    if (makeSock) {
        sockaddr* selfAddr;
        int selfSize;
        sockaddr* peerAddr;
        int peerSize;
        OsNet::TCPNET::s_GetAcceptExSockaddrs(this->m_addr, 0, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, &selfAddr, &selfSize, &peerAddr, &peerSize);
        connAddr->peerAddr = *reinterpret_cast<NETADDR*>(peerAddr);
        connAddr->selfAddr = *reinterpret_cast<NETADDR*>(selfAddr);
        setsockopt(sock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, reinterpret_cast<const char*>(&this->m_listen->m_sock), sizeof(this->m_listen->m_sock));
    } else {
        closesocket(sock);
        sock = INVALID_SOCKET;
    }

    this->m_sock = INVALID_SOCKET;
    this->Init();
    this->m_lock.Leave();

    return sock;
}

// OFFSET: 0x0086DE50
void OsNet::NETCONN::Close() {
    this->m_lock.Enter();
    this->CloseAndUnlock();
}

// OFFSET: 0x0086DE70
OS_SEND OsNet::TCPCONN::SendSync(const void* data, uint32_t bytes, uint32_t* bytesSent, uint32_t timeout) {
    *bytesSent = 0;
    this->m_lock.Enter();

    if (this->m_sock == INVALID_SOCKET) {
        this->m_lock.Leave();
        return OS_SEND_SOCKET_CLOSED;
    }

    uint32_t startTime = OsGetAsyncTimeMs();
    int32_t remaining = timeout;
    OUTPUT* output = this->m_outputList.Tail();

    while (output) {
        SEvent event(0, 0);
        output->m_completionEvent = &event;
        this->m_lock.Leave();

        if (event.Wait(remaining)) {
            this->m_lock.Enter();
            output = this->m_outputList.Tail();

            if (output) {
                output->m_completionEvent = nullptr;
            }

            this->m_lock.Leave();
            return OS_SEND_TIMEOUT;
        }

        remaining += startTime - OsGetAsyncTimeMs();
        this->m_lock.Enter();
        output = this->m_outputList.Tail();

        if (output && remaining <= 0) {
            this->m_lock.Leave();
            return OS_SEND_TIMEOUT;
        }
    }

    const char* buffer = static_cast<const char*>(data);

    while (true) {
        int32_t sent = send(this->m_sock, buffer, bytes, 0);

        if (sent == SOCKET_ERROR) {
            int32_t error = WSAGetLastError();

            if (error != WSAEWOULDBLOCK) {
                if (error != WSAECONNABORTED && error != WSAECONNRESET) {
                    OsNet::TCPNET::LogWrite("%s 1", OsNet::OSNETERR_SENDFAILED);
                }

                this->Close();
                this->m_lock.Leave();
                return OS_SEND_ERROR;
            }

            fd_set writefds;
            writefds.fd_array[0] = this->m_sock;
            writefds.fd_count = 1;

            int32_t wait = startTime - OsGetAsyncTimeMs() + remaining;
            timeval tv;
            tv.tv_sec = wait / 1000;
            tv.tv_usec = 1000 * (wait % 1000);

            this->m_lock.Leave();
            select(static_cast<int>(this->m_sock) + 1, nullptr, &writefds, nullptr, &tv);
            this->m_lock.Enter();

            remaining = startTime - OsGetAsyncTimeMs() + wait;

            if (!FD_ISSET(this->m_sock, &writefds) && remaining < 0) {
                this->m_lock.Leave();
                return OS_SEND_TIMEOUT;
            }

            continue;
        }

        *bytesSent += sent;

        if (sent >= static_cast<int32_t>(bytes)) {
            this->m_lock.Leave();
            return OS_SEND_OK;
        }

        buffer += sent;
        bytes -= sent;
    }
}

// OFFSET: 0x0086E0C0
void OsNet::SLTCPCONN::ContinueWrite() {
    this->m_lock.Enter();

    if (this->m_sock == INVALID_SOCKET) {
        this->m_lock.Leave();
        return;
    }

    OUTPUT* output;

    while ((output = this->m_outputList.Head()) != nullptr) {
        output->m_state = OUTPUTSTATE_WRITING;
        int32_t sent = send(this->m_sock, reinterpret_cast<const char*>(output->m_data), output->m_bytes, 0);

        if (sent == SOCKET_ERROR) {
            int32_t error = WSAGetLastError();

            if (error != WSAEWOULDBLOCK) {
                if (error != WSAECONNABORTED && error != WSAECONNRESET) {
                    OsNet::TCPNET::LogWrite("%s 2", OsNet::OSNETERR_SENDFAILED);
                }

                this->Close();
            }

            break;
        }

        if (sent < static_cast<int32_t>(output->m_bytes)) {
            memmove(output->m_data, &output->m_data[sent], output->m_bytes - sent);
            output->m_bytes -= sent;
            this->m_lock.Leave();
            return;
        }

        this->CompleteWrite(&output->m_overlap, output->m_bytes);
    }

    this->m_lock.Leave();
}

// OFFSET: 0x0086E1C0
void OsNet::SLTCPCONN::Selected(TCPNET* net, SELECTSET selectSet) {
    if (selectSet == SELECTSET_R) {
        this->ContinueRead();
    } else if (selectSet == SELECTSET_W) {
        this->ContinueWrite();
    } else {
        OsNet::TCPNET::LogWrite("%s 13", OsNet::OSNETERR_INTERNAL);
    }
}

// OFFSET: [0x0086E260, 0x0086E2B0]
OsNet::NETCONN::~NETCONN() {
}

// OFFSET: 0x0086E2E0
OsNet::NETCONN::NETCONN(TCPNET* net, SOCKET sock, NETEVENTPROC eventProc, void* user, const NETCONNADDR* connAddr) {
    this->m_sock = sock;
    this->m_listSlot = 0;
    this->m_list = CONNLIST_NONE;
    this->m_refCount.m_value = 1;
    this->m_eventProcUserLock = 0;
    this->m_eventProc = eventProc;
    this->m_user = user;
    this->m_net = net;
    this->m_time = GetTickCount();
    memcpy(&this->m_connAddr, connAddr, sizeof(this->m_connAddr));
}

// OFFSET: 0x0086E350
template <class T, int LINKOFFSET, int SLOTS>
uint8_t OsNet::TSSlottedListEx<T, LINKOFFSET, SLOTS>::LinkNode(T* ptr) {
    InterlockedIncrement(&this->m_linkSlot.m_value);
    uint8_t slot = this->m_linkSlot.m_value & (SLOTS - 1);

    this->m_locks[slot].Enter();
    this->m_lists[slot].LinkToTail(ptr);
    this->m_locks[slot].Leave();

    InterlockedIncrement(&this->m_count.m_value);

    return slot;
}

// OFFSET: 0x0086E3B0
template <class T, int LINKOFFSET, int SLOTS>
void OsNet::TSSlottedListEx<T, LINKOFFSET, SLOTS>::UnlinkNode(T* ptr, uint8_t slot) {
    this->m_locks[slot].Enter();
    this->m_lists[slot].UnlinkNode(ptr);
    this->m_locks[slot].Leave();

    InterlockedDecrement(&this->m_count.m_value);
}

// OFFSET: 0x0086E440
void OsNet::TCPNET::LinkConn(NETCONN* conn, CONNLIST toList) {
    conn->m_lock.Enter();

    uint8_t slot = 0;

    if (conn->m_list != CONNLIST_NONE) {
        this->m_connList[conn->m_list].UnlinkNode(conn, conn->m_listSlot);
    }

    if (toList != CONNLIST_NONE) {
        slot = this->m_connList[toList].LinkNode(conn);
    }

    conn->m_list = toList;
    conn->m_listSlot = slot;

    conn->m_lock.Leave();
}

// OFFSET: 0x0086E4B0
void OsNet::NETCONN::Disconnect(int32_t notify) {
    this->Close();
    this->m_net->LinkConn(this, CONNLISTS);

    if (notify) {
        this->NoteDisconnect();
    }

    this->DecRef();
}

// OFFSET: 0x0086E550
OsNet::TCPCONN::TCPCONN(TCPNET* net, SOCKET sock, NETEVENTPROC eventProc, void* user, const NETCONNADDR* connAddr) : NETCONNFULL(net, sock, eventProc, user, connAddr) {
    this->m_bytes = 0;

    BOOL keepAlive = 1;
    setsockopt(this->m_sock, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const char*>(&keepAlive), sizeof(keepAlive));

    u_long nonBlocking = 1;
    ioctlsocket(this->m_sock, FIONBIO, &nonBlocking);
}

// OFFSET: 0x0086E5D0
OsNet::OUTPUT* OsNet::TCPCONN::LockedEnqueue(const void* data, uint32_t bytes) {
    uint32_t time = GetTickCount();
    OUTPUT* head = this->m_outputList.Head();

    if (head && time - this->m_time > 600000 && time - head->m_time > 120000) {
        this->Close();
        return nullptr;
    }

    const uint8_t* buffer = static_cast<const uint8_t*>(data);
    OUTPUT* output = this->m_outputList.Tail();

    if (output && output->m_state == OUTPUTSTATE_WAITING && output->m_bytes < output->m_dataBytes) {
        uint32_t copy = output->m_dataBytes - output->m_bytes;

        if (copy > bytes) {
            copy = bytes;
        }

        memcpy(&output->m_data[output->m_bytes], buffer, copy);
        output->m_bytes += copy;
        buffer += copy;
        bytes -= copy;
    }

    if (bytes) {
        output = new (SMemAlloc(sizeof(OUTPUT), __FILE__, __LINE__, 0)) OUTPUT();
        output->m_state = OUTPUTSTATE_WAITING;
        output->m_time = time;
        output->m_bytes = bytes;
        output->m_dataBytes = bytes > sizeof(this->m_data) ? bytes : sizeof(this->m_data);
        output->m_data = static_cast<uint8_t*>(SMemAlloc(output->m_dataBytes, __FILE__, __LINE__, 0));
        memcpy(output->m_data, buffer, bytes);
        this->m_outputList.LinkToTail(output);
    }

    return output;
}

// OFFSET: 0x0086E720
void OsNet::TCPCONN::Send(const void* data, uint32_t bytes) {
    this->m_lock.Enter();

    if (this->m_sock == INVALID_SOCKET) {
        this->m_lock.Leave();
        return;
    }

    const char* buffer = static_cast<const char*>(data);
    int32_t oldOutput = this->m_outputList.Head() != nullptr;

    if (!oldOutput) {
        int32_t sent = send(this->m_sock, buffer, bytes, 0);

        if (sent == SOCKET_ERROR) {
            int32_t error = WSAGetLastError();

            if (error != WSAEWOULDBLOCK) {
                if (error != WSAECONNABORTED && error != WSAECONNRESET) {
                    OsNet::TCPNET::LogWrite("%s 1", OsNet::OSNETERR_SENDFAILED);
                }

                this->Close();
                this->m_lock.Leave();
                return;
            }
        } else {
            if (sent >= static_cast<int32_t>(bytes)) {
                this->m_lock.Leave();
                return;
            }

            buffer += sent;
            bytes -= sent;
        }
    }

    OUTPUT* output = this->LockedEnqueue(buffer, bytes);

    if (!oldOutput && output) {
        this->StartWriteAndLeaveLock(output);
        return;
    }

    this->m_lock.Leave();
}

// OFFSET: 0x0086E810
void OsNet::IOTCPCONN::DecIo() {
    this->m_lock.Enter();

    if (InterlockedDecrement(&this->m_ioCount.m_value)) {
        this->m_lock.Leave();
        return;
    }

    this->CloseAndUnlock();
    this->Disconnect(1);
}

// OFFSET: [0x0086EA40, 0x0086EEA0]
OsNet::TCPCONN::~TCPCONN() {
    this->m_outputList.Clear();
    this->m_net->LinkConn(this, CONNLISTS);
}

// OFFSET: 0x0086EAF0
void OsNet::TCPCONN::CompleteWrite(NETOVERLAP* overlap, uint32_t bytes) {
    this->m_lock.Enter();

    reinterpret_cast<OUTPUT*>(reinterpret_cast<uint8_t*>(overlap) - offsetof(OUTPUT, m_overlap))->m_state = OUTPUTSTATE_COMPLETED;

    OUTPUT* output;

    while ((output = this->m_outputList.Head()) != nullptr) {
        if (output->m_state != OUTPUTSTATE_COMPLETED) {
            this->StartWriteAndLeaveLock(output);
            return;
        }

        this->m_outputList.DeleteNode(output);
    }

    this->m_lock.Leave();
}

// OFFSET: 0x0086EBD0
OsNet::IOTCPCONN::IOTCPCONN(TCPNET* net, HANDLE port, SOCKET sock, NETEVENTPROC eventProc, void* user, const NETCONNADDR* connAddr, const void* data, uint32_t bytes) : TCPCONN(net, sock, eventProc, user, connAddr) {
    this->m_ioCount.m_value = 1;

    if (!CreateIoCompletionPort(reinterpret_cast<HANDLE>(this->m_sock), port, reinterpret_cast<ULONG_PTR>(this), 0)) {
        OsNet::TCPNET::LogWrite("%s 2", OsNet::OSNETERR_PORTFAILED);
        this->Disconnect(0);
        return;
    }

    if (data && bytes) {
        this->Send(data, bytes);
    }

    if (this->NoteConnect()) {
        this->m_net->LinkConn(this, CONNLIST_TCP_CONNECTED);
        this->StartRead();
    }

    this->DecIo();
}

// OFFSET: 0x0086ED30
OsNet::SLTCPCONN::SLTCPCONN(TCPNET* net, SOCKET sock, NETEVENTPROC eventProc, void* user, const NETCONNADDR* connAddr, const void* data, uint32_t bytes) : TCPCONN(net, sock, eventProc, user, connAddr) {
    if (data && bytes) {
        this->Send(data, bytes);
    }

    if (this->NoteConnect()) {
        this->m_net->LinkConn(this, CONNLIST_TCP_CONNECTED);
    } else {
        this->Disconnect(1);
    }
}

// OFFSET: 0x0086EE00
void OsNet::TCPNET::TcpMakeConn(SOCKET sock, NETEVENTPROC eventProc, void* user, const NETCONNADDR* connAddr, const void* data, uint32_t bytes) {
    if (this->m_port) {
        new (SMemAlloc(sizeof(IOTCPCONN), __FILE__, __LINE__, 0)) IOTCPCONN(this, this->m_port, sock, eventProc, user, connAddr, data, bytes);
    } else {
        new (SMemAlloc(sizeof(SLTCPCONN), __FILE__, __LINE__, 0)) SLTCPCONN(this, sock, eventProc, user, connAddr, data, bytes);
    }
}

// INLINED
void OsNet::TCPNET::CompleteAcceptEx(TCPACCEPT* accept, int32_t makeConn) {
    NETCONNADDR connAddr;
    SOCKET sock = accept->Complete(&connAddr, makeConn);

    if (sock != INVALID_SOCKET) {
        this->TcpMakeConn(sock, accept->m_listen->m_eventProc, accept->m_listen->m_user, &connAddr, nullptr, 0);
    }
}

// OFFSET: 0x0086EFE0
void OsNet::TCPNET::IoPump(uint32_t timeout) {
    uint32_t elapsed = 0;
    LARGE_INTEGER startTime;
    startTime.QuadPart = 0;

    if (timeout != INFINITE) {
        if (OsNet::TCPNET::s_qpcexists) {
            QueryPerformanceCounter(&startTime);
        } else {
            startTime.LowPart = GetTickCount();
        }
    }

    DWORD bytes;
    ULONG_PTR key;
    LPOVERLAPPED overlapped;
    BOOL result = GetQueuedCompletionStatus(this->m_port, &bytes, &key, &overlapped, timeout);

    while (!OsNet::TCPNET::s_pumpShutdown) {
        if (timeout != INFINITE && !result && !overlapped) {
            return;
        }

        NETCONN* conn = reinterpret_cast<NETCONN*>(key);
        NETOVERLAP* overlap = reinterpret_cast<NETOVERLAP*>(overlapped);

        if (!conn) {
            if (overlap) {
                if (overlap->m_type == OVERLAPTYPE_ACCEPT) {
                    this->CompleteAcceptEx(reinterpret_cast<TCPACCEPT*>(reinterpret_cast<uint8_t*>(overlap) - offsetof(TCPACCEPT, m_overlap)), result);
                } else {
                    OsNet::TCPNET::LogWrite("%s 2", OsNet::OSNETERR_OVERLAPTYPE);
                }
            }
        } else if (result && overlap && overlap->m_type != OVERLAPTYPE_READ) {
            if (overlap->m_type == OVERLAPTYPE_WRITE) {
                conn->CompleteWrite(overlap, bytes);
            } else {
                OsNet::TCPNET::LogWrite("%s 1", OsNet::OSNETERR_OVERLAPTYPE);
            }

            conn->DecIo();
        } else {
            if (result && overlap) {
                conn->CompleteRead(overlap, bytes);
            }

            conn->DecIo();
        }

        if (timeout != INFINITE) {
            if (OsNet::TCPNET::s_qpcexists) {
                LARGE_INTEGER currTime;
                QueryPerformanceCounter(&currTime);
                int32_t ms = static_cast<int32_t>((currTime.QuadPart - startTime.QuadPart) * OsNet::TCPNET::s_qpctoms);
                elapsed = static_cast<uint32_t>(ms);

                if (ms < 0) {
                    return;
                }
            } else {
                elapsed = GetTickCount() - startTime.LowPart;
            }

            if (elapsed >= timeout) {
                return;
            }
        }

        result = GetQueuedCompletionStatus(this->m_port, &bytes, &key, &overlapped, timeout - elapsed);
    }
}

// INLINED
void OsNet::TCPNET::Pump(uint32_t timeout) {
    if (this->m_pumpThreads.Count()) {
        Sleep(timeout);
    } else if (this->m_port) {
        this->IoPump(timeout);
    } else {
        OsNet::TCPNET::LogWrite("%s 7", OsNet::OSNETERR_INTERNAL);
        Sleep(timeout);
    }
}

// OFFSET: 0x0086F1C0
void OsNetPump(uint32_t timeout) {
    if (OsNet::TCPNET::s_pnet) {
        OsNet::TCPNET::s_pnet->Pump(timeout);
    } else {
        Sleep(timeout);
    }
}

// OFFSET: 0x0086F550
void OsNet::SLTCPCONN::AddToSelectSets(NETSELECTSETS* selectSets) {
    NETSELSOCKPTR* selsockptr = selectSets->m_selsockTable.New(static_cast<uint32_t>(this->m_sock), HASHKEY_NONE(), 0, 0);
    selsockptr->ptr = this;

    selectSets->AddToSet(this, SELECTSET_R);

    if (this->m_outputList.Head()) {
        selectSets->AddToSet(this, SELECTSET_W);
    }
}

