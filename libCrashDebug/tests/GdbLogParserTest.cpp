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
    #include <FileFailureInject.h>
    #include <GdbLogParser.h>
    #include <MallocFailureInject.h>
    #include <mockFileIo.h>
}

// Include C++ headers for test harness.
#include "CppUTest/TestHarness.h"

#define DUMMY_FILE_HANDLE (FILE*)1

TEST_GROUP(GdbLogParser)
{
    IMemory*        m_pMem;
    RegisterContext m_actualRegisters;
    RegisterContext m_expectedRegisters;

    void setup()
    {
        m_pMem = MemorySim_Init();
        fakeLogFileAccess();
        for (size_t i = 0 ; i < ARRAY_SIZE(m_actualRegisters.R) ; i++)
        {
            m_actualRegisters.R[i] = 0xDEADBEEF;
            m_expectedRegisters.R[i] = 0xDEADBEEF;
        }
    }

    void fakeLogFileAccess()
    {
        fopenSetReturn(DUMMY_FILE_HANDLE);
        fcloseIgnore();
        fseekSetReturn(0);
    }

    void teardown()
    {
        CHECK_EQUAL(noException, getExceptionCode());
        checkRegisters();
        fopenRestore();
        fcloseRestore();
        fseekRestore();
        MemorySim_Uninit(m_pMem);
        mockFileIo_Uninit();
        MallocFailureInject_Restore();
        clearExceptionCode();
    }

    void checkRegisters()
    {
        for (size_t i = 0 ; i < ARRAY_SIZE(m_actualRegisters.R) ; i++)
        {
            CHECK_EQUAL(m_expectedRegisters.R[i], m_actualRegisters.R[i]);
        }
    }
};


TEST(GdbLogParser, InvalidLogFilename_ShouldThrow)
{
    fopenSetReturn(NULL);
        __try_and_catch( GdbLogParse(m_pMem, &m_actualRegisters, "invalid_file.log") );
    CHECK_EQUAL(fileException, getExceptionCode());
    clearExceptionCode();
}

TEST(GdbLogParser, OneLineLogFile_1RamValue_FailFSeekCall_ShouldThrow)
{
    static const char* testLines[] = { "0x10000000:\t0x11111111" };

    mockFileIo_SetFgetsData(testLines, ARRAY_SIZE(testLines));
    fseekSetReturn(-1);
        __try_and_catch( GdbLogParse(m_pMem, &m_actualRegisters, "foo.log") );
    CHECK_EQUAL(fileException, getExceptionCode());
    clearExceptionCode();
}

TEST(GdbLogParser, EmptyLogfile_ShouldReturnNoRegions)
{
    static const char* xmlForEmptyRegions = "<?xml version=\"1.0\"?>"
                                            "<!DOCTYPE memory-map PUBLIC \"+//IDN gnu.org//DTD GDB Memory Map V1.0//EN\" \"http://sourceware.org/gdb/gdb-memory-map.dtd\">"
                                            "<memory-map>"
                                            "</memory-map>";
        GdbLogParse(m_pMem, &m_actualRegisters, "foo.log");
    const char* pMemoryLayout = MemorySim_GetMemoryMapXML(m_pMem);
    STRCMP_EQUAL(xmlForEmptyRegions, pMemoryLayout);
}

TEST(GdbLogParser, OneLineLogFile_NotRamOrRegisters_ShouldReturnNoRegions)
{
    static const char* xmlForEmptyRegions = "<?xml version=\"1.0\"?>"
                                            "<!DOCTYPE memory-map PUBLIC \"+//IDN gnu.org//DTD GDB Memory Map V1.0//EN\" \"http://sourceware.org/gdb/gdb-memory-map.dtd\">"
                                            "<memory-map>"
                                            "</memory-map>";
    static const char* testLines[] = { "56	GPIO leds[5] = {" };

    mockFileIo_SetFgetsData(testLines, ARRAY_SIZE(testLines));
        GdbLogParse(m_pMem, &m_actualRegisters, "foo.log");
    const char* pMemoryLayout = MemorySim_GetMemoryMapXML(m_pMem);
    STRCMP_EQUAL(xmlForEmptyRegions, pMemoryLayout);
}

