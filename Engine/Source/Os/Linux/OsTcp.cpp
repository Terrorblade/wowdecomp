#if defined(WOWDECOMP_PLATFORM_LINUX)

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <arpa/inet.h>
#include <cerrno>
#include <ctime>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include "Engine/Source/Os/Linux/OsTcpConn.h"
#include "Engine/Source/Os/TimeManager.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Storm/Source/SLock.h"

// Winsock errors map to errno: WSAEWOULDBLOCK is EAGAIN/EWOULDBLOCK, WSAECONNABORTED/WSAECONNRESET are
// ECONNABORTED/ECONNRESET (and EPIPE, which Linux reports for a send on a reset connection). Sends pass MSG_NOSIGNAL
// so a closed peer is an error result, as on Windows, instead of SIGPIPE.

static uint32_t TickCount() {
    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<uint32_t>(static_cast<uint64_t>(now.tv_sec) * 1000 + now.tv_nsec / 1000000);
}

static bool WouldBlock(int error) {
    return error == EAGAIN || error == EWOULDBLOCK;
}

static bool ConnectionLost(int error) {
    return error == ECONNABORTED || error == ECONNRESET || error == EPIPE;
}

static void SleepMs(uint32_t milliseconds) {
    timespec request;
    request.tv_sec = milliseconds / 1000;
    request.tv_nsec = static_cast<long>(milliseconds % 1000) * 1000000;

    while (nanosleep(&request, &request) && errno == EINTR) {
    }
}

const char* OsNet::OSNETERR_INTERNAL = "Internal error";

const char* OsNet::OSNETERR_WSASTARTUPFAILED = "WSAStartup failed";

const char* OsNet::OSNETERR_WSASTARTUPVERSION = "WSAStartup wrong version";

const char* OsNet::OSNETERR_SENDFAILED = "Send failed";

const char* OsNet::OSNETERR_CREATETHREADFAILED = "CreateThread failed";

const char* OsNet::OSNETERR_CREATEEVENTFAILED = "CreateEvent failed";

const char* OsNet::OSNETERR_BINDFAILED = "Bind failed";

const char* OsNet::OSNETERR_LISTENFAILED = "Listen failed";

const char* OsNet::OSNETERR_SOCKETFAILED = "Socket failed";

const char* OsNet::OSNETERR_SELECTFAILED = "Select failed";

const char* OsNet::OSNETERR_ACCEPTFAILED = "Accept failed";

const char* OsNet::OSNETERR_PORTFAILED = "CreateIoCompletionPort failed";

const char* OsNet::OSNETERR_OVERLAPTYPE = "Unknown overlap type";

const char* OsNet::OSNETERR_LISTENCLOSED = "Listen closed";

const char* OsNet::OSNETERR_ACCEPTEXFAILED = "AcceptEx failed";

float OsNet::TCPNET::s_qpctoms;

int32_t OsNet::TCPNET::s_qpcexists;

HSLOG OsNet::TCPNET::s_log;

int32_t OsNet::TCPNET::s_pumpShutdown;

OsNet::TCPNET* OsNet::TCPNET::s_pnet;

OsNet::OUTPUT::OUTPUT() {
    memset(&this->m_overlap.m_overlapped, 0, sizeof(this->m_overlap.m_overlapped));
    this->m_overlap.m_type = OVERLAPTYPE_WRITE;
    this->m_completionEvent = nullptr;
}

OsNet::OUTPUT::~OUTPUT() {
    if (this->m_dataBytes && this->m_data) {
        SMemFree(this->m_data, __FILE__, __LINE__, 0);
    }

    if (this->m_completionEvent) {
        this->m_completionEvent->Set();
    }
}

OsNet::NETCONNFULL::NETCONNFULL(TCPNET* net, SOCKET sock, NETEVENTPROC eventProc, void* user, const NETCONNADDR* connAddr) : NETCONN(net, sock, eventProc, user, connAddr) {
}

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

int32_t OsNet::NETCONN::NoteData(void* data, uint32_t bytes, uint32_t* bytesProcessed, const NETCONNADDR* connAddr) {
    NETEVENTPROC eventProc;
    void* user;
    this->GetEventProcAndUser(eventProc, user);

    if (!eventProc) {
        return 0;
    }

    return eventProc(reinterpret_cast<HNETCONN>(this), connAddr ? connAddr : &this->m_connAddr, NETNOTE_DATA, user, data, bytes, bytesProcessed);
}

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

