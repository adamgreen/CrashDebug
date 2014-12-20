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

// Include headers from C modules under test.
extern "C"
{
    #include <common.h>
    #include <CrashCatcherDump.h>
    #include <CrashCatcher.h>
    #include <FileFailureInject.h>
    #include <MallocFailureInject.h>
}

#include <string.h>

// Include C++ headers for test harness.
#include "CppUTest/TestHarness.h"


static const char* g_testFilename = "CrashCatcherHexDumpTest.dmp";


typedef struct
{
    uint8_t  signature[4];
    uint32_t R[TOTAL_REG_COUNT];
    uint32_t exceptionPSR;
} DumpFileTop;


TEST_GROUP(CrashCatcherHexDump)
{
    IMemory*        m_pMem;
    RegisterContext m_actualRegisters;
    RegisterContext m_expectedRegisters;
    DumpFileTop     m_fileTop;

    void setup()
    {
        m_pMem = MemorySim_Init();
        for (size_t i = 0 ; i < ARRAY_SIZE(m_actualRegisters.R) ; i++)
        {
            m_actualRegisters.R[i] = 0xDEADBEEF;
            m_expectedRegisters.R[i] = 0xDEADBEEF;
        }
        initFileTop();
    }

    void initFileTop()
    {
        m_fileTop.signature[0] = CRASH_CATCHER_SIGNATURE_BYTE0;
        m_fileTop.signature[1] = CRASH_CATCHER_SIGNATURE_BYTE1;
        m_fileTop.signature[2] = CRASH_CATCHER_VERSION_MAJOR;
        m_fileTop.signature[3] = CRASH_CATCHER_VERSION_MINOR;
        for (size_t i = 0 ; i < ARRAY_SIZE(m_fileTop.R) ; i++)
            m_fileTop.R[i] = 0xDEADBEEF;
    }

    void teardown()
    {
        CHECK_EQUAL(noException, getExceptionCode());
        checkRegisters();
        fopenRestore();
        freadRestore();
        remove(g_testFilename);
        MemorySim_Uninit(m_pMem);
        MallocFailureInject_Restore();
        clearExceptionCode();
    }

    void checkRegisters()
    {
        for (size_t i = 0 ; i < ARRAY_SIZE(m_actualRegisters.R) ; i++)
        {
            CHECK_EQUAL(m_expectedRegisters.R[i], m_actualRegisters.R[i]);
        }
        CHECK_EQUAL(m_expectedRegisters.exceptionPSR, m_actualRegisters.exceptionPSR);
    }

    void createTestDumpFile(const void* pData, size_t dataSize)
    {
        static const char nibbleToHex[] = "0123456789ABCDEF";
        const uint8_t* pCurr = (uint8_t*)pData;
        FILE* pFile = fopen(g_testFilename, "w");
        while (dataSize-- > 0)
        {
            uint8_t currByte = *pCurr++;
            char    nibble = nibbleToHex[currByte >> 4];
            fwrite(&nibble, 1, 1, pFile);
            nibble = nibbleToHex[currByte & 0xF];
            fwrite(&nibble, 1, 1, pFile);
        }
        fclose(pFile);
    }

    void validateNoMemoryRegionsCreated()
    {
        static const char* xmlForEmptyRegions = "<?xml version=\"1.0\"?>"
                                                "<!DOCTYPE memory-map PUBLIC \"+//IDN gnu.org//DTD GDB Memory Map V1.0//EN\" \"http://sourceware.org/gdb/gdb-memory-map.dtd\">"
                                                "<memory-map>"
                                                "</memory-map>";
        const char* pMemoryLayout = MemorySim_GetMemoryMapXML(m_pMem);
        STRCMP_EQUAL(xmlForEmptyRegions, pMemoryLayout);
    }
};


TEST(CrashCatcherHexDump, InvalidLogFilename_ShouldThrowFileException)
{
    fopenSetReturn(NULL);
        __try_and_catch( CrashCatcherDump_ReadHex(m_pMem, &m_actualRegisters, "invalidDumpFilename.dmp") );
    CHECK_EQUAL(fileException, getExceptionCode());
    clearExceptionCode();
}

