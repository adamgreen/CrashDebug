/*  Copyright (C) 2019  Adam Green (https://github.com/adamgreen)

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
#include <CrashDebugCommandLine.h>
#include <ElfLoad.h>
#include <FileFailureInject.h>
#include <GdbLogParser.h>
#include <MallocFailureInject.h>
#include <MemorySim.h>
#include <printfSpy.h>
#include <stdio.h>
#include <string.h>
#include <version.h>


static void displayCopyrightNotice(void)
{
    printf("CrashDebug - Cortex-M Post-Mortem Debugging Aid (" VERSION_STRING ")\n\n"
           COPYRIGHT_NOTICE
           "\n");
}

static void displayUsage(void)
{
    printf("Usage: CrashDebug (--elf elfFilename | --bin imageFilename baseAddress)\n"
           "                   --dump dumpFilename\n"
           "                  [--alias baseAddress size redirectAddress]\n"
           "Where: NOTE: The --elf and --bin options are mutually exclusive.  Use one\n"
           "             or the other but not both.\n"
           "       --elf is used to provide the filename of the .elf image containing\n"
           "         the device's FLASH contents at the time of the crash.\n"
           "       --bin is used to provide the filename of the binary image loaded into\n"
           "         the device's FLASH when the crash occurred. These binary images are\n"
           "         typically generated by running:\n"
           "           \"arm-none-eabi-objcopy -O binary input.elf output.bin\"\n"
           "         The baseAddress parameter indicates where the contents of the .bin\n"
           "         file was loaded into FLASH.  This address will typically be\n"
           "         0x00000000 unless a boot loader was in use.\n"
           "       --dump is used to provide the filename of the crash dump which\n"
           "         contains the contents of RAM and the CPU registers at the time of\n"
           "         the crash.  See the following link to learn more about generating\n"
           "         these crash dumps:\n"
           "           http://github.com/adamgreen/CrashDebug#crash-dump-generation\n"
           "       --alias is used to trap memory accesses to the region defined\n"
           "         by baseAddress/size and redirect them to the region at\n"
           "         redirectAddress. For example acesses to baseAddress will access\n"
           "         redirectAddress instead).\n");
}


typedef struct FileData
{
    char* pData;
    long  dataSize;
} FileData;

typedef enum
{
    GDB_LOG,
    CRASH_CATCHER_BIN,
    CRASH_CATCHER_HEX,
} DumpFileType;

typedef enum
{
    FIRST_PASS,
    SECOND_PASS
} ParsePass;


static void parseArguments(CrashDebugCommandLine* pThis, int volatile argc, const char** volatile argv, ParsePass pass);
static int parseArgument(CrashDebugCommandLine* pThis, int argc, const char** ppArgs, ParsePass pass);
static int hasDoubleDashPrefix(const char* pArgument);
static int parseFlagArgument(CrashDebugCommandLine* pThis, int argc, const char** ppArgs, ParsePass pass);
static int parseBinFilenameOption(CrashDebugCommandLine* pThis, int argc, const char** ppArgs, ParsePass pass);
static int parseElfFilenameOption(CrashDebugCommandLine* pThis, int argc, const char** ppArgs, ParsePass pass);
static int parseDumpFilenameOption(CrashDebugCommandLine* pThis, int argc, const char** ppArgs, ParsePass pass);
static int parseAliasOption(CrashDebugCommandLine* pThis, int argc, const char** ppArgs, ParsePass pass);
static void throwIfRequiredArgumentNotSpecified(CrashDebugCommandLine* pThis);
static void loadImageFile(CrashDebugCommandLine* pThis);
static FileData loadFileData(const char* pFilename);
static void loadBinFile(CrashDebugCommandLine* pThis, volatile FileData* pFileData);
static void loadDumpFile(CrashDebugCommandLine* pThis);
static DumpFileType getFileType(const char* pDumpFilename);
static int hasBinaryCrashCatcherSignature(const uint8_t* pHeader);
static int hasHexCrashCatcherSignature(const uint8_t* pHeader);
static uint8_t hiNibbleDigit(uint8_t byte);
static uint8_t loNibbleDigit(uint8_t byte);
static uint8_t nibbleDigit(uint8_t byte);


__throws void CrashDebugCommandLine_Init(CrashDebugCommandLine* pThis, int volatile argc, const char** volatile argv)
{
    __try
    {
        memset(pThis, 0, sizeof(*pThis));
        parseArguments(pThis, argc, argv, FIRST_PASS);
        throwIfRequiredArgumentNotSpecified(pThis);
        pThis->pMemory = MemorySim_Init();
        loadImageFile(pThis);
        loadDumpFile(pThis);
        parseArguments(pThis, argc, argv, SECOND_PASS);
    }
    __catch
    {
        displayCopyrightNotice();
        displayUsage();
        MemorySim_Uninit(pThis->pMemory);
        pThis->pMemory = NULL;
        __rethrow;
    }
}

static void parseArguments(CrashDebugCommandLine* pThis, int volatile argc, const char** volatile argv, ParsePass pass)
{
    while (argc)
    {
        int argumentsUsed = parseArgument(pThis, argc, argv, pass);
        argc -= argumentsUsed;
        argv += argumentsUsed;
    }
}

static int parseArgument(CrashDebugCommandLine* pThis, int argc, const char** ppArgs, ParsePass pass)
{
    if (hasDoubleDashPrefix(*ppArgs))
        return parseFlagArgument(pThis, argc, ppArgs, pass);
    else
        __throw(invalidArgumentException);
}

static int hasDoubleDashPrefix(const char* pArgument)
{
    return pArgument[0] == '-' && pArgument[1] == '-';
}

static int parseFlagArgument(CrashDebugCommandLine* pThis, int argc, const char** ppArgs, ParsePass pass)
{
    if (0 == strcasecmp(*ppArgs, "--bin"))
        return parseBinFilenameOption(pThis, argc - 1, &ppArgs[1], pass);
    else if (0 == strcasecmp(*ppArgs, "--elf"))
        return parseElfFilenameOption(pThis, argc - 1, &ppArgs[1], pass);
    else if (0 == strcasecmp(*ppArgs, "--dump"))
        return parseDumpFilenameOption(pThis, argc - 1, &ppArgs[1], pass);
    else if (0 == strcasecmp(*ppArgs, "--alias"))
        return parseAliasOption(pThis, argc - 1, &ppArgs[1], pass);
    else
        __throw(invalidArgumentException);
}

static int parseBinFilenameOption(CrashDebugCommandLine* pThis, int argc, const char** ppArgs, ParsePass pass)
{
    if (argc < 2)
        __throw(invalidArgumentException);

    if (pass == FIRST_PASS)
    {
        pThis->pBinFilename = ppArgs[0];
        pThis->baseAddress = strtoul(ppArgs[1], NULL, 0);
    }
    return 3;
}

static int parseElfFilenameOption(CrashDebugCommandLine* pThis, int argc, const char** ppArgs, ParsePass pass)
{
    if (argc < 1)
        __throw(invalidArgumentException);

    if (pass == FIRST_PASS)
        pThis->pElfFilename = ppArgs[0];
    return 2;
}

static int parseDumpFilenameOption(CrashDebugCommandLine* pThis, int argc, const char** ppArgs, ParsePass pass)
{
    if (argc < 1)
        __throw(invalidArgumentException);

    if (pass == FIRST_PASS)
        pThis->pDumpFilename = ppArgs[0];
    return 2;
}

static int parseAliasOption(CrashDebugCommandLine* pThis, int argc, const char** ppArgs, ParsePass pass)
{
    if (argc < 3)
        __throw(invalidArgumentException);

    if (pass == SECOND_PASS)
    {
        uint32_t baseAddress = strtoul(ppArgs[0], NULL, 0);
        uint32_t size = strtoul(ppArgs[1], NULL, 0);
        uint32_t redirectAddress = strtoul(ppArgs[2], NULL, 0);
        MemorySim_CreateAlias(pThis->pMemory, baseAddress, redirectAddress, size);
    }
    return 4;
}

static void throwIfRequiredArgumentNotSpecified(CrashDebugCommandLine* pThis)
{
    if ((!pThis->pBinFilename && !pThis->pElfFilename) || !pThis->pDumpFilename)
        __throw(invalidArgumentException);
}

static void loadImageFile(CrashDebugCommandLine* pThis)
{
    volatile FileData fileData = { NULL, 0 };

    __try
    {
        if (pThis->pElfFilename)
        {
            fileData = loadFileData(pThis->pElfFilename);
            ElfLoad_FromMemory(pThis->pMemory, fileData.pData, fileData.dataSize);
        }
        else
        {
            fileData = loadFileData(pThis->pBinFilename);
            loadBinFile(pThis, &fileData);
        }
        free(fileData.pData);
    }
    __catch
    {
        free(fileData.pData);
        __rethrow;
    }
}

static FileData loadFileData(const char* pFilename)
{
    FILE* volatile pFile = NULL;
    char* volatile pBuffer = NULL;
    volatile long  fileSize = 0;
    FileData       fileData;

    __try
    {
        size_t bytesRead = 0;

        pFile = fopen(pFilename, "rb");
        if (!pFile)
            __throw(fileException);
        fileSize = GetFileSize(pFile);

        pBuffer = malloc(fileSize);
        if (!pBuffer)
            __throw(outOfMemoryException);

        bytesRead = fread(pBuffer, 1, fileSize, pFile);
        if ((long)bytesRead != fileSize)
            __throw(fileException);

        fclose(pFile);
    }
    __catch
    {
        free(pBuffer);
        if (pFile)
            fclose(pFile);
        __rethrow;
    }

    fileData.pData = pBuffer;
    fileData.dataSize = fileSize;
    return fileData;
}

static void loadBinFile(CrashDebugCommandLine* pThis, volatile FileData* pFileData)
{
    MemorySim_CreateRegion(pThis->pMemory, pThis->baseAddress, pFileData->dataSize);
    MemorySim_LoadFromFlashImage(pThis->pMemory, pThis->baseAddress, pFileData->pData, pFileData->dataSize);
    MemorySim_MakeRegionReadOnly(pThis->pMemory, pThis->baseAddress);
}

static void loadDumpFile(CrashDebugCommandLine* pThis)
{
    switch(getFileType(pThis->pDumpFilename))
    {
    case GDB_LOG:
        GdbLogParse(pThis->pMemory, &pThis->context, pThis->pDumpFilename);
        break;
    case CRASH_CATCHER_HEX:
        CrashCatcherDump_ReadHex(pThis->pMemory, &pThis->context, pThis->pDumpFilename);
        break;
    case CRASH_CATCHER_BIN:
        CrashCatcherDump_ReadBinary(pThis->pMemory, &pThis->context, pThis->pDumpFilename);
        break;
    }
}

static DumpFileType getFileType(const char* pDumpFilename)
{
    uint8_t fileHeader[4] = { 0, 0, 0, 0 };

    FILE*   pFile = fopen(pDumpFilename, "rb");
    if (!pFile)
        __throw(fileException);
    fread(fileHeader, 1, sizeof(fileHeader), pFile);
    fclose(pFile);

    if (hasBinaryCrashCatcherSignature(fileHeader))
        return CRASH_CATCHER_BIN;
    else if (hasHexCrashCatcherSignature(fileHeader))
        return CRASH_CATCHER_HEX;
    else
        return GDB_LOG;
}

static int hasBinaryCrashCatcherSignature(const uint8_t* pHeader)
{
    return (pHeader[0] == CRASH_CATCHER_SIGNATURE_BYTE0 && pHeader[1] == CRASH_CATCHER_SIGNATURE_BYTE1);
}

static int hasHexCrashCatcherSignature(const uint8_t* pHeader)
{
    return (pHeader[0] == hiNibbleDigit(CRASH_CATCHER_SIGNATURE_BYTE0) &&
            pHeader[1] == loNibbleDigit(CRASH_CATCHER_SIGNATURE_BYTE0) &&
            pHeader[2] == hiNibbleDigit(CRASH_CATCHER_SIGNATURE_BYTE1) &&
            pHeader[3] == loNibbleDigit(CRASH_CATCHER_SIGNATURE_BYTE1));
}

static uint8_t hiNibbleDigit(uint8_t byte)
{
    return nibbleDigit(byte >> 4);
}

static uint8_t loNibbleDigit(uint8_t byte)
{
    return nibbleDigit(byte & 0xF);
}

static uint8_t nibbleDigit(uint8_t byte)
{
    static const uint8_t nibbleToHexDigit[] = "0123456789ABCDEF";
    return nibbleToHexDigit[byte];
}


void CrashDebugCommandLine_Uninit(CrashDebugCommandLine* pThis)
{
    MemorySim_Uninit(pThis->pMemory);
}