TEST(GdbLogParser, OneLineLogFile_1RamValue_ShouldReturn1WordRegion)
{
    static const char* xmlForMemory = "<?xml version=\"1.0\"?>"
                                      "<!DOCTYPE memory-map PUBLIC \"+//IDN gnu.org//DTD GDB Memory Map V1.0//EN\" \"http://sourceware.org/gdb/gdb-memory-map.dtd\">"
                                      "<memory-map>"
                                      "<memory type=\"ram\" start=\"0x10000000\" length=\"0x4\"></memory>"
                                      "</memory-map>";
    static const char* testLines[] = { "0x10000000:\t0x11111111" };

    mockFileIo_SetFgetsData(testLines, ARRAY_SIZE(testLines));
        GdbLogParse(m_pMem, &m_actualRegisters, "foo.log");
    const char* pMemoryLayout = MemorySim_GetMemoryMapXML(m_pMem);
    STRCMP_EQUAL(xmlForMemory, pMemoryLayout);
    CHECK_EQUAL(0x11111111, IMemory_Read32(m_pMem, 0x10000000));
}

TEST(GdbLogParser, OneLineLogFile_4RamValues_ShouldReturn4WordRegion)
{
    static const char* xmlForMemory = "<?xml version=\"1.0\"?>"
                                      "<!DOCTYPE memory-map PUBLIC \"+//IDN gnu.org//DTD GDB Memory Map V1.0//EN\" \"http://sourceware.org/gdb/gdb-memory-map.dtd\">"
                                      "<memory-map>"
                                      "<memory type=\"ram\" start=\"0x10000000\" length=\"0x10\"></memory>"
                                      "</memory-map>";
    static const char* testLines[] = { "0x10000000:\t0x11111111\t0x22222222\t0x33333333\t0x44444444" };

    mockFileIo_SetFgetsData(testLines, ARRAY_SIZE(testLines));
        GdbLogParse(m_pMem, &m_actualRegisters, "foo.log");
    const char* pMemoryLayout = MemorySim_GetMemoryMapXML(m_pMem);
    STRCMP_EQUAL(xmlForMemory, pMemoryLayout);
    CHECK_EQUAL(0x11111111, IMemory_Read32(m_pMem, 0x10000000));
    CHECK_EQUAL(0x22222222, IMemory_Read32(m_pMem, 0x10000004));
    CHECK_EQUAL(0x33333333, IMemory_Read32(m_pMem, 0x10000008));
    CHECK_EQUAL(0x44444444, IMemory_Read32(m_pMem, 0x1000000c));
}

TEST(GdbLogParser, OneLineLogFile_5RamValues_ShouldReturn4WordRegion)
{
    static const char* xmlForMemory = "<?xml version=\"1.0\"?>"
                                      "<!DOCTYPE memory-map PUBLIC \"+//IDN gnu.org//DTD GDB Memory Map V1.0//EN\" \"http://sourceware.org/gdb/gdb-memory-map.dtd\">"
                                      "<memory-map>"
                                      "<memory type=\"ram\" start=\"0x10000000\" length=\"0x10\"></memory>"
                                      "</memory-map>";
    static const char* testLines[] = { "0x10000000:\t0x11111111\t0x22222222\t0x33333333\t0x44444444\t55555555" };

    mockFileIo_SetFgetsData(testLines, ARRAY_SIZE(testLines));
        GdbLogParse(m_pMem, &m_actualRegisters, "foo.log");
    const char* pMemoryLayout = MemorySim_GetMemoryMapXML(m_pMem);
    STRCMP_EQUAL(xmlForMemory, pMemoryLayout);
    CHECK_EQUAL(0x11111111, IMemory_Read32(m_pMem, 0x10000000));
    CHECK_EQUAL(0x22222222, IMemory_Read32(m_pMem, 0x10000004));
    CHECK_EQUAL(0x33333333, IMemory_Read32(m_pMem, 0x10000008));
    CHECK_EQUAL(0x44444444, IMemory_Read32(m_pMem, 0x1000000c));
}

TEST(GdbLogParser, OneLineLogFile_4RamValuesWithSymbols_ShouldReturn4WordRegion)
{
    static const char* xmlForMemory = "<?xml version=\"1.0\"?>"
                                      "<!DOCTYPE memory-map PUBLIC \"+//IDN gnu.org//DTD GDB Memory Map V1.0//EN\" \"http://sourceware.org/gdb/gdb-memory-map.dtd\">"
                                      "<memory-map>"
                                      "<memory type=\"ram\" start=\"0x10000000\" length=\"0x10\"></memory>"
                                      "</memory-map>";
    static const char* testLines[] = { "0x10000000:\t0x11111111 <symbol>\t0x22222222 <<symbol>>\t0x33333333<<<symbol>>>\t0x44444444<<<<symbol>>>>" };

    mockFileIo_SetFgetsData(testLines, ARRAY_SIZE(testLines));
        GdbLogParse(m_pMem, &m_actualRegisters, "foo.log");
    const char* pMemoryLayout = MemorySim_GetMemoryMapXML(m_pMem);
    STRCMP_EQUAL(xmlForMemory, pMemoryLayout);
    CHECK_EQUAL(0x11111111, IMemory_Read32(m_pMem, 0x10000000));
    CHECK_EQUAL(0x22222222, IMemory_Read32(m_pMem, 0x10000004));
    CHECK_EQUAL(0x33333333, IMemory_Read32(m_pMem, 0x10000008));
    CHECK_EQUAL(0x44444444, IMemory_Read32(m_pMem, 0x1000000c));
}

