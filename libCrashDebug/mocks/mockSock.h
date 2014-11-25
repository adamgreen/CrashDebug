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
#ifndef _MOCK_SOCK_H_
#define _MOCK_SOCK_H_


#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>


void mockSock_Init(size_t sendDataBufferSize);
void mockSock_Uninit(void);

void        mockSock_socketSetReturn(int returnValue);
void        mockSock_bindSetReturn(int returnValue);
void        mockSock_listenSetReturn(int returnValue);
void        mockSock_acceptSetReturn(int returnValue);
void        mockSock_selectSetReturn(int returnValue);
void        mockSock_recvSetBuffer(const void* pBuffer, size_t bufferSize);
void        mockSock_recvSetReturnValues(ssize_t ret1, ssize_t ret2, ssize_t ret3, ssize_t ret4);
void        mockSock_sendFailIteration(int callToFail);
const char* mockSock_sendData(void);

/* Hooks for socket related APIs */
extern int (*hook_socket)(int domain, int type, int protocol);
extern int (*hook_setsockopt)(int socket, int level, int option_name, const void *option_value, socklen_t option_len);
extern int (*hook_bind)(int socket, const struct sockaddr *address, socklen_t address_len);
extern int (*hook_listen)(int socket, int backlog);
extern int (*hook_accept)(int socket, struct sockaddr* address, socklen_t* address_len);
extern int (*hook_select)(int nfds,
                          fd_set* readfds,
                          fd_set* writefds,
                          fd_set* errorfds,
                          struct timeval* timeout);
extern int (*hook_close)(int fildes);
extern ssize_t (*hook_recv)(int socket, void *buffer, size_t length, int flags);
extern ssize_t (*hook_send)(int socket, const void *buffer, size_t length, int flags);


/* Redirect calls to socket APIs to the mocks instead */
#define socket      hook_socket
#define setsockopt  hook_setsockopt
#define bind        hook_bind
#define listen      hook_listen
#define accept      hook_accept
#define select      hook_select
#define close       hook_close
#define recv        hook_recv
#define send        hook_send


#endif /* _MOCK_SOCK_H_ */
