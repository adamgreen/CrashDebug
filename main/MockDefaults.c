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
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>


/* Not using my test mocks in production so point hooks to Standard CRT functions. */
void*  (*hook_malloc)(size_t size) = malloc;
void*  (*hook_realloc)(void* ptr, size_t size) = realloc;
void   (*hook_free)(void* ptr) = free;
int    (*hook_printf)(const char* pFormat, ...) = printf;
int    (*hook_fprintf)(FILE* pFile, const char* pFormat, ...) = fprintf;
FILE*  (*hook_fopen)(const char* filename, const char* mode) = fopen;
int    (*hook_fclose)(FILE* stream) = fclose;
int    (*hook_fseek)(FILE* stream, long offset, int whence) = fseek;
long   (*hook_ftell)(FILE* stream) = ftell;
size_t (*hook_fwrite)(const void* ptr, size_t size, size_t nitems, FILE* stream) = fwrite;
size_t (*hook_fread)(void* ptr, size_t size, size_t nitems, FILE* stream) = fread;

int     (*hook_open)(const char *path, int oflag, ...) = open;
ssize_t (*hook_read)(int fildes, void *buf, size_t nbyte) = read;
ssize_t (*hook_write)(int fildes, const void *buf, size_t nbyte) = write;
off_t   (*hook_lseek)(int fildes, off_t offset, int whence) = lseek;
int     (*hook_close)(int fildes) = close;
int     (*hook_unlink)(const char *path) = unlink;
int     (*hook_rename)(const char *oldPath, const char *newPath) = rename;
int     (*hook_fstat)(int fildes, struct stat *buf) = fstat;
int     (*hook_stat)(const char* path, struct stat* buf) = stat;
int     (*hook_feof)(FILE *stream) = feof;
char*   (*hook_fgets)(char * str, int size, FILE * stream) = fgets;
FILE*   (*hook_popen)(const char *command, const char *mode) = popen;
int     (*hook_pclose)(FILE *stream) = pclose;

int (*hook_socket)(int domain, int type, int protocol) = socket;
int (*hook_setsockopt)(int socket,
                       int level,
                       int option_name,
                       const void *option_value,
                       socklen_t option_len) = setsockopt;
int (*hook_bind)(int socket, const struct sockaddr *address, socklen_t address_len) = bind;
int (*hook_listen)(int socket, int backlog) = listen;
int (*hook_accept)(int socket, struct sockaddr* address, socklen_t* address_len) = accept;
int (*hook_select)(int nfds,
                   fd_set* readfds,
                   fd_set* writefds,
                   fd_set* errorfds,
                   struct timeval* timeout) = select;
ssize_t (*hook_recv)(int socket, void *buffer, size_t length, int flags) = recv;
ssize_t (*hook_send)(int socket, const void *buffer, size_t length, int flags) = send;