TEST(GdbLogParser, TwoLineLogFile_8RamValues_ShouldReturn8WordRegion)
{
    static const char* xmlForMemory = "<?xml version=\"1.0\"?>"
                                      "<!DOCTYPE memory-map PUBLIC \"+//IDN gnu.org//DTD GDB Memory Map V1.0//EN\" \"http://sourceware.org/gdb/gdb-memory-map.dtd\">"
                                      "<memory-map>"
                                      "<memory type=\"ram\" start=\"0x10000000\" length=\"0x20\"></memory>"
                                      "</memory-map>";
    static const char* testLines[] = { "0x10000000:\t0x11111111\t0x22222222\t0x33333333\t0x44444444",
                                       "0x10000010:\t0x55555555\t0x66666666\t0x77777777\t0x88888888" };

    mockFileIo_SetFgetsData(testLines, ARRAY_SIZE(testLines));
        GdbLogParse(m_pMem, &m_actualRegisters, "foo.log");
    const char* pMemoryLayout = MemorySim_GetMemoryMapXML(m_pMem);
    STRCMP_EQUAL(xmlForMemory, pMemoryLayout);
    CHECK_EQUAL(0x11111111, IMemory_Read32(m_pMem, 0x10000000));
    CHECK_EQUAL(0x22222222, IMemory_Read32(m_pMem, 0x10000004));
    CHECK_EQUAL(0x33333333, IMemory_Read32(m_pMem, 0x10000008));
    CHECK_EQUAL(0x44444444, IMemory_Read32(m_pMem, 0x1000000c));
    CHECK_EQUAL(0x55555555, IMemory_Read32(m_pMem, 0x10000010));
    CHECK_EQUAL(0x66666666, IMemory_Read32(m_pMem, 0x10000014));
    CHECK_EQUAL(0x77777777, IMemory_Read32(m_pMem, 0x10000018));
    CHECK_EQUAL(0x88888888, IMemory_Read32(m_pMem, 0x1000001c));
}

TEST(GdbLogParser, TwoLineLogFile_8RamValues_ShouldReturnTwo4WordRegions)
{
    static const char* xmlForMemory = "<?xml version=\"1.0\"?>"
                                      "<!DOCTYPE memory-map PUBLIC \"+//IDN gnu.org//DTD GDB Memory Map V1.0//EN\" \"http://sourceware.org/gdb/gdb-memory-map.dtd\">"
                                      "<memory-map>"
                                      "<memory type=\"ram\" start=\"0x10000000\" length=\"0x10\"></memory>"
                                      "<memory type=\"ram\" start=\"0x20000000\" length=\"0x10\"></memory>"
                                      "</memory-map>";
    static const char* testLines[] = { "0x10000000:\t0x11111111\t0x22222222\t0x33333333\t0x44444444",
                                       "0x20000000:\t0x55555555\t0x66666666\t0x77777777\t0x88888888" };

    mockFileIo_SetFgetsData(testLines, ARRAY_SIZE(testLines));
        GdbLogParse(m_pMem, &m_actualRegisters, "foo.log");
    const char* pMemoryLayout = MemorySim_GetMemoryMapXML(m_pMem);
    STRCMP_EQUAL(xmlForMemory, pMemoryLayout);
    CHECK_EQUAL(0x11111111, IMemory_Read32(m_pMem, 0x10000000));
    CHECK_EQUAL(0x22222222, IMemory_Read32(m_pMem, 0x10000004));
    CHECK_EQUAL(0x33333333, IMemory_Read32(m_pMem, 0x10000008));
    CHECK_EQUAL(0x44444444, IMemory_Read32(m_pMem, 0x1000000c));
    CHECK_EQUAL(0x55555555, IMemory_Read32(m_pMem, 0x20000000));
    CHECK_EQUAL(0x66666666, IMemory_Read32(m_pMem, 0x20000004));
    CHECK_EQUAL(0x77777777, IMemory_Read32(m_pMem, 0x20000008));
    CHECK_EQUAL(0x88888888, IMemory_Read32(m_pMem, 0x2000000c));
}

