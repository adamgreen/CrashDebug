/*  Copyright (C) 2015  Adam Green (https://github.com/adamgreen)

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*/
#include "CrashCatcherBaseTest.h"


TEST_GROUP_BASE(CrashCatcherBinaryDump, CrashCatcherBaseTest)
{
    void setup()
    {
        m_pTestFilename = "CrashCatcherBinaryDumpTest.dmp";
        CrashCatcherBaseTest::setup();
    }

    void teardown()
    {
        CrashCatcherBaseTest::teardown();
    }

    void createTestDumpFile(const void* pData, size_t dataSize)
    {
        FILE* pFile = fopen(m_pTestFilename, "w");
        fwrite(pData, 1, dataSize, pFile);
        fclose(pFile);
    }
};


TEST(CrashCatcherBinaryDump, InvalidLogFilename_ShouldThrowFileException)
{
    fopenSetReturn(NULL);
        __try_and_catch( CrashCatcherDump_ReadBinary(m_pMem, &m_actualRegisters, "invalidDumpFilename.dmp") );
    CHECK_EQUAL(fileException, getExceptionCode());
    clearExceptionCode();
}

TEST(CrashCatcherBinaryDump, InvalidSignature_ShouldThrowFileFormatException)
{
    m_fileTop.signature[3] += 1;
    createTestDumpFile(&m_fileTop, sizeof(m_fileTop));
        __try_and_catch( CrashCatcherDump_ReadBinary(m_pMem, &m_actualRegisters, m_pTestFilename) );
    CHECK_EQUAL(fileFormatException, getExceptionCode());
    clearExceptionCode();
}

TEST(CrashCatcherBinaryDump, FailSignatureRead_ShouldThrowFileFormatException)
{
    createTestDumpFile(&m_fileTop, sizeof(m_fileTop));
    freadFail(-1);
        __try_and_catch( CrashCatcherDump_ReadBinary(m_pMem, &m_actualRegisters, m_pTestFilename) );
    CHECK_EQUAL(fileFormatException, getExceptionCode());
    clearExceptionCode();
}

TEST(CrashCatcherBinaryDump, FileTooShortForFlags_ShouldThrowFileFormatException)
{
    createTestDumpFile(&m_fileTop, offsetof(DumpFileTop, R) - 1);
        __try_and_catch( CrashCatcherDump_ReadBinary(m_pMem, &m_actualRegisters, m_pTestFilename) );
    CHECK_EQUAL(fileFormatException, getExceptionCode());
    clearExceptionCode();
}

TEST(CrashCatcherBinaryDump, FileTooShortForRegisters_ShouldThrowFileFormatException)
{
    createTestDumpFile(&m_fileTop, sizeof(m_fileTop) - 1);
        __try_and_catch( CrashCatcherDump_ReadBinary(m_pMem, &m_actualRegisters, m_pTestFilename) );
    CHECK_EQUAL(fileFormatException, getExceptionCode());
    clearExceptionCode();
}