int32_t OsNet::NETSELSOCK::IsClosed() {
    return this->m_sock == INVALID_SOCKET;
}

void OsNet::NETSELECTSETS::AddToSet(NETSELSOCK* selsock, SELECTSET selectSet) {
    FD_SET(selsock->m_sock, &this->m_sets[selectSet]);
}

void OsNet::TCPNET::LogWrite(const char* format, ...) {
    char line[1024];

    int error = errno;
    uint32_t length = static_cast<uint32_t>(SStrPrintf(line, sizeof(line), "t[0x%08x] se[%u] we[%u] ", static_cast<uint32_t>(gettid()), error, error));

    va_list arglist;
    va_start(arglist, format);
    SStrVPrintf(&line[length], sizeof(line) - length, format, arglist);

    DebugLog("OsNet: (INFO): %s\n", line);

    if (OsNet::TCPNET::s_log) {
        SLogWrite(OsNet::TCPNET::s_log, line);
        SLogFlush(OsNet::TCPNET::s_log);
    }
}

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

void OsNet::TCPCONN::CloseAndUnlock() {
    if (this->m_sock != INVALID_SOCKET) {
        close(this->m_sock);
        this->m_sock = INVALID_SOCKET;
    }

    this->m_lock.Leave();
}

void OsNet::TCPCONN::SetNagle(int32_t enable) {
    int noDelay = !enable;
    setsockopt(this->m_sock, IPPROTO_TCP, TCP_NODELAY, &noDelay, sizeof(noDelay));
}

int32_t OsNet::TCPCONN::SetWindow(uint32_t size) {
    int value = static_cast<int>(size);

    if (setsockopt(this->m_sock, SOL_SOCKET, SO_SNDBUF, &value, sizeof(value))) {
        return 0;
    }

    return setsockopt(this->m_sock, SOL_SOCKET, SO_RCVBUF, &value, sizeof(value)) == 0;
}