TEST(GdbLogParser, FailMemoryAllocationForRegion_ShouldThrow)
{
    static const char* xmlForMemory = "<?xml version=\"1.0\"?>"
                                      "<!DOCTYPE memory-map PUBLIC \"+//IDN gnu.org//DTD GDB Memory Map V1.0//EN\" \"http://sourceware.org/gdb/gdb-memory-map.dtd\">"
                                      "<memory-map>"
                                      "</memory-map>";
    static const char* testLines[] = { "0x10000000:\t0x11111111" };

    mockFileIo_SetFgetsData(testLines, ARRAY_SIZE(testLines));
    MallocFailureInject_FailAllocation(1);
        __try_and_catch( GdbLogParse(m_pMem, &m_actualRegisters, "foo.log") );
    const char* pMemoryLayout = MemorySim_GetMemoryMapXML(m_pMem);
    STRCMP_EQUAL(xmlForMemory, pMemoryLayout);
    CHECK_EQUAL(outOfMemoryException, getExceptionCode());
    clearExceptionCode();
}

TEST(GdbLogParser, OneLineLogFile_JustR0Register_ShouldReturnNoRegionsAndSetR0)
{
    static const char* xmlForEmptyRegions = "<?xml version=\"1.0\"?>"
                                            "<!DOCTYPE memory-map PUBLIC \"+//IDN gnu.org//DTD GDB Memory Map V1.0//EN\" \"http://sourceware.org/gdb/gdb-memory-map.dtd\">"
                                            "<memory-map>"
                                            "</memory-map>";
    static const char* testLines[] = { "r0             0x11111111\t286331153" };

    mockFileIo_SetFgetsData(testLines, ARRAY_SIZE(testLines));
        GdbLogParse(m_pMem, &m_actualRegisters, "foo.log");
    const char* pMemoryLayout = MemorySim_GetMemoryMapXML(m_pMem);
    STRCMP_EQUAL(xmlForEmptyRegions, pMemoryLayout);
    m_expectedRegisters.R[R0] = 0x11111111;
}

TEST(GdbLogParser, HaveAllRegisters_ShouldReturnNoRegionsAndSetAllRegisters)
{
    static const char* xmlForEmptyRegions = "<?xml version=\"1.0\"?>"
                                            "<!DOCTYPE memory-map PUBLIC \"+//IDN gnu.org//DTD GDB Memory Map V1.0//EN\" \"http://sourceware.org/gdb/gdb-memory-map.dtd\">"
                                            "<memory-map>"
                                            "</memory-map>";
    static const char* testLines[] = { "r0             0x00000000\t0",
                                       "r1             0x11111111",
                                       "r2             0x22222222",
                                       "r3             0x33333333",
                                       "r4             0x44444444",
                                       "r5             0x55555555",
                                       "r6             0x66666666",
                                       "r7             0x77777777",
                                       "r8             0x88888888",
                                       "r9             0x99999999",
                                       "r10            0xAAAAAAAA",
                                       "r11            0xbbbbbbbb",
                                       "r12            0xcccccccc",
                                       "sp             0xdddddddd",
                                       "lr             0xeeeeeeee",
                                       "pc             0xffffffff",
                                       "xpsr           0xf00df00d" };

    mockFileIo_SetFgetsData(testLines, ARRAY_SIZE(testLines));
        GdbLogParse(m_pMem, &m_actualRegisters, "foo.log");
    const char* pMemoryLayout = MemorySim_GetMemoryMapXML(m_pMem);
    STRCMP_EQUAL(xmlForEmptyRegions, pMemoryLayout);
    m_expectedRegisters.R[R0]  = 0x0;
    m_expectedRegisters.R[R1]  = 0x11111111;
    m_expectedRegisters.R[R2]  = 0x22222222;
    m_expectedRegisters.R[R3]  = 0x33333333;
    m_expectedRegisters.R[R4]  = 0x44444444;
    m_expectedRegisters.R[R5]  = 0x55555555;
    m_expectedRegisters.R[R6]  = 0x66666666;
    m_expectedRegisters.R[R7]  = 0x77777777;
    m_expectedRegisters.R[R8]  = 0x88888888;
    m_expectedRegisters.R[R9]  = 0x99999999;
    m_expectedRegisters.R[R10] = 0xAAAAAAAA;
    m_expectedRegisters.R[R11] = 0xBBBBBBBB;
    m_expectedRegisters.R[R12] = 0xCCCCCCCC;
    m_expectedRegisters.R[SP]  = 0xDDDDDDDD;
    m_expectedRegisters.R[LR]  = 0xEEEEEEEE;
    m_expectedRegisters.R[PC]  = 0xFFFFFFFF;
    m_expectedRegisters.R[XPSR] = 0xF00DF00D;
}