TEST(CrashCatcherHexDump, InvalidSignature_ShouldThrowFileFormatException)
{
    m_fileTop.signature[3] += 1;
    createTestDumpFile(&m_fileTop, sizeof(m_fileTop));
        __try_and_catch( CrashCatcherDump_ReadHex(m_pMem, &m_actualRegisters, g_testFilename) );
    CHECK_EQUAL(fileFormatException, getExceptionCode());
    clearExceptionCode();
}

TEST(CrashCatcherHexDump, FailSignatureRead_ShouldThrowFileFormatException)
{
    createTestDumpFile(&m_fileTop, sizeof(m_fileTop));
    freadFail(-1);
        __try_and_catch( CrashCatcherDump_ReadHex(m_pMem, &m_actualRegisters, g_testFilename) );
    CHECK_EQUAL(fileFormatException, getExceptionCode());
    clearExceptionCode();
}

TEST(CrashCatcherHexDump, FileTooShortForRegisters_ShouldThrowFileFormatException)
{
    createTestDumpFile(&m_fileTop, sizeof(m_fileTop) - 1);
        __try_and_catch( CrashCatcherDump_ReadHex(m_pMem, &m_actualRegisters, g_testFilename) );
    CHECK_EQUAL(fileFormatException, getExceptionCode());
    clearExceptionCode();
}

TEST(CrashCatcherHexDump, DumpContainingRegistersOnly_VerifyRegistersReadAndEmptyMemoryRegions)
{
    for (uint32_t i = 0 ; i < 16 ; i++)
        m_fileTop.R[i] = i * 0x11111111;
    m_fileTop.R[XPSR] = 0xF00DF00D;
    m_fileTop.exceptionPSR =  0xBAADFEED;
    createTestDumpFile(&m_fileTop, sizeof(m_fileTop));
        CrashCatcherDump_ReadHex(m_pMem, &m_actualRegisters, g_testFilename);
    validateNoMemoryRegionsCreated();
    m_expectedRegisters.R[R0]            = 0x0;
    m_expectedRegisters.R[R1]            = 0x11111111;
    m_expectedRegisters.R[R2]            = 0x22222222;
    m_expectedRegisters.R[R3]            = 0x33333333;
    m_expectedRegisters.R[R4]            = 0x44444444;
    m_expectedRegisters.R[R5]            = 0x55555555;
    m_expectedRegisters.R[R6]            = 0x66666666;
    m_expectedRegisters.R[R7]            = 0x77777777;
    m_expectedRegisters.R[R8]            = 0x88888888;
    m_expectedRegisters.R[R9]            = 0x99999999;
    m_expectedRegisters.R[R10]           = 0xAAAAAAAA;
    m_expectedRegisters.R[R11]           = 0xBBBBBBBB;
    m_expectedRegisters.R[R12]           = 0xCCCCCCCC;
    m_expectedRegisters.R[SP]            = 0xDDDDDDDD;
    m_expectedRegisters.R[LR]            = 0xEEEEEEEE;
    m_expectedRegisters.R[PC]            = 0xFFFFFFFF;
    m_expectedRegisters.R[XPSR]          = 0xF00DF00D;
    m_expectedRegisters.exceptionPSR     = 0xBAADFEED;
}

TEST(CrashCatcherHexDump, DumpContainingOneMemoryRegionOf1Word_VerifyMemoryContents)
{
    struct FileData
    {
        DumpFileTop                  fileTop;
        CrashCatcherMemoryRegionInfo region1;
        uint32_t                     region1Data[1];
    } fileData;
    memcpy(&fileData.fileTop, &m_fileTop, sizeof(m_fileTop));
    fileData.region1.startAddress = 0x10000000;
    fileData.region1.endAddress = 0x10000004;
    fileData.region1Data[0] = 0x11111111;
    createTestDumpFile(&fileData, sizeof(fileData));
        CrashCatcherDump_ReadHex(m_pMem, &m_actualRegisters, g_testFilename);
    static const char* xmlForMemory = "<?xml version=\"1.0\"?>"
                                      "<!DOCTYPE memory-map PUBLIC \"+//IDN gnu.org//DTD GDB Memory Map V1.0//EN\" \"http://sourceware.org/gdb/gdb-memory-map.dtd\">"
                                      "<memory-map>"
                                      "<memory type=\"ram\" start=\"0x10000000\" length=\"0x4\"></memory>"
                                      "</memory-map>";
    const char* pMemoryLayout = MemorySim_GetMemoryMapXML(m_pMem);
    STRCMP_EQUAL(xmlForMemory, pMemoryLayout);
    CHECK_EQUAL(0x11111111, IMemory_Read32(m_pMem, 0x10000000));
}

