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
#include <common.h>
#include <CrashCatcher.h>
#include <CrashCatcherDump.h>
#include <CrashCatcherPriv.h>
#include <FileFailureInject.h>
#include <stdio.h>
#include <string.h>


typedef union
{
    CrashCatcherMemoryRegion region;
    uint32_t                 sentinel;
} RegionOrSentinel;


static FILE* openFileAndThrowOnError(const char* pLogFilename);
static void validateDumpSignature(FILE* pFile);
static void readRegisters(RegisterContext* pContext, FILE* pFile);
static void readMemoryRegions(IMemory* pMem, FILE* pFile);
static int readNextMemoryRegion(IMemory* pMem, FILE* pFile);
static int isStackOverflowSentinelInsteadOfRegionDescription(int bytesRead, const RegionOrSentinel* pSentinel);
static void createAndLoadMemoryRegion(IMemory* pMem, FILE* pFile, CrashCatcherMemoryRegion* pRegion);


__throws void CrashCatcherDump_ReadBinary(IMemory* pMem, RegisterContext* pContext, const char* pCrashDumpFilename)
{
    FILE* volatile pFile = NULL;

    __try
    {
        pFile = openFileAndThrowOnError(pCrashDumpFilename);
        validateDumpSignature(pFile);
        readRegisters(pContext, pFile);
        readMemoryRegions(pMem, pFile);
        fclose(pFile);
    }
    __catch
    {
        if (pFile != NULL);
            fclose(pFile);
        __rethrow;
    }
}

static FILE* openFileAndThrowOnError(const char* pLogFilename)
{
    FILE* pLogFile = fopen(pLogFilename, "r");
    if (!pLogFile)
        __throw(fileException);
    return pLogFile;
}

static void validateDumpSignature(FILE* pFile)
{
    static const uint8_t expectedSignature[4] = {CRASH_CATCHER_SIGNATURE_BYTE0,
                                                 CRASH_CATCHER_SIGNATURE_BYTE1,
                                                 CRASH_CATCHER_VERSION_MAJOR,
                                                 CRASH_CATCHER_VERSION_MINOR};
    uint8_t              actualSignature[4];
    int                  result = -1;

    result = fread(actualSignature, 1, sizeof(actualSignature), pFile);
    if (result == -1)
        __throw(fileFormatException);
    if (0 != memcmp(actualSignature, expectedSignature, sizeof(actualSignature)))
        __throw(fileFormatException);
}

static void readRegisters(RegisterContext* pContext, FILE* pFile)
{
    int result = fread(pContext, 1, sizeof(*pContext), pFile);
    if (result != sizeof(*pContext))
        __throw(fileFormatException);
}

static void readMemoryRegions(IMemory* pMem, FILE* pFile)
{
    int eof = FALSE;

    do
    {
        eof = readNextMemoryRegion(pMem, pFile);
    } while (!eof);
}

static int readNextMemoryRegion(IMemory* pMem, FILE* pFile)
{
    int              bytesRead;
    RegionOrSentinel regionOrSentinel;

    bytesRead = fread(&regionOrSentinel, 1, sizeof(regionOrSentinel), pFile);
    if (isStackOverflowSentinelInsteadOfRegionDescription(bytesRead, &regionOrSentinel))
        __throw(stackOverflowException);
    else if (bytesRead == 0)
        return TRUE;
    else if (bytesRead != sizeof(regionOrSentinel.region))
        __throw(fileFormatException);

    createAndLoadMemoryRegion(pMem, pFile, &regionOrSentinel.region);
    return FALSE;
}

static int isStackOverflowSentinelInsteadOfRegionDescription(int bytesRead, const RegionOrSentinel* pSentinel)
{
    return (bytesRead == sizeof(pSentinel->sentinel) && pSentinel->sentinel == STACK_SENTINEL);
}

static void createAndLoadMemoryRegion(IMemory* pMem, FILE* pFile, CrashCatcherMemoryRegion* pRegion)
{
    uint32_t bytesInRegion = pRegion->endAddress - pRegion->startAddress;
    uint32_t address = pRegion->startAddress;
    MemorySim_CreateRegion(pMem, pRegion->startAddress, bytesInRegion);
    for (uint32_t i = 0 ; i < bytesInRegion ; i++)
    {
        uint8_t byte;
        int bytesRead = fread(&byte, 1, 1, pFile);
        if (bytesRead != 1)
            __throw(fileFormatException);
        IMemory_Write8(pMem, address++, byte);
    }
}

