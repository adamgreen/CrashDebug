/*  Copyright (C) 2014  Adam Green (https://github.com/adamgreen)

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*/
#include <assert.h>
#include <common.h>
#include <string.h>
#include "mockSock.h"


int             g_sockReturn;
int             g_bindReturn;
int             g_listenReturn;
int             g_acceptReturn;
int             g_selectReturn;
ssize_t         g_recvReturnValues[4];
const uint8_t*  g_pRecvCurr;
const uint8_t*  g_pRecvEnd;
int             g_sendCallToFail;
char*           g_pSendStart;
char*           g_pSendCurr;
char*           g_pSendEnd;


static int mock_socket(int domain, int type, int protocol);
static int mock_setsockopt(int socket, int level, int option_name, const void *option_value, socklen_t option_len);
static int mock_bind(int socket, const struct sockaddr *address, socklen_t address_len);
static int mock_listen(int socket, int backlog);
static int mock_accept(int socket, struct sockaddr* address, socklen_t* address_len);
static int mock_select(int nfds,
                       fd_set* readfds,
                       fd_set* writefds,
                       fd_set* errorfds,
                       struct timeval* timeout);
static ssize_t mock_recv(int socket, void *buffer, size_t length, int flags);
static ssize_t popRecvReturnValue(void);
static ssize_t mock_send(int socket, const void *buffer, size_t length, int flags);


int (*hook_socket)(int domain, int type, int protocol) = mock_socket;
int (*hook_setsockopt)(int socket,
                       int level,
                       int option_name,
                       const void *option_value,
                       socklen_t option_len) = mock_setsockopt;
int (*hook_bind)(int socket, const struct sockaddr *address, socklen_t address_len) = mock_bind;
int (*hook_listen)(int socket, int backlog) = mock_listen;
int (*hook_accept)(int socket, struct sockaddr* address, socklen_t* address_len) = mock_accept;
int (*hook_select)(int nfds,
                   fd_set* readfds,
                   fd_set* writefds,
                   fd_set* errorfds,
                   struct timeval* timeout) = mock_select;
ssize_t (*hook_recv)(int socket, void *buffer, size_t length, int flags) = mock_recv;
ssize_t (*hook_send)(int socket, const void *buffer, size_t length, int flags) = mock_send;


void mockSock_Init(size_t sendDataBufferSize)
{
    g_sockReturn = 4;
    g_bindReturn = 0;
    g_listenReturn = 0;
    g_acceptReturn = 0;
    g_selectReturn = 1;
    memset(g_recvReturnValues, 0, sizeof(g_recvReturnValues));
    g_pRecvCurr = g_pRecvEnd = NULL;
    g_sendCallToFail = 0;

    g_pSendStart = malloc(sendDataBufferSize + 1);
    g_pSendCurr = g_pSendStart;
    g_pSendEnd = g_pSendStart + sendDataBufferSize;
}


void mockSock_Uninit(void)
{
    free(g_pSendStart);
    g_pSendStart = NULL;
    g_pSendCurr = NULL;
    g_pSendEnd = NULL;
}


void mockSock_socketSetReturn(int returnValue)
{
    g_sockReturn = returnValue;
}

void mockSock_bindSetReturn(int returnValue)
{
    g_bindReturn = returnValue;
}

void mockSock_listenSetReturn(int returnValue)
{
    g_listenReturn = returnValue;
}

void mockSock_acceptSetReturn(int returnValue)
{
    g_acceptReturn = returnValue;
}

void mockSock_selectSetReturn(int returnValue)
{
    g_selectReturn = returnValue;
}

void mockSock_recvSetBuffer(const void* pBuffer, size_t bufferSize)
{
    g_pRecvCurr = pBuffer;
    g_pRecvEnd = g_pRecvCurr + bufferSize;
}

void mockSock_recvSetReturnValues(ssize_t ret1, ssize_t ret2, ssize_t ret3, ssize_t ret4)
{
    g_recvReturnValues[0] = ret1;
    g_recvReturnValues[1] = ret2;
    g_recvReturnValues[2] = ret3;
    g_recvReturnValues[3] = ret4;
}

void mockSock_sendFailIteration(int callToFail)
{
    g_sendCallToFail = callToFail;
}

const char* mockSock_sendData(void)
{
    *g_pSendCurr = '\0';
    return g_pSendStart;
}



static int mock_socket(int domain, int type, int protocol)
{
    return g_sockReturn;
}

static int mock_setsockopt(int socket, int level, int option_name, const void *option_value, socklen_t option_len)
{
    return 0;
}

static int mock_bind(int socket, const struct sockaddr *address, socklen_t address_len)
{
    return g_bindReturn;
}

static int mock_listen(int socket, int backlog)
{
    return g_listenReturn;
}

static int mock_accept(int socket, struct sockaddr* address, socklen_t* address_len)
{
    return g_acceptReturn;
}

static int mock_select(int nfds,
                       fd_set* readfds,
                       fd_set* writefds,
                       fd_set* errorfds,
                       struct timeval* timeout)
{
    return g_selectReturn;
}

static ssize_t mock_recv(int socket, void *buffer, size_t length, int flags)
{
    ssize_t bytesLeft = g_pRecvEnd - g_pRecvCurr;
    ssize_t size = popRecvReturnValue();

    assert (size <= bytesLeft && size <= (ssize_t)length);
    if (size > 0)
    {
        memcpy(buffer, g_pRecvCurr, size);
        g_pRecvCurr += size;
    }
    return size;
}

static ssize_t popRecvReturnValue(void)
{
    ssize_t retVal = g_recvReturnValues[0];
    memmove(g_recvReturnValues, g_recvReturnValues + 1, sizeof(g_recvReturnValues) - sizeof(g_recvReturnValues[0]));
    g_recvReturnValues[ARRAY_SIZE(g_recvReturnValues) - 1] = 0;

    return retVal;
}

static ssize_t mock_send(int socket, const void *buffer, size_t length, int flags)
{
    size_t bytesLeft = g_pSendEnd - g_pSendCurr;
    size_t bytesToCopy = length < bytesLeft ? length : bytesLeft;

    if (g_sendCallToFail)
    {
        if (--g_sendCallToFail == 0)
            return -1;
    }
    memcpy(g_pSendCurr, buffer, bytesToCopy);
    g_pSendCurr += bytesToCopy;

    return (ssize_t)length;
}
