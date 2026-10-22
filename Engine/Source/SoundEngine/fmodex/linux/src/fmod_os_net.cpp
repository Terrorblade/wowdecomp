#if defined(WOWDECOMP_PLATFORM_POSIX)

#include "fmod_settings.h"

#include "fmod_net.h"
#include "fmod_os_misc.h"
#include "fmod_os_net.h"
#include "fmod_time.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

/*
    Socket handles are file descriptors stored in the void * handle; (void *)-1 is the invalid handle, as on Windows.
*/
#define FMOD_NET_SOCKET(_handle)   ((int)(intptr_t)(_handle))
#define FMOD_NET_HANDLE(_sock)     ((void *)(intptr_t)(_sock))

static bool                     FMOD_OS_Net_Initialised  = false;
static FMOD_OS_CRITICALSECTION *gResolveCrit             = 0;


static void FMOD_OS_Net_SetNonBlocking(int sock, bool nonblocking)
{
    int flags = fcntl(sock, F_GETFL, 0);

    if (flags == -1)
    {
        return;
    }

    fcntl(sock, F_SETFL, nonblocking ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK));
}


/*
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
FMOD_RESULT FMOD_OS_Net_Init()
{
    FMOD_RESULT result;

    if (!FMOD_OS_Net_Initialised)
    {
        result = FMOD_OS_CriticalSection_Create(&gResolveCrit);
        if (result != FMOD_OK)
        {
            return result;
        }

        FMOD_OS_Net_Initialised = true;
    }

    return FMOD_OK;
}


/*
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
FMOD_RESULT FMOD_OS_Net_Shutdown()
{
    if (FMOD_OS_Net_Initialised)
    {
        FMOD_OS_CriticalSection_Free(gResolveCrit);
        gResolveCrit = 0;

        FMOD_OS_Net_Initialised = false;
    }

    return FMOD_OK;
}


/*
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]
    Name resolution uses getaddrinfo, which blocks (Windows polls WSAAsyncGetHostByName against FMOD_Net_Timeout).
    The connect itself honours FMOD_Net_Timeout as on Windows.

	[SEE_ALSO]
*/
FMOD_RESULT FMOD_OS_Net_Connect(const char *host, const unsigned short port, void **handle)
{
    struct sockaddr_in   server;
    FMOD_RESULT          result;
    int                  sock;
    int                  rc;
    struct timeval       tv;
    fd_set               writefd;
    unsigned int         timeout = FMOD_Net_Timeout;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        return FMOD_ERR_MEMORY;
    }
    else
    {
        *handle = FMOD_NET_HANDLE(sock);
    }

    memset(&server, 0, sizeof(struct sockaddr_in));

    if (inet_pton(AF_INET, host, &server.sin_addr) != 1)
    {
        struct addrinfo  hints;
        struct addrinfo *res = 0;

        result = FMOD_OS_CriticalSection_Enter(gResolveCrit);
        if (result != FMOD_OK)
        {
            close(sock);
            return result;
        }

        memset(&hints, 0, sizeof(hints));
        hints.ai_family   = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        rc = getaddrinfo(host, 0, &hints, &res);
        if (rc != 0 || !res)
        {
            FMOD_OS_CriticalSection_Leave(gResolveCrit);
            close(sock);
            return FMOD_ERR_NET_URL;
        }

        memcpy(&server.sin_addr, &((struct sockaddr_in *)res->ai_addr)->sin_addr, sizeof(struct in_addr));
        freeaddrinfo(res);

        FMOD_OS_CriticalSection_Leave(gResolveCrit);
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    FD_ZERO(&writefd);
    FD_SET(sock, &writefd);

    tv.tv_sec  = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;

    FMOD_OS_Net_SetNonBlocking(sock, true);

    rc = connect(sock, (struct sockaddr *)&server, sizeof(server));
    if (rc == -1)
    {
        if (errno != EINPROGRESS && errno != EWOULDBLOCK)
        {
            close(sock);
            return FMOD_ERR_NET_CONNECT;
        }

        /*
            Select will block for timeout length, or return when connected
        */
        rc = select(sock + 1, 0, &writefd, 0, &tv);
        if (rc <= 0)
        {
            close(sock);
            return FMOD_ERR_NET_CONNECT;
        }
        else
        {
            int       err = 0;
            socklen_t len = sizeof(err);

            if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &len) != 0 || err != 0)
            {
                close(sock);
                return FMOD_ERR_NET_CONNECT;
            }
        }
    }

    FMOD_OS_Net_SetNonBlocking(sock, false);

    return FMOD_OK;
}