TEST(CrashCatcherHexDump, FailAllocationForRegion_VerifyEmptyMemoryAndExceptionThrown)
{
    struct FileData
    {
        DumpFileTop                  fileTop;
        CrashCatcherMemoryRegionInfo region1;
        uint32_t                     region1Data[1];
    } fileData;
    memcpy(&fileData.fileTop, &m_fileTop, sizeof(m_fileTop));
    fileData.region1.startAddress = 0x10000000;
    fileData.region1.endAddress = 0x10000004;
    fileData.region1Data[0] = 0x11111111;
    createTestDumpFile(&fileData, sizeof(fileData));
    MallocFailureInject_FailAllocation(1);
        __try_and_catch( CrashCatcherDump_ReadHex(m_pMem, &m_actualRegisters, g_testFilename) );
    CHECK_EQUAL(outOfMemoryException, getExceptionCode());
    clearExceptionCode();
    validateNoMemoryRegionsCreated();
}

TEST(CrashCatcherHexDump, DumpContainingTruncatedMemoryRegionDescription_ExceptionShouldBeThrownAndNoMemoryRegionsCreated)
{
    struct FileData
    {
        DumpFileTop                  fileTop;
        CrashCatcherMemoryRegionInfo region1;
        uint32_t                     region1Data[1];
    } fileData;
    memcpy(&fileData.fileTop, &m_fileTop, sizeof(m_fileTop));
    fileData.region1.startAddress = 0x10000000;
    fileData.region1.endAddress = 0x10000004;
    fileData.region1Data[0] = 0x11111111;
    createTestDumpFile(&fileData, sizeof(fileData) - sizeof(uint32_t) - 1);
        __try_and_catch( CrashCatcherDump_ReadHex(m_pMem, &m_actualRegisters, g_testFilename) );
    CHECK_EQUAL(fileFormatException, getExceptionCode());
    clearExceptionCode();
    validateNoMemoryRegionsCreated();
}

TEST(CrashCatcherHexDump, DumpContainingTruncatedMemoryRegionData_ExceptionShouldBeThrown_PartiallyCompletedMemoryRegion)
{
    struct FileData
    {
        DumpFileTop                  fileTop;
        CrashCatcherMemoryRegionInfo region1;
        uint32_t                     region1Data[1];
    } fileData;
    memcpy(&fileData.fileTop, &m_fileTop, sizeof(m_fileTop));
    fileData.region1.startAddress = 0x10000000;
    fileData.region1.endAddress = 0x10000004;
    fileData.region1Data[0] = 0x11111111;
    createTestDumpFile(&fileData, sizeof(fileData) - 1);
        __try_and_catch( CrashCatcherDump_ReadHex(m_pMem, &m_actualRegisters, g_testFilename) );
    CHECK_EQUAL(fileFormatException, getExceptionCode());
    clearExceptionCode();
    static const char* xmlForMemory = "<?xml version=\"1.0\"?>"
                                      "<!DOCTYPE memory-map PUBLIC \"+//IDN gnu.org//DTD GDB Memory Map V1.0//EN\" \"http://sourceware.org/gdb/gdb-memory-map.dtd\">"
                                      "<memory-map>"
                                      "<memory type=\"ram\" start=\"0x10000000\" length=\"0x4\"></memory>"
                                      "</memory-map>";
    const char* pMemoryLayout = MemorySim_GetMemoryMapXML(m_pMem);
    STRCMP_EQUAL(xmlForMemory, pMemoryLayout);
    CHECK_EQUAL(0x00111111, IMemory_Read32(m_pMem, 0x10000000));
}

