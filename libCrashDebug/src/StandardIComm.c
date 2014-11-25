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
#include <mockFileIo.h>
#include <mockSock.h>
#include <netdb.h>
#include <StandardIComm.h>
#include <string.h>


/* Implementation of IComm interface. */
typedef struct StandardIComm StandardIComm;

static int  hasReceiveData(IComm* pComm);
static int  receiveChar(IComm* pComm);
static void sendChar(IComm* pComm, int character);
static int  shouldStopRun(IComm* pComm);
static int  isGdbConnected(IComm* pComm);

static ICommVTable g_icommVTable = {hasReceiveData, receiveChar, sendChar, shouldStopRun, isGdbConnected};

static struct StandardIComm
{
    ICommVTable* pVTable;
} g_comm = {&g_icommVTable};

static int stdinHasDataToRead();


__throws IComm* StandardIComm_Init()
{
    StandardIComm* pThis = &g_comm;
    return (IComm*)pThis;
}


void StandardIComm_Uninit(IComm* pComm)
{
}



/* IComm Interface Implementation. */
static int hasReceiveData(IComm* pComm)
{
    int            hasData = FALSE;

    __try
    {
        hasData = stdinHasDataToRead();
    }
    __catch
    {
        return FALSE;
    }
    return hasData;
}

static int stdinHasDataToRead()
{
    int            result = -1;
    struct timeval zeroTimeout = {0, 0};
    fd_set         readSet;

    FD_ZERO(&readSet);
    FD_SET(STDIN_FILENO, &readSet);
    result = select(STDIN_FILENO + 1, &readSet, NULL, NULL, &zeroTimeout);
    if (result == -1)
        __throw(fileException);
    return result;
}

static int receiveChar(IComm* pComm)
{
    ssize_t result = -1;
    char    c = 0;

    result = read(STDIN_FILENO, &c, sizeof(c));
    if (result == -1)
    {
        __throw(fileException);
    }
    return c;
}

static void sendChar(IComm* pComm, int character)
{
    ssize_t result = -1;
    char    c = (char)character;

    result = write(STDOUT_FILENO, &c, sizeof(c));
    if (result == -1)
        __throw(fileException);
}

static int shouldStopRun(IComm* pComm)
{
    return FALSE;
}

static int  isGdbConnected(IComm* pComm)
{
    return TRUE;
}