/*
	[DESCRIPTION]
    Begin listening on the designated port, non-blocking

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
FMOD_RESULT FMOD_OS_Net_Listen(const unsigned short port, void **listenhandle)
{
	int                 listensocket;
    struct sockaddr_in  address;

    // Create the socket
    listensocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listensocket == -1)
    {
        return FMOD_ERR_MEMORY;
    }

	// Set non-blocking mode
    FMOD_OS_Net_SetNonBlocking(listensocket, true);

	// Set to listen from any IP on the defined port
    memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	address.sin_addr.s_addr = htonl(INADDR_ANY);

	// Bind the address to the socket
	if (bind(listensocket, (struct sockaddr *)&address, sizeof(address)) == -1)
	{
        close(listensocket);
        return FMOD_ERR_NET_SOCKET_ERROR;
	}

	// Start listening for connections
	if (listen(listensocket, 3) == -1)
	{
        close(listensocket);
        return FMOD_ERR_NET_SOCKET_ERROR;
	}

    *listenhandle = FMOD_NET_HANDLE(listensocket);

    return FMOD_OK;
}


/*
	[DESCRIPTION]
    Accept a pending connection from the queue

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
FMOD_RESULT FMOD_OS_Net_Accept(const void *listenhandle, void **clienthandle)
{
    int listensocket = FMOD_NET_SOCKET(listenhandle);
    int clientsocket;

    if (listenhandle == (void *)-1)
    {
        return FMOD_ERR_NET_SOCKET_ERROR;
    }

    // Accept a waiting connection
    clientsocket = accept(listensocket, 0, 0);
    if (clientsocket == -1)
    {
        if (errno == EWOULDBLOCK || errno == EAGAIN)
        {
            return FMOD_ERR_NET_WOULD_BLOCK;
        }
        else
        {
            return FMOD_ERR_NET_SOCKET_ERROR;
        }
    }

    *clienthandle = FMOD_NET_HANDLE(clientsocket);

    return FMOD_OK;
}


/*
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
FMOD_RESULT FMOD_OS_Net_Close(const void *handle)
{
    int sock = FMOD_NET_SOCKET(handle);

    if (sock != -1)
    {
        close(sock);
    }

    return FMOD_OK;
}


/*
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]
    MSG_NOSIGNAL: a peer that went away must give FMOD_ERR_NET_SOCKET_ERROR, not SIGPIPE.

	[SEE_ALSO]
*/
FMOD_RESULT FMOD_OS_Net_Write(const void *handle, const char *buf, const unsigned int len, unsigned int *byteswritten)
{
    int     sock = FMOD_NET_SOCKET(handle);
    ssize_t written;
    size_t  bytestowrite = len;

    if (handle == (void *)-1)
    {
        return FMOD_ERR_NET_SOCKET_ERROR;
    }

    *byteswritten = 0;

    while (bytestowrite)
    {
        written = send(sock, buf, bytestowrite, MSG_NOSIGNAL);
        if (written == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }

            if (errno == EWOULDBLOCK || errno == EAGAIN)
            {
                return FMOD_ERR_NET_WOULD_BLOCK;
            }
            else
            {
                return FMOD_ERR_NET_SOCKET_ERROR;
            }
        }

        *byteswritten += (unsigned int)written;
        bytestowrite  -= (size_t)written;
        buf           += written;
    }

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
FMOD_RESULT FMOD_OS_Net_Read(const void *handle, char *buf, const unsigned int len, unsigned int *bytesread)
{
    int     sock = FMOD_NET_SOCKET(handle);
    ssize_t read;
    size_t  bytestoread = len;

    if (handle == (void *)-1)
    {
        return FMOD_ERR_NET_SOCKET_ERROR;
    }

    if (!buf || (len <= 0))
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    *bytesread = 0;

    while (bytestoread)
    {
        read = recv(sock, buf, bytestoread, 0);
        if (read == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }

            if (errno == EWOULDBLOCK || errno == EAGAIN)
            {
                return FMOD_ERR_NET_WOULD_BLOCK;
            }
            else
            {
                return FMOD_ERR_NET_SOCKET_ERROR;
            }
        }
        else if (read == 0)
        {
            /*
                Connection closed gracefully
            */
            return FMOD_ERR_FILE_EOF;
        }
        else
        {
            *bytesread  += (unsigned int)read;
            bytestoread -= (size_t)read;
            buf         += read;
        }
    }

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
FMOD_RESULT FMOD_OS_Net_ReadLine(const void *handle, char *buf, const unsigned int len)
{
    unsigned int pos, read;
    char c = 0;

    if (handle == (void *)-1)
    {
        return FMOD_ERR_NET_SOCKET_ERROR;
    }

    if (!buf || (len <= 0))
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    buf[0] = 0;

    pos = 0;
    while (pos < len)
    {
        FMOD_RESULT result = FMOD_OS_Net_Read(handle, &c, 1, &read);
        if (result == FMOD_ERR_NET_WOULD_BLOCK)
        {
            return FMOD_ERR_NET_WOULD_BLOCK;
        }

        if (read == 1)
        {
            if (c == '\n')
            {
                break;
            }
            else if (c != '\r')
            {
                buf[pos++] = c;
            }
        }
        else
        {
            break;
        }
    }

    if (pos >= len)
    {
        pos = len - 1;
    }

    buf[pos] = 0;

    return FMOD_OK;
}

#endif