TEST(CrashCatcherHexDump, DumpContainingTwoMemoryRegionsOf1Word_VerifyMemoryContents)
{
    struct FileData
    {
        DumpFileTop                  fileTop;
        CrashCatcherMemoryRegionInfo region1;
        uint32_t                     region1Data[1];
        CrashCatcherMemoryRegionInfo region2;
        uint32_t                     region2Data[1];
    } fileData;
    memcpy(&fileData.fileTop, &m_fileTop, sizeof(m_fileTop));
    fileData.region1.startAddress = 0x10000000;
    fileData.region1.endAddress = 0x10000004;
    fileData.region1Data[0] = 0x11111111;
    fileData.region2.startAddress = 0x20000000;
    fileData.region2.endAddress = 0x20000004;
    fileData.region2Data[0] = 0x22222222;
    createTestDumpFile(&fileData, sizeof(fileData));
        CrashCatcherDump_ReadHex(m_pMem, &m_actualRegisters, g_testFilename);
    static const char* xmlForMemory = "<?xml version=\"1.0\"?>"
                                      "<!DOCTYPE memory-map PUBLIC \"+//IDN gnu.org//DTD GDB Memory Map V1.0//EN\" \"http://sourceware.org/gdb/gdb-memory-map.dtd\">"
                                      "<memory-map>"
                                      "<memory type=\"ram\" start=\"0x10000000\" length=\"0x4\"></memory>"
                                      "<memory type=\"ram\" start=\"0x20000000\" length=\"0x4\"></memory>"
                                      "</memory-map>";
    const char* pMemoryLayout = MemorySim_GetMemoryMapXML(m_pMem);
    STRCMP_EQUAL(xmlForMemory, pMemoryLayout);
    CHECK_EQUAL(0x11111111, IMemory_Read32(m_pMem, 0x10000000));
    CHECK_EQUAL(0x22222222, IMemory_Read32(m_pMem, 0x20000000));
}

TEST(CrashCatcherHexDump, DumpContainingOneMemoryRegionsFollowedByStackOverflowError_VerifyMemoryContentsAndExceptionThrown)
{
    struct FileData
    {
        DumpFileTop                  fileTop;
        CrashCatcherMemoryRegionInfo region1;
        uint32_t                     region1Data[1];
        uint32_t                     overflowSentinel;
    } fileData;
    memcpy(&fileData.fileTop, &m_fileTop, sizeof(m_fileTop));
    fileData.region1.startAddress = 0x10000000;
    fileData.region1.endAddress = 0x10000004;
    fileData.region1Data[0] = 0x11111111;
    fileData.overflowSentinel = STACK_SENTINEL;
    createTestDumpFile(&fileData, sizeof(fileData));
        __try_and_catch( CrashCatcherDump_ReadHex(m_pMem, &m_actualRegisters, g_testFilename) );
    CHECK_EQUAL(stackOverflowException, getExceptionCode());
    clearExceptionCode();
    static const char* xmlForMemory = "<?xml version=\"1.0\"?>"
                                      "<!DOCTYPE memory-map PUBLIC \"+//IDN gnu.org//DTD GDB Memory Map V1.0//EN\" \"http://sourceware.org/gdb/gdb-memory-map.dtd\">"
                                      "<memory-map>"
                                      "<memory type=\"ram\" start=\"0x10000000\" length=\"0x4\"></memory>"
                                      "</memory-map>";
    const char* pMemoryLayout = MemorySim_GetMemoryMapXML(m_pMem);
    STRCMP_EQUAL(xmlForMemory, pMemoryLayout);
    CHECK_EQUAL(0x11111111, IMemory_Read32(m_pMem, 0x10000000));
}