void OsNet::TCPCONN::SetRecvTimeout(uint32_t timeout) {
    // Winsock takes milliseconds as a DWORD; Linux takes a timeval.
    timeval tv;
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = static_cast<suseconds_t>(timeout % 1000) * 1000;
    setsockopt(this->m_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}

void OsNet::SLTCPCONN::StartWriteAndLeaveLock(OUTPUT* output) {
    this->m_lock.Leave();
}

void OsNet::SLTCPCONN::ContinueRead() {
    uint32_t bytes = sizeof(this->m_data) - this->m_bytes;

    if (bytes >= sizeof(this->m_data)) {
        bytes = sizeof(this->m_data);
    }

    int32_t received = static_cast<int32_t>(recv(this->m_sock, &this->m_data[this->m_bytes], bytes, 0));

    if (received == SOCKET_ERROR) {
        if (!WouldBlock(errno)) {
            this->Close();
        }
    } else if (received) {
        this->CompleteRead(nullptr, received);
    } else {
        this->Close();
    }
}

void OsNetAddrMake(uint32_t address, uint16_t port, NETADDR* addr) {
    sockaddr_in* in = reinterpret_cast<sockaddr_in*>(addr);
    in->sin_family = AF_INET;
    in->sin_port = htons(port);
    in->sin_addr.s_addr = address;
    memset(in->sin_zero, 0, sizeof(in->sin_zero));
}

uint32_t OsNetAddrGetAddress(const NETADDR* addr, uint16_t* port) {
    const sockaddr_in* in = reinterpret_cast<const sockaddr_in*>(addr);

    if (port) {
        *port = ntohs(in->sin_port);
    }

    return in->sin_addr.s_addr;
}

const char* OsNetAddrToStr(const NETADDR* addr, char* buffer, uint32_t size) {
    const sockaddr_in* in = reinterpret_cast<const sockaddr_in*>(addr);
    char address[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &in->sin_addr, address, sizeof(address));
    SStrPrintf(buffer, size, "%s:%u", address, ntohs(in->sin_port));
    return buffer;
}

void OsNet::TCPACCEPT::Init() {
    this->m_lock.Enter();

    // Without AcceptEx there is nothing to post in advance (Complete accepts from the listening socket); only the
    // close of a pending socket when listening is disabled remains.
    if (!this->m_listen->m_enabled && this->m_sock != INVALID_SOCKET) {
        close(this->m_sock);
        this->m_sock = INVALID_SOCKET;
    }

    this->m_lock.Leave();
}

SOCKET OsNet::TCPACCEPT::Complete(NETCONNADDR* connAddr, int32_t makeSock) {
    this->m_lock.Enter();

    if (!this->m_listen->m_enabled) {
        if (this->m_sock != INVALID_SOCKET) {
            close(this->m_sock);
        }

        this->m_sock = INVALID_SOCKET;
        this->m_lock.Leave();
        return INVALID_SOCKET;
    }

    SOCKET sock = this->m_sock;

    if (sock == INVALID_SOCKET) {
        sock = accept4(this->m_listen->m_sock, nullptr, nullptr, SOCK_CLOEXEC);

        if (sock == INVALID_SOCKET && !WouldBlock(errno)) {
            OsNet::TCPNET::LogWrite(OsNet::OSNETERR_ACCEPTFAILED);
        }
    }

    if (sock != INVALID_SOCKET && makeSock) {
        sockaddr_in selfAddr = {};
        socklen_t selfSize = sizeof(selfAddr);
        sockaddr_in peerAddr = {};
        socklen_t peerSize = sizeof(peerAddr);
        getsockname(sock, reinterpret_cast<sockaddr*>(&selfAddr), &selfSize);
        getpeername(sock, reinterpret_cast<sockaddr*>(&peerAddr), &peerSize);
        memcpy(&connAddr->peerAddr, &peerAddr, sizeof(connAddr->peerAddr));
        memcpy(&connAddr->selfAddr, &selfAddr, sizeof(connAddr->selfAddr));
    } else if (sock != INVALID_SOCKET) {
        close(sock);
        sock = INVALID_SOCKET;
    }

    this->m_sock = INVALID_SOCKET;
    this->Init();
    this->m_lock.Leave();

    return sock;
}

void OsNet::NETCONN::Close() {
    this->m_lock.Enter();
    this->CloseAndUnlock();
}

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
        int32_t sent = static_cast<int32_t>(send(this->m_sock, buffer, bytes, MSG_NOSIGNAL));

        if (sent == SOCKET_ERROR) {
            int32_t error = errno;

            if (!WouldBlock(error)) {
                if (!ConnectionLost(error)) {
                    OsNet::TCPNET::LogWrite("%s 1", OsNet::OSNETERR_SENDFAILED);
                }

                this->Close();
                this->m_lock.Leave();
                return OS_SEND_ERROR;
            }

            // poll instead of a one-socket select (and no FD_SETSIZE limit on the descriptor).
            pollfd writefd;
            writefd.fd = this->m_sock;
            writefd.events = POLLOUT;
            writefd.revents = 0;

            int32_t wait = startTime - OsGetAsyncTimeMs() + remaining;

            this->m_lock.Leave();
            poll(&writefd, 1, wait > 0 ? wait : 0);
            this->m_lock.Enter();

            remaining = startTime - OsGetAsyncTimeMs() + wait;

            if (!(writefd.revents & POLLOUT) && remaining < 0) {
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

void OsNet::SLTCPCONN::ContinueWrite() {
    this->m_lock.Enter();

    if (this->m_sock == INVALID_SOCKET) {
        this->m_lock.Leave();
        return;
    }

    OUTPUT* output;

    while ((output = this->m_outputList.Head()) != nullptr) {
        output->m_state = OUTPUTSTATE_WRITING;
        int32_t sent = static_cast<int32_t>(send(this->m_sock, output->m_data, output->m_bytes, MSG_NOSIGNAL));

        if (sent == SOCKET_ERROR) {
            int32_t error = errno;

            if (!WouldBlock(error)) {
                if (!ConnectionLost(error)) {
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

void OsNet::SLTCPCONN::Selected(TCPNET* net, SELECTSET selectSet) {
    if (selectSet == SELECTSET_R) {
        this->ContinueRead();
    } else if (selectSet == SELECTSET_W) {
        this->ContinueWrite();
    } else {
        OsNet::TCPNET::LogWrite("%s 13", OsNet::OSNETERR_INTERNAL);
    }
}

OsNet::NETCONN::~NETCONN() {
}

OsNet::NETCONN::NETCONN(TCPNET* net, SOCKET sock, NETEVENTPROC eventProc, void* user, const NETCONNADDR* connAddr) {
    this->m_sock = sock;
    this->m_listSlot = 0;
    this->m_list = CONNLIST_NONE;
    this->m_refCount.m_value = 1;
    this->m_eventProcUserLock = 0;
    this->m_eventProc = eventProc;
    this->m_user = user;
    this->m_net = net;
    this->m_time = TickCount();
    memcpy(&this->m_connAddr, connAddr, sizeof(this->m_connAddr));
}

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

template <class T, int LINKOFFSET, int SLOTS>
void OsNet::TSSlottedListEx<T, LINKOFFSET, SLOTS>::UnlinkNode(T* ptr, uint8_t slot) {
    this->m_locks[slot].Enter();
    this->m_lists[slot].UnlinkNode(ptr);
    this->m_locks[slot].Leave();

    InterlockedDecrement(&this->m_count.m_value);
}

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

void OsNet::NETCONN::Disconnect(int32_t notify) {
    this->Close();
    this->m_net->LinkConn(this, CONNLISTS);

    if (notify) {
        this->NoteDisconnect();
    }

    this->DecRef();
}

OsNet::TCPCONN::TCPCONN(TCPNET* net, SOCKET sock, NETEVENTPROC eventProc, void* user, const NETCONNADDR* connAddr) : NETCONNFULL(net, sock, eventProc, user, connAddr) {
    this->m_bytes = 0;

    int keepAlive = 1;
    setsockopt(this->m_sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(keepAlive));

    int nonBlocking = 1;
    ioctl(this->m_sock, FIONBIO, &nonBlocking);
}

OsNet::OUTPUT* OsNet::TCPCONN::LockedEnqueue(const void* data, uint32_t bytes) {
    uint32_t time = TickCount();
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

void OsNet::TCPCONN::Send(const void* data, uint32_t bytes) {
    this->m_lock.Enter();

    if (this->m_sock == INVALID_SOCKET) {
        this->m_lock.Leave();
        return;
    }

    const char* buffer = static_cast<const char*>(data);
    int32_t oldOutput = this->m_outputList.Head() != nullptr;

    if (!oldOutput) {
        int32_t sent = static_cast<int32_t>(send(this->m_sock, buffer, bytes, MSG_NOSIGNAL));

        if (sent == SOCKET_ERROR) {
            int32_t error = errno;

            if (!WouldBlock(error)) {
                if (!ConnectionLost(error)) {
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

OsNet::TCPCONN::~TCPCONN() {
    this->m_outputList.Clear();
    this->m_net->LinkConn(this, CONNLISTS);
}

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

void OsNet::TCPNET::TcpMakeConn(SOCKET sock, NETEVENTPROC eventProc, void* user, const NETCONNADDR* connAddr, const void* data, uint32_t bytes) {
    new (SMemAlloc(sizeof(SLTCPCONN), __FILE__, __LINE__, 0)) SLTCPCONN(this, sock, eventProc, user, connAddr, data, bytes);
}

void OsNet::TCPNET::CompleteAcceptEx(TCPACCEPT* accept, int32_t makeConn) {
    NETCONNADDR connAddr;
    SOCKET sock = accept->Complete(&connAddr, makeConn);

    if (sock != INVALID_SOCKET) {
        this->TcpMakeConn(sock, accept->m_listen->m_eventProc, accept->m_listen->m_user, &connAddr, nullptr, 0);
    }
}

void OsNet::TCPNET::Pump(uint32_t timeout) {
    if (this->m_pumpThreads.Count()) {
        SleepMs(timeout);
    } else {
        OsNet::TCPNET::LogWrite("%s 7", OsNet::OSNETERR_INTERNAL);
        SleepMs(timeout);
    }
}

void OsNetPump(uint32_t timeout) {
    if (OsNet::TCPNET::s_pnet) {
        OsNet::TCPNET::s_pnet->Pump(timeout);
    } else {
        SleepMs(timeout);
    }
}

void OsNet::SLTCPCONN::AddToSelectSets(NETSELECTSETS* selectSets) {
    NETSELSOCKPTR* selsockptr = selectSets->m_selsockTable.New(static_cast<uint32_t>(this->m_sock), HASHKEY_NONE(), 0, 0);
    selsockptr->ptr = this;

    selectSets->AddToSet(this, SELECTSET_R);

    if (this->m_outputList.Head()) {
        selectSets->AddToSet(this, SELECTSET_W);
    }
}

#endif
