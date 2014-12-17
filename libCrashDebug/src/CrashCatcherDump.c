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

typedef struct Object
{
    int              (*read)(struct Object* pObject, void* pBuffer, size_t bytesToRead);
    IMemory*         pMem;
    RegisterContext* pContext;
    FILE*            pFile;
} Object;


static int binaryRead(Object* pObject, void* pBuffer, size_t bytesToRead);
static void initObject(Object* pObject,
                       IMemory* pMem,
                       RegisterContext* pContext,
                       const char* pCrashDumpFilename,
                       int (*read)(struct Object*, void*, size_t));
static FILE* openFileAndThrowOnError(const char* pLogFilename);
static void validateDumpSignature(Object* pObject);
static void readRegisters(Object* pObject);
static void readMemoryRegions(Object* pObject);
static int readNextMemoryRegion(Object* pObject);
static int isStackOverflowSentinelInsteadOfRegionDescription(int bytesRead, const RegionOrSentinel* pSentinel);
static void createAndLoadMemoryRegion(Object* pObject, CrashCatcherMemoryRegion* pRegion);
static void destructObject(Object* pObject);
static int hexRead(Object* pObject, void* pBuffer, size_t bytesToRead);
static int readNextCharacterSkippingNewLines(Object* pObject, char* pHexDigit);
static uint8_t nibbleDigitToVal(char hexDigit);


__throws void CrashCatcherDump_ReadBinary(IMemory* pMem, RegisterContext* pContext, const char* pCrashDumpFilename)
{
    Object         object;

    __try
    {
        initObject(&object, pMem, pContext, pCrashDumpFilename, binaryRead);
        validateDumpSignature(&object);
        readRegisters(&object);
        readMemoryRegions(&object);
        destructObject(&object);
    }
    __catch
    {
        destructObject(&object);
        __rethrow;
    }
}

static int binaryRead(Object* pObject, void* pBuffer, size_t bytesToRead)
{
    return fread(pBuffer, 1, bytesToRead, pObject->pFile);
}

static void initObject(Object* pObject,
                       IMemory* pMem,
                       RegisterContext* pContext,
                       const char* pCrashDumpFilename,
                       int (*read)(struct Object*, void*, size_t))
{
    memset(pObject, 0, sizeof(*pObject));
    pObject->read = read;
    pObject->pMem = pMem;
    pObject->pContext = pContext;
    pObject->pFile = openFileAndThrowOnError(pCrashDumpFilename);
}

static FILE* openFileAndThrowOnError(const char* pLogFilename)
{
    FILE* pLogFile = fopen(pLogFilename, "r");
    if (!pLogFile)
        __throw(fileException);
    return pLogFile;
}

static void validateDumpSignature(Object* pObject)
{
    static const uint8_t expectedSignature[4] = {CRASH_CATCHER_SIGNATURE_BYTE0,
                                                 CRASH_CATCHER_SIGNATURE_BYTE1,
                                                 CRASH_CATCHER_VERSION_MAJOR,
                                                 CRASH_CATCHER_VERSION_MINOR};
    uint8_t              actualSignature[4];
    int                  result = -1;

    result = pObject->read(pObject, actualSignature, sizeof(actualSignature));
    if (result == -1)
        __throw(fileFormatException);
    if (0 != memcmp(actualSignature, expectedSignature, sizeof(actualSignature)))
        __throw(fileFormatException);
}

static void readRegisters(Object* pObject)
{
    int result = pObject->read(pObject, pObject->pContext, sizeof(*pObject->pContext));
    if (result != sizeof(*pObject->pContext))
        __throw(fileFormatException);
}

static void readMemoryRegions(Object* pObject)
{
    int eof = FALSE;

    do
    {
        eof = readNextMemoryRegion(pObject);
    } while (!eof);
}

static int readNextMemoryRegion(Object* pObject)
{
    int              bytesRead;
    RegionOrSentinel regionOrSentinel;

    bytesRead = pObject->read(pObject, &regionOrSentinel, sizeof(regionOrSentinel));
    if (isStackOverflowSentinelInsteadOfRegionDescription(bytesRead, &regionOrSentinel))
        __throw(stackOverflowException);
    else if (bytesRead == 0)
        return TRUE;
    else if (bytesRead != sizeof(regionOrSentinel.region))
        __throw(fileFormatException);

    createAndLoadMemoryRegion(pObject, &regionOrSentinel.region);
    return FALSE;
}

static int isStackOverflowSentinelInsteadOfRegionDescription(int bytesRead, const RegionOrSentinel* pSentinel)
{
    return (bytesRead == sizeof(pSentinel->sentinel) && pSentinel->sentinel == STACK_SENTINEL);
}

static void createAndLoadMemoryRegion(Object* pObject, CrashCatcherMemoryRegion* pRegion)
{
    uint32_t bytesInRegion = pRegion->endAddress - pRegion->startAddress;
    uint32_t address = pRegion->startAddress;
    MemorySim_CreateRegion(pObject->pMem, pRegion->startAddress, bytesInRegion);
    for (uint32_t i = 0 ; i < bytesInRegion ; i++)
    {
        uint8_t byte;
        int bytesRead = pObject->read(pObject, &byte, 1);
        if (bytesRead != 1)
            __throw(fileFormatException);
        IMemory_Write8(pObject->pMem, address++, byte);
    }
}

static void destructObject(Object* pObject)
{
    if (pObject && pObject->pFile)
    {
        fclose(pObject->pFile);
        pObject->pFile = NULL;
    }
}



__throws void CrashCatcherDump_ReadHex(IMemory* pMem, RegisterContext* pContext, const char* pCrashDumpFilename)
{
    Object         object;

    __try
    {
        initObject(&object, pMem, pContext, pCrashDumpFilename, hexRead);
        validateDumpSignature(&object);
        readRegisters(&object);
        readMemoryRegions(&object);
        destructObject(&object);
    }
    __catch
    {
        destructObject(&object);
        __rethrow;
    }
}

static int hexRead(Object* pObject, void* pBuffer, size_t bytesToRead)
{
    int      bytesRead = 0;
    uint8_t* pCurr = pBuffer;

    while (bytesToRead--)
    {
        char hiNibble;
        char loNibble;
        int  result;

        result = readNextCharacterSkippingNewLines(pObject, &hiNibble);
        if (result != 1)
            break;
        result = readNextCharacterSkippingNewLines(pObject, &loNibble);
        if (result != 1)
            break;
        *pCurr++ = (nibbleDigitToVal(hiNibble) << 4) | nibbleDigitToVal(loNibble);
        bytesRead++;
    }
    return bytesRead;
}

static int readNextCharacterSkippingNewLines(Object* pObject, char* pHexDigit)
{
    char curr;
    int  result;

    do
    {
        result = fread(&curr, 1, 1, pObject->pFile);
    } while (result == 1 && (curr == '\r' || curr == '\n'));

    if (result == 1)
        *pHexDigit = curr;
    return result;
}

static uint8_t nibbleDigitToVal(char hexDigit)
{
    if (hexDigit >= '0' && hexDigit <= '9')
        return hexDigit - '0';
    if (hexDigit >= 'a' && hexDigit <= 'f')
        return hexDigit - 'a' + 10;
    if (hexDigit >= 'A' && hexDigit <= 'F')
        return hexDigit - 'A' + 10;

    __throw(fileFormatException);
    return 0;
}