TEST(GdbLogParser, HaveAllRegistersAndTwoMemoryBanks_ShouldReturnTwoRegionsAndSetAllRegisters)
{
    static const char* xmlForMemory = "<?xml version=\"1.0\"?>"
                                      "<!DOCTYPE memory-map PUBLIC \"+//IDN gnu.org//DTD GDB Memory Map V1.0//EN\" \"http://sourceware.org/gdb/gdb-memory-map.dtd\">"
                                      "<memory-map>"
                                      "<memory type=\"ram\" start=\"0x10000000\" length=\"0x10\"></memory>"
                                      "<memory type=\"ram\" start=\"0x20000000\" length=\"0x10\"></memory>"
                                      "</memory-map>";
    static const char* testLines[] = { "0x10000000:\t0x11111111\t0x22222222\t0x33333333\t0x44444444",
                                       "0x20000000:\t0x55555555\t0x66666666\t0x77777777\t0x88888888",
                                       "r0             0x00000000\t0",
                                       "r1             0x11111111",
                                       "r2             0x22222222",
                                       "r3             0x33333333",
                                       "r4             0x44444444",
                                       "r5             0x55555555",
                                       "r6             0x66666666",
                                       "r7             0x77777777",
                                       "r8             0x88888888",
                                       "r9             0x99999999",
                                       "r10            0xAAAAAAAA",
                                       "r11            0xbbbbbbbb",
                                       "r12            0xcccccccc",
                                       "sp             0xdddddddd",
                                       "lr             0xeeeeeeee",
                                       "pc             0xffffffff",
                                       "xpsr           0xf00df00d" };

    mockFileIo_SetFgetsData(testLines, ARRAY_SIZE(testLines));
        GdbLogParse(m_pMem, &m_actualRegisters, "foo.log");
    const char* pMemoryLayout = MemorySim_GetMemoryMapXML(m_pMem);
    STRCMP_EQUAL(xmlForMemory, pMemoryLayout);
    CHECK_EQUAL(0x11111111, IMemory_Read32(m_pMem, 0x10000000));
    CHECK_EQUAL(0x22222222, IMemory_Read32(m_pMem, 0x10000004));
    CHECK_EQUAL(0x33333333, IMemory_Read32(m_pMem, 0x10000008));
    CHECK_EQUAL(0x44444444, IMemory_Read32(m_pMem, 0x1000000c));
    CHECK_EQUAL(0x55555555, IMemory_Read32(m_pMem, 0x20000000));
    CHECK_EQUAL(0x66666666, IMemory_Read32(m_pMem, 0x20000004));
    CHECK_EQUAL(0x77777777, IMemory_Read32(m_pMem, 0x20000008));
    CHECK_EQUAL(0x88888888, IMemory_Read32(m_pMem, 0x2000000c));
    m_expectedRegisters.R[R0]  = 0x0;
    m_expectedRegisters.R[R1]  = 0x11111111;
    m_expectedRegisters.R[R2]  = 0x22222222;
    m_expectedRegisters.R[R3]  = 0x33333333;
    m_expectedRegisters.R[R4]  = 0x44444444;
    m_expectedRegisters.R[R5]  = 0x55555555;
    m_expectedRegisters.R[R6]  = 0x66666666;
    m_expectedRegisters.R[R7]  = 0x77777777;
    m_expectedRegisters.R[R8]  = 0x88888888;
    m_expectedRegisters.R[R9]  = 0x99999999;
    m_expectedRegisters.R[R10] = 0xAAAAAAAA;
    m_expectedRegisters.R[R11] = 0xBBBBBBBB;
    m_expectedRegisters.R[R12] = 0xCCCCCCCC;
    m_expectedRegisters.R[SP]  = 0xDDDDDDDD;
    m_expectedRegisters.R[LR]  = 0xEEEEEEEE;
    m_expectedRegisters.R[PC]  = 0xFFFFFFFF;
    m_expectedRegisters.R[XPSR] = 0xF00DF00D;
}