TEST(CrashCatcherHexDump, DumpContainingNewlinesBetweenHexDigits_VerifyRegistersReadAndEmptyMemoryRegions)
{
    static const char testHexDump[] = "63430100\r\n"
                                      "00000000111111112222222233333333\n"
                                      "44444444555555556666666677777777\r"
                                      "8888888899999999AAAAAAAABBBBBBBB\n\r"
                                      "CCCCCCCCDDDDDDDDEEEEEEEEFFFFFFFF\r\n"
                                      "0DF00DF0EDFEADBA";
    FILE* pFile = fopen(g_testFilename, "w");
    fwrite(testHexDump, 1, sizeof(testHexDump) - 1, pFile);
    fclose(pFile);
        CrashCatcherDump_ReadHex(m_pMem, &m_actualRegisters, g_testFilename);
    validateNoMemoryRegionsCreated();
    m_expectedRegisters.R[R0]            = 0x0;
    m_expectedRegisters.R[R1]            = 0x11111111;
    m_expectedRegisters.R[R2]            = 0x22222222;
    m_expectedRegisters.R[R3]            = 0x33333333;
    m_expectedRegisters.R[R4]            = 0x44444444;
    m_expectedRegisters.R[R5]            = 0x55555555;
    m_expectedRegisters.R[R6]            = 0x66666666;
    m_expectedRegisters.R[R7]            = 0x77777777;
    m_expectedRegisters.R[R8]            = 0x88888888;
    m_expectedRegisters.R[R9]            = 0x99999999;
    m_expectedRegisters.R[R10]           = 0xAAAAAAAA;
    m_expectedRegisters.R[R11]           = 0xBBBBBBBB;
    m_expectedRegisters.R[R12]           = 0xCCCCCCCC;
    m_expectedRegisters.R[SP]            = 0xDDDDDDDD;
    m_expectedRegisters.R[LR]            = 0xEEEEEEEE;
    m_expectedRegisters.R[PC]            = 0xFFFFFFFF;
    m_expectedRegisters.R[XPSR]          = 0xF00DF00D;
    m_expectedRegisters.exceptionPSR     = 0xBAADFEED;
}

TEST(CrashCatcherHexDump, SameTestAsPreviousButWithLowercaseHexDigits)
{
    static const char testHexDump[] = "63430100\r\n"
                                      "00000000111111112222222233333333\n"
                                      "44444444555555556666666677777777\r"
                                      "8888888899999999aaaaaaaabbbbbbbb\n\r"
                                      "ccccccccddddddddeeeeeeeeffffffff\r\n"
                                      "0df00df0edfeadba";
    FILE* pFile = fopen(g_testFilename, "w");
    fwrite(testHexDump, 1, sizeof(testHexDump) - 1, pFile);
    fclose(pFile);
        CrashCatcherDump_ReadHex(m_pMem, &m_actualRegisters, g_testFilename);
    validateNoMemoryRegionsCreated();
    m_expectedRegisters.R[R0]            = 0x0;
    m_expectedRegisters.R[R1]            = 0x11111111;
    m_expectedRegisters.R[R2]            = 0x22222222;
    m_expectedRegisters.R[R3]            = 0x33333333;
    m_expectedRegisters.R[R4]            = 0x44444444;
    m_expectedRegisters.R[R5]            = 0x55555555;
    m_expectedRegisters.R[R6]            = 0x66666666;
    m_expectedRegisters.R[R7]            = 0x77777777;
    m_expectedRegisters.R[R8]            = 0x88888888;
    m_expectedRegisters.R[R9]            = 0x99999999;
    m_expectedRegisters.R[R10]           = 0xAAAAAAAA;
    m_expectedRegisters.R[R11]           = 0xBBBBBBBB;
    m_expectedRegisters.R[R12]           = 0xCCCCCCCC;
    m_expectedRegisters.R[SP]            = 0xDDDDDDDD;
    m_expectedRegisters.R[LR]            = 0xEEEEEEEE;
    m_expectedRegisters.R[PC]            = 0xFFFFFFFF;
    m_expectedRegisters.R[XPSR]          = 0xF00DF00D;
    m_expectedRegisters.exceptionPSR     = 0xBAADFEED;
}

