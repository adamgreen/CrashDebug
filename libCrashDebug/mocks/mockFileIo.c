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
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>

static int     mock_open(const char *path, int oflag, ...);
static ssize_t mock_read(int fildes, void *buf, size_t nbyte);
static ssize_t mock_write(int fildes, const void *buf, size_t nbyte);
static off_t   mock_lseek(int fildes, off_t offset, int whence);
static int     mock_close(int fildes);
static int     mock_unlink(const char *path);
static int     mock_rename(const char *oldPath, const char *newPath);
static int     mock_fstat(int fildes, struct stat *buf);
static int     mock_stat(const char* path, struct stat* buf);
static FILE*   mock_popen(const char *command, const char *mode);
static int     mock_pclose(FILE *stream);
static int     mock_feof(FILE *stream);
static char*   mock_fgets(char * str, int size, FILE * stream);


int     (*hook_open)(const char *path, int oflag, ...) = mock_open;
ssize_t (*hook_read)(int fildes, void *buf, size_t nbyte) = mock_read;
ssize_t (*hook_write)(int fildes, const void *buf, size_t nbyte) = mock_write;
off_t   (*hook_lseek)(int fildes, off_t offset, int whence) = mock_lseek;
int     (*hook_close)(int fildes) = mock_close;
int     (*hook_unlink)(const char *path) = mock_unlink;
int     (*hook_rename)(const char *oldPath, const char *newPath) = mock_rename;
int     (*hook_fstat)(int fildes, struct stat *buf) = mock_fstat;
int     (*hook_stat)(const char* path, struct stat* buf) = mock_stat;
FILE*   (*hook_popen)(const char *command, const char *mode) = mock_popen;
int     (*hook_pclose)(FILE *stream) = mock_pclose;
int     (*hook_feof)(FILE *stream) = mock_feof;
char*   (*hook_fgets)(char * str, int size, FILE * stream) = mock_fgets;


typedef struct CallResult
{
    int result;
    int error;
} CallResult;

#define CALL_GLOBAL(X) \
   CallResult g_##X;