TEST(CrashCatcherBinaryDump, DumpContainingRegistersOnly_VerifyRegistersReadAndEmptyMemoryRegions)
{
    for (uint32_t i = 0 ; i < 16 ; i++)
        m_fileTop.R[i] = i * 0x11111111;
    m_fileTop.R[XPSR] = 0xF00DF00D;
    m_fileTop.exceptionPSR =  0xBAADFEED;
    createTestDumpFile(&m_fileTop, sizeof(m_fileTop));
        CrashCatcherDump_ReadBinary(m_pMem, &m_actualRegisters, m_pTestFilename);
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

TEST(CrashCatcherBinaryDump, DumpContainingFloatRegistersToo_VerifyRegistersReadAndEmptyMemoryRegions)
{
    DumpFPFileTop fileData;

    memcpy(&fileData, &m_fileTop, sizeof(m_fileTop));
    for (uint32_t i = 0 ; i < 16 ; i++)
        fileData.R[i] = i * 0x11111111;
    fileData.R[XPSR] = 0xF00DF00D;
    fileData.exceptionPSR =  0xBAADFEED;
    fileData.flags = CRASH_CATCHER_FLAGS_FLOATING_POINT;
    for (uint32_t i = 0 ; i < 32 ; i++)
        fileData.FPR[i] = i;
    fileData.FPR[FPSCR] = 0xBAADF00D;
    createTestDumpFile(&fileData, sizeof(fileData));
        CrashCatcherDump_ReadBinary(m_pMem, &m_actualRegisters, m_pTestFilename);
    validateNoMemoryRegionsCreated();
    m_expectedRegisters.flags            = CRASH_CATCHER_FLAGS_FLOATING_POINT;
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
    m_expectedRegisters.FPR[S0]          = 0;
    m_expectedRegisters.FPR[S1]          = 1;
    m_expectedRegisters.FPR[S2]          = 2;
    m_expectedRegisters.FPR[S3]          = 3;
    m_expectedRegisters.FPR[S4]          = 4;
    m_expectedRegisters.FPR[S5]          = 5;
    m_expectedRegisters.FPR[S6]          = 6;
    m_expectedRegisters.FPR[S7]          = 7;
    m_expectedRegisters.FPR[S8]          = 8;
    m_expectedRegisters.FPR[S9]          = 9;
    m_expectedRegisters.FPR[S10]         = 10;
    m_expectedRegisters.FPR[S11]         = 11;
    m_expectedRegisters.FPR[S12]         = 12;
    m_expectedRegisters.FPR[S13]         = 13;
    m_expectedRegisters.FPR[S14]         = 14;
    m_expectedRegisters.FPR[S15]         = 15;
    m_expectedRegisters.FPR[S16]         = 16;
    m_expectedRegisters.FPR[S17]         = 17;
    m_expectedRegisters.FPR[S18]         = 18;
    m_expectedRegisters.FPR[S19]         = 19;
    m_expectedRegisters.FPR[S20]         = 20;
    m_expectedRegisters.FPR[S21]         = 21;
    m_expectedRegisters.FPR[S22]         = 22;
    m_expectedRegisters.FPR[S23]         = 23;
    m_expectedRegisters.FPR[S24]         = 24;
    m_expectedRegisters.FPR[S25]         = 25;
    m_expectedRegisters.FPR[S26]         = 26;
    m_expectedRegisters.FPR[S27]         = 27;
    m_expectedRegisters.FPR[S28]         = 28;
    m_expectedRegisters.FPR[S29]         = 29;
    m_expectedRegisters.FPR[S30]         = 30;
    m_expectedRegisters.FPR[S31]         = 31;
    m_expectedRegisters.FPR[FPSCR]       = 0xBAADF00D;
}

TEST(CrashCatcherBinaryDump, FileTooShortForFloatRegisters_ShouldThrowFileFormatException)
{
    DumpFPFileTop fileData;
    memset(&fileData, 0, sizeof(fileData));
    memcpy(&fileData, &m_fileTop, sizeof(m_fileTop));
    fileData.flags = CRASH_CATCHER_FLAGS_FLOATING_POINT;

    createTestDumpFile(&fileData, sizeof(fileData) - 1);
        __try_and_catch( CrashCatcherDump_ReadBinary(m_pMem, &m_actualRegisters, m_pTestFilename) );
    CHECK_EQUAL(fileFormatException, getExceptionCode());
    clearExceptionCode();
    m_expectedRegisters.flags = CRASH_CATCHER_FLAGS_FLOATING_POINT;
}

TEST(CrashCatcherBinaryDump, DumpContainingOneMemoryRegionOf1Word_VerifyMemoryContents)
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
        CrashCatcherDump_ReadBinary(m_pMem, &m_actualRegisters, m_pTestFilename);
    static const char* xmlForMemory = "<?xml version=\"1.0\"?>"
                                      "<!DOCTYPE memory-map PUBLIC \"+//IDN gnu.org//DTD GDB Memory Map V1.0//EN\" \"http://sourceware.org/gdb/gdb-memory-map.dtd\">"
                                      "<memory-map>"
                                      "<memory type=\"ram\" start=\"0x10000000\" length=\"0x4\"></memory>"
                                      "</memory-map>";
    const char* pMemoryLayout = MemorySim_GetMemoryMapXML(m_pMem);
    STRCMP_EQUAL(xmlForMemory, pMemoryLayout);
    CHECK_EQUAL(0x11111111, IMemory_Read32(m_pMem, 0x10000000));
}

TEST(CrashCatcherBinaryDump, FailAllocationForRegion_VerifyEmptyMemoryAndExceptionThrown)
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
        __try_and_catch( CrashCatcherDump_ReadBinary(m_pMem, &m_actualRegisters, m_pTestFilename) );
    CHECK_EQUAL(outOfMemoryException, getExceptionCode());
    clearExceptionCode();
    validateNoMemoryRegionsCreated();
}

TEST(CrashCatcherBinaryDump, DumpContainingTruncatedMemoryRegionDescription_ExceptionShouldBeThrownAndNoMemoryRegionsCreated)
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
        __try_and_catch( CrashCatcherDump_ReadBinary(m_pMem, &m_actualRegisters, m_pTestFilename) );
    CHECK_EQUAL(fileFormatException, getExceptionCode());
    clearExceptionCode();
    validateNoMemoryRegionsCreated();
}

TEST(CrashCatcherBinaryDump, DumpContainingTruncatedMemoryRegionData_ExceptionShouldBeThrown_PartiallyCompletedMemoryRegion)
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
        __try_and_catch( CrashCatcherDump_ReadBinary(m_pMem, &m_actualRegisters, m_pTestFilename) );
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

TEST(CrashCatcherBinaryDump, DumpContainingTwoMemoryRegionsOf1Word_VerifyMemoryContents)
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
        CrashCatcherDump_ReadBinary(m_pMem, &m_actualRegisters, m_pTestFilename);
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

TEST(CrashCatcherBinaryDump, DumpContainingOneMemoryRegionsFollowedByStackOverflowError_VerifyMemoryContentsAndExceptionThrown)
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
    fileData.overflowSentinel = CRASH_CATCHER_STACK_SENTINEL;
    createTestDumpFile(&fileData, sizeof(fileData));
        __try_and_catch( CrashCatcherDump_ReadBinary(m_pMem, &m_actualRegisters, m_pTestFilename) );
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