TEST(CrashCatcherHexDump, DumpContainingOneTooFewHexDigits_VerifyExceptionThrown)
{
    static const char testHexDump[] = "63430100\r\n"
                                      "00000000111111112222222233333333\n"
                                      "44444444555555556666666677777777\r"
                                      "8888888899999999AAAAAAAABBBBBBBB\n\r"
                                      "CCCCCCCCDDDDDDDDEEEEEEEEFFFFFFFF\r\n"
                                      "0DF00DF0EDFEADBA";
    FILE* pFile = fopen(g_testFilename, "w");
    fwrite(testHexDump, 1, sizeof(testHexDump) - 2, pFile);
    fclose(pFile);
        __try_and_catch( CrashCatcherDump_ReadHex(m_pMem, &m_actualRegisters, g_testFilename) );
    CHECK_EQUAL(fileFormatException, getExceptionCode());
    clearExceptionCode();
    validateNoMemoryRegionsCreated();
    m_expectedRegisters.R[R0]            = 0x0;
    m_expectedRegisters.R[R1]            = 0x11111111;
    m_expectedRegisters.R[R2]            = 0x22222222;
    m_expectedRegisters.R[R3]            = 0x33333333;
    m_expectedRegisters.R[R4]            = 0x44444444;
    m_expectedRegisters.R[R5]            = 0x55555555;
    m_expectedRegisters.R[R6]            = 0x66666666;
    m_expectedRegisters.R[R7]            = 0x77777777;
    m_expectedRegisters.R[R8]            = 0x88888888;
    m_expectedRegisters.R[R9]            = 0x99999999;
    m_expectedRegisters.R[R10]           = 0xAAAAAAAA;
    m_expectedRegisters.R[R11]           = 0xBBBBBBBB;
    m_expectedRegisters.R[R12]           = 0xCCCCCCCC;
    m_expectedRegisters.R[SP]            = 0xDDDDDDDD;
    m_expectedRegisters.R[LR]            = 0xEEEEEEEE;
    m_expectedRegisters.R[PC]            = 0xFFFFFFFF;
    m_expectedRegisters.R[XPSR]          = 0xF00DF00D;
    m_expectedRegisters.exceptionPSR     = 0x00ADFEED;
}

TEST(CrashCatcherHexDump, DumpContainingNonHexDigitInLastByte_VerifyExceptionThrown)
{
    static const char testHexDump[] = "63430100\r\n"
                                      "00000000111111112222222233333333\n"
                                      "44444444555555556666666677777777\r"
                                      "8888888899999999AAAAAAAABBBBBBBB\n\r"
                                      "CCCCCCCCDDDDDDDDEEEEEEEEFFFFFFFF\r\n"
                                      "0DF00DF0EDFEADBG";
    FILE* pFile = fopen(g_testFilename, "w");
    fwrite(testHexDump, 1, sizeof(testHexDump) - 1, pFile);
    fclose(pFile);
        __try_and_catch( CrashCatcherDump_ReadHex(m_pMem, &m_actualRegisters, g_testFilename) );
    CHECK_EQUAL(fileFormatException, getExceptionCode());
    clearExceptionCode();
    validateNoMemoryRegionsCreated();
    m_expectedRegisters.R[R0]            = 0x0;
    m_expectedRegisters.R[R1]            = 0x11111111;
    m_expectedRegisters.R[R2]            = 0x22222222;
    m_expectedRegisters.R[R3]            = 0x33333333;
    m_expectedRegisters.R[R4]            = 0x44444444;
    m_expectedRegisters.R[R5]            = 0x55555555;
    m_expectedRegisters.R[R6]            = 0x66666666;
    m_expectedRegisters.R[R7]            = 0x77777777;
    m_expectedRegisters.R[R8]            = 0x88888888;
    m_expectedRegisters.R[R9]            = 0x99999999;
    m_expectedRegisters.R[R10]           = 0xAAAAAAAA;
    m_expectedRegisters.R[R11]           = 0xBBBBBBBB;
    m_expectedRegisters.R[R12]           = 0xCCCCCCCC;
    m_expectedRegisters.R[SP]            = 0xDDDDDDDD;
    m_expectedRegisters.R[LR]            = 0xEEEEEEEE;
    m_expectedRegisters.R[PC]            = 0xFFFFFFFF;
    m_expectedRegisters.R[XPSR]          = 0xF00DF00D;
    m_expectedRegisters.exceptionPSR     = 0x00ADFEED;
}