#define CALL_MOCK(X) \
    if (g_##X.result < 0) \
        errno = g_##X.error; \
    return g_##X.result;

#define CALL_SET(X) \
    g_##X.result = result; \
    g_##X.error = err;


static       int      g_readResult;
static       int      g_readError;
static const uint8_t* g_pReadStart;
static const uint8_t* g_pReadEnd;
static const uint8_t* g_pReadCurr;
static       int      g_writeResult;
static       int      g_writeError;
static char*          g_pStdOutStart;
static char*          g_pStdOutEnd;
static char*          g_pStdOutCurr;
static char*          g_pStdErrStart;
static char*          g_pStdErrEnd;
static char*          g_pStdErrCurr;
static char*          g_pRegularStart;
static char*          g_pRegularEnd;
static char*          g_pRegularCurr;
static struct stat    g_fstatStructure;
static struct stat    g_statStructure;
static FILE*          g_popenResult;
static int            g_feofResult;
static const char**   g_ppFgetsEnd;
static const char**   g_ppFgetsCurr;
CALL_GLOBAL(open)
CALL_GLOBAL(lseek)
CALL_GLOBAL(close)
CALL_GLOBAL(unlink)
CALL_GLOBAL(rename)
CALL_GLOBAL(fstat)
CALL_GLOBAL(stat)


static int mock_open(const char *path, int oflag, ...)
{
    CALL_MOCK(open)
}

static ssize_t mock_read(int fildes, void *buf, size_t nbyte)
{
    int bytesLeft = g_pReadEnd - g_pReadCurr;
    if (g_readResult)
    {
        errno = g_readError;
        return g_readResult;
    }

    if (nbyte > (size_t)bytesLeft)
        nbyte = bytesLeft;
    memcpy(buf, g_pReadCurr, nbyte);
    g_pReadCurr += nbyte;
    return nbyte;
}


static ssize_t mock_write(int fildes, const void *buf, size_t nbyte)
{
    char*  pBuffer;
    char** ppCurr;
    int    bytesLeft;

    if (g_writeResult)
    {
        errno = g_writeError;
        return g_writeResult;
    }

    switch (fildes)
    {
    case STDOUT_FILENO:
        pBuffer = g_pStdOutCurr;
        ppCurr = &g_pStdOutCurr;
        bytesLeft = g_pStdOutEnd - g_pStdOutCurr;
        break;
    case STDERR_FILENO:
        pBuffer = g_pStdErrCurr;
        ppCurr = &g_pStdErrCurr;
        bytesLeft = g_pStdErrEnd - g_pStdErrCurr;
        break;
    default:
        pBuffer = g_pRegularCurr;
        ppCurr = &g_pRegularCurr;
        bytesLeft = g_pRegularEnd - g_pRegularCurr;
        break;
    }

    nbyte = nbyte > (size_t)bytesLeft ? bytesLeft : nbyte;
    memcpy(pBuffer, buf, nbyte);
    (*ppCurr) += nbyte;
    return nbyte;
}

static off_t mock_lseek(int fildes, off_t offset, int whence)
{
    CALL_MOCK(lseek)
}

static int mock_close(int fildes)
{
    CALL_MOCK(close)
}

static int mock_unlink(const char *path)
{
    CALL_MOCK(unlink)
}

static int mock_rename(const char *oldPath, const char *newPath)
{
    CALL_MOCK(rename)
}

static int mock_fstat(int fildes, struct stat *buf)
{
    if (g_fstat.result < 0)
        errno = g_fstat.error;
    if (g_fstat.result == 0)
        *buf = g_fstatStructure;
    return g_fstat.result;
}

static int mock_stat(const char* path, struct stat* buf)
{
    if (g_stat.result < 0)
        errno = g_stat.error;
    if (g_stat.result == 0)
        *buf = g_statStructure;
    return g_stat.result;
}

static FILE* mock_popen(const char *command, const char *mode)
{
    return g_popenResult;
}

static int mock_pclose(FILE *stream)
{
    return 0;
}

static int mock_feof(FILE *stream)
{
    return g_feofResult;
}

static char* mock_fgets(char * str, int size, FILE * stream)
{
    if (g_ppFgetsCurr >= g_ppFgetsEnd)
        return NULL;

    strncpy(str, *g_ppFgetsCurr++, size);
    str[size-1] = '\0';
    return str;
}


void mockFileIo_SetOpenToFail(int result, int err)
{
    CALL_SET(open);
}

void mockFileIo_SetReadData(const void* pvData, size_t dataSize)
{
    g_pReadStart = g_pReadCurr = (uint8_t*)pvData;
    g_pReadEnd = g_pReadStart + dataSize;
}

void mockFileIo_SetReadToFail(int result, int err)
{
    g_readResult = result;
    g_readError = err;
}

void mockFileIo_SetWriteToFail(int result, int err)
{
    g_writeResult = result;
    g_writeError = err;
}

void mockFileIo_CreateWriteBuffer(size_t bufferSize)
{
    g_pStdOutStart = malloc(bufferSize + 1);
    g_pStdOutEnd = g_pStdOutStart + bufferSize;
    g_pStdOutCurr = g_pStdOutStart;
    g_pStdErrStart = malloc(bufferSize + 1);
    g_pStdErrEnd = g_pStdErrStart + bufferSize;
    g_pStdErrCurr = g_pStdErrStart;
    g_pRegularStart = malloc(bufferSize + 1);
    g_pRegularEnd = g_pRegularStart + bufferSize;
    g_pRegularCurr = g_pRegularStart;
}

const char* mockFileIo_GetStdOutData(void)
{
    *g_pStdOutCurr = '\0';
    return g_pStdOutStart;
}

const char* mockFileIo_GetStdErrData(void)
{
    *g_pStdErrCurr = '\0';
    return g_pStdErrStart;
}

const char* mockFileIo_GetRegularFileData(void)
{
    *g_pRegularCurr = '\0';
    return g_pRegularStart;
}

void mockFileIo_SetLSeekToFail(int result, int err)
{
    CALL_SET(lseek);
}

void mockFileIo_SetCloseToFail(int result, int err)
{
    CALL_SET(close);
}

void mockFileIo_SetUnlinkToFail(int result, int err)
{
    CALL_SET(unlink);
}

void mockFileIo_SetRenameToFail(int result, int err)
{
    CALL_SET(rename);
}

void mockFileIo_SetFStatCallResults(int result, int err, struct stat* pStat)
{
    g_fstat.result = result;
    g_fstat.error = err;
    if (pStat)
        g_fstatStructure = *pStat;
}

void mockFileIo_SetStatCallResults(int result, int err, struct stat* pStat)
{
    g_stat.result = result;
    g_stat.error = err;
    if (pStat)
        g_statStructure = *pStat;
}

void mockFileIo_SetPOpenCallResult(FILE* pResult)
{
    g_popenResult = pResult;
}

void mockFileIo_SetFEOFCallResult(int result)
{
    g_feofResult = result;
}

void mockFileIo_SetFgetsData(const char** ppLines, size_t lineCount)
{
    g_ppFgetsCurr = ppLines;
    g_ppFgetsEnd = ppLines + lineCount;
}

void mockFileIo_Uninit(void)
{
    g_pReadStart = g_pReadCurr = g_pReadEnd = NULL;
    free(g_pStdOutStart);
    g_pStdOutStart = g_pStdOutCurr = g_pStdOutEnd = NULL;
    free(g_pStdErrStart);
    g_pStdErrStart = g_pStdErrCurr = g_pStdErrEnd = NULL;
    free(g_pRegularStart);
    g_pRegularStart = g_pRegularCurr = g_pRegularEnd = NULL;
    g_readResult = 0;
    g_writeResult = 0;
    mockFileIo_SetCloseToFail(0, 0);
}
