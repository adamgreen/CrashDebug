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
#include "string.h"

// Include headers from C modules under test.
extern "C"
{
    #include <common.h>
    #include <FileFailureInject.h>
    #include <MallocFailureInject.h>
    #include <mockFileIo.h>
    #include <CrashDebugCommandLine.h>
    #include <printfSpy.h>
}

// Include C++ headers for test harness.
#include "CppUTest/TestHarness.h"


// This test harness wants access to the actual fgets() routine and not the mock.
#undef fgets


static const char     g_usageString[] = "Usage:";
static const char*    g_imageFilename = "image.bin";
static const char*    g_dumpFilename = "gdb.txt";
static const uint32_t g_imageData[2] = { 0x10000004, 0x00000100 };
static const char     g_dumpData[] =  "0x10000000:\t0x11111111\t0x22222222\t0x33333333\t0x44444444\n"
                                      "r0             0x5a5a5a5a\t0\n"
                                      "r1             0x11111111\n"
                                      "r2             0x22222222\n"
                                      "r3             0x33333333\n"
                                      "r4             0x44444444\n"
                                      "r5             0x55555555\n"
                                      "r6             0x66666666\n"
                                      "r7             0x77777777\n"
                                      "r8             0x88888888\n"
                                      "r9             0x99999999\n"
                                      "r10            0xAAAAAAAA\n"
                                      "r11            0xbbbbbbbb\n"
                                      "r12            0xcccccccc\n"
                                      "sp             0xdddddddd\n"
                                      "lr             0xeeeeeeee\n"
                                      "pc             0xffffffff\n"
                                      "xpsr           0xf00df00d\n";



TEST_GROUP(CrashDebugCommandLine)
{
    const char*           m_argv[10];
    CrashDebugCommandLine m_commandLine;
    int                   m_argc;
    RegisterContext       m_expectedRegisters;
    char*                 (*m_fgets)(char * str, int size, FILE * stream);

    void setup()
    {
        memset(m_argv, 0, sizeof(m_argv));
        memset(&m_commandLine, 0xff, sizeof(m_commandLine));
        memset(&m_expectedRegisters, 0x00, sizeof(m_expectedRegisters));
        m_argc = 0;

        unhookFgets();
        printfSpy_Hook(strlen(g_usageString));
    }

    void unhookFgets()
    {
        m_fgets = hook_fgets;
        hook_fgets = fgets;
    }

    void teardown()
    {
        CHECK_EQUAL(noException, getExceptionCode());
        checkRegisters();
        rehookFgets();
        fopenRestore();
        fseekRestore();
        ftellRestore();
        freadRestore();
        fwriteRestore();
        printfSpy_Unhook();
        MallocFailureInject_Restore();
        CrashDebugCommandLine_Uninit(&m_commandLine);
        remove(g_imageFilename);
        remove(g_dumpFilename);
    }

    void checkRegisters()
    {
        for (size_t i = 0 ; i < ARRAY_SIZE(m_expectedRegisters.R) ; i++)
        {
            CHECK_EQUAL(m_expectedRegisters.R[i], m_commandLine.context.R[i]);
        }
    }

    void rehookFgets()
    {
        hook_fgets = m_fgets;
    }

    void addArg(const char* pArg)
    {
        CHECK(m_argc < (int)ARRAY_SIZE(m_argv));
        m_argv[m_argc++] = pArg;
    }

    void validateExceptionThrownAndUsageStringDisplayed(int expectedException = invalidArgumentException)
    {
        CHECK_EQUAL(expectedException, getExceptionCode());
        STRCMP_EQUAL(g_usageString, printfSpy_GetLastOutput());
        clearExceptionCode();
    }

    void createTestFiles()
    {
        FILE* pFile = fopen(g_imageFilename, "w");
        fwrite(g_imageData, 1, sizeof(g_imageData), pFile);
        fclose(pFile);

        pFile = fopen(g_dumpFilename, "w");
        fwrite(g_dumpData, 1, sizeof(g_dumpData), pFile);
        fclose(pFile);
    }
};


TEST(CrashDebugCommandLine, NoParameters_ShouldThrow)
{
    __try_and_catch( CrashDebugCommandLine_Init(&m_commandLine, m_argc, m_argv) );
    validateExceptionThrownAndUsageStringDisplayed();
    CHECK(m_commandLine.pMemory == NULL);
}

TEST(CrashDebugCommandLine, NotDoubleSlashArgument_ShouldThrowAsNotAllowedForThisApp)
{
    addArg(g_imageFilename);
    createTestFiles();
        __try_and_catch( CrashDebugCommandLine_Init(&m_commandLine, m_argc, m_argv) );
    validateExceptionThrownAndUsageStringDisplayed();
    CHECK(m_commandLine.pMemory == NULL);
}

TEST(CrashDebugCommandLine, InvalidCommandLineFlag_ShouldThrow)
{
    addArg("--foo");
    createTestFiles();
        __try_and_catch( CrashDebugCommandLine_Init(&m_commandLine, m_argc, m_argv) );
    validateExceptionThrownAndUsageStringDisplayed();
    CHECK(m_commandLine.pMemory == NULL);
}

TEST(CrashDebugCommandLine, JustImageFilename_ShouldThrowAsDumpFilenameIsRequiredToo)
{
    addArg("--bin");
    addArg(g_imageFilename);
    addArg("0x0");
    createTestFiles();
        __try_and_catch( CrashDebugCommandLine_Init(&m_commandLine, m_argc, m_argv) );
    validateExceptionThrownAndUsageStringDisplayed();
    CHECK(m_commandLine.pMemory == NULL);
}

TEST(CrashDebugCommandLine, LeaveOffImageFilenameAndBaseAddress_ShouldThrow)
{
    addArg("--dump");
    addArg(g_dumpFilename);
    addArg("--bin");
    createTestFiles();
        __try_and_catch( CrashDebugCommandLine_Init(&m_commandLine, m_argc, m_argv) );
    validateExceptionThrownAndUsageStringDisplayed();
    CHECK(m_commandLine.pMemory == NULL);
}

TEST(CrashDebugCommandLine, LeaveOffBaseAddress_ShouldThrow)
{
    addArg("--dump");
    addArg(g_dumpFilename);
    addArg("--bin");
    addArg(g_imageFilename);
    createTestFiles();
        __try_and_catch( CrashDebugCommandLine_Init(&m_commandLine, m_argc, m_argv) );
    validateExceptionThrownAndUsageStringDisplayed();
    CHECK(m_commandLine.pMemory == NULL);
}

TEST(CrashDebugCommandLine, LeaveOffDumpFilename_ShouldThrow)
{
    addArg("--bin");
    addArg(g_imageFilename);
    addArg("0x0");
    addArg("--dump");
    createTestFiles();
        __try_and_catch( CrashDebugCommandLine_Init(&m_commandLine, m_argc, m_argv) );
    validateExceptionThrownAndUsageStringDisplayed();
    CHECK(m_commandLine.pMemory == NULL);
}

TEST(CrashDebugCommandLine, ValidImageAndDumpFilenames_ValidateMemoryAndRegisters)
{
    addArg("--bin");
    addArg(g_imageFilename);
    addArg("0x0");
    addArg("--dump");
    addArg(g_dumpFilename);
    createTestFiles();
        CrashDebugCommandLine_Init(&m_commandLine, m_argc, m_argv);
    CHECK_EQUAL(g_imageData[0], IMemory_Read32(m_commandLine.pMemory, 0x00000000));
    CHECK_EQUAL(g_imageData[1], IMemory_Read32(m_commandLine.pMemory, 0x00000004));
    CHECK_EQUAL(0x11111111, IMemory_Read32(m_commandLine.pMemory, 0x10000000));
    CHECK_EQUAL(0x22222222, IMemory_Read32(m_commandLine.pMemory, 0x10000004));
    CHECK_EQUAL(0x33333333, IMemory_Read32(m_commandLine.pMemory, 0x10000008));
    CHECK_EQUAL(0x44444444, IMemory_Read32(m_commandLine.pMemory, 0x1000000c));
    m_expectedRegisters.R[R0]  = 0x5a5a5a5a;
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

TEST(CrashDebugCommandLine, ValidImageAndDumpFilenames_DifferentBaseAddress_ValidateMemoryAndRegisters)
{
    addArg("--bin");
    addArg(g_imageFilename);
    addArg("0x4000");
    addArg("--dump");
    addArg(g_dumpFilename);
    createTestFiles();
        CrashDebugCommandLine_Init(&m_commandLine, m_argc, m_argv);
    CHECK_EQUAL(g_imageData[0], IMemory_Read32(m_commandLine.pMemory, 0x00004000));
    CHECK_EQUAL(g_imageData[1], IMemory_Read32(m_commandLine.pMemory, 0x00004004));
    CHECK_EQUAL(0x11111111, IMemory_Read32(m_commandLine.pMemory, 0x10000000));
    CHECK_EQUAL(0x22222222, IMemory_Read32(m_commandLine.pMemory, 0x10000004));
    CHECK_EQUAL(0x33333333, IMemory_Read32(m_commandLine.pMemory, 0x10000008));
    CHECK_EQUAL(0x44444444, IMemory_Read32(m_commandLine.pMemory, 0x1000000c));
    m_expectedRegisters.R[R0]  = 0x5a5a5a5a;
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

TEST(CrashDebugCommandLine, FailImageFileOpen_ShouldThrow)
{
    addArg("--bin");
    addArg(g_imageFilename);
    addArg("0x0");
    addArg("--dump");
    addArg(g_dumpFilename);
    createTestFiles();
    fopenSetReturn(NULL);
        __try_and_catch( CrashDebugCommandLine_Init(&m_commandLine, m_argc, m_argv) );
    validateExceptionThrownAndUsageStringDisplayed(fileException);
}

TEST(CrashDebugCommandLine, FailImageFileSeekInGetFileSize_ShouldThrow)
{
    addArg("--bin");
    addArg(g_imageFilename);
    addArg("0x0");
    addArg("--dump");
    addArg(g_dumpFilename);
    createTestFiles();
    fseekSetReturn(-1);
        __try_and_catch( CrashDebugCommandLine_Init(&m_commandLine, m_argc, m_argv) );
    validateExceptionThrownAndUsageStringDisplayed(fileException);
}

TEST(CrashDebugCommandLine, FailImageFileBufferAllocation_ShouldThrow)
{
    addArg("--bin");
    addArg(g_imageFilename);
    addArg("0x0");
    addArg("--dump");
    addArg(g_dumpFilename);
    createTestFiles();
    MallocFailureInject_FailAllocation(1);
        __try_and_catch( CrashDebugCommandLine_Init(&m_commandLine, m_argc, m_argv) );
    validateExceptionThrownAndUsageStringDisplayed(outOfMemoryException);
}

TEST(CrashDebugCommandLine, FailImageFileRead_ShouldThrow)
{
    addArg("--bin");
    addArg(g_imageFilename);
    addArg("0x0");
    addArg("--dump");
    addArg(g_dumpFilename);
    createTestFiles();
    freadFail(-1);
        __try_and_catch( CrashDebugCommandLine_Init(&m_commandLine, m_argc, m_argv) );
    validateExceptionThrownAndUsageStringDisplayed(fileException);
}

TEST(CrashDebugCommandLine, FailMemoryRegionAllocation_ShouldThrow)
{
    addArg("--bin");
    addArg(g_imageFilename);
    addArg("0");
    addArg("--dump");
    addArg(g_dumpFilename);
    createTestFiles();
    MallocFailureInject_FailAllocation(2);
        __try_and_catch( CrashDebugCommandLine_Init(&m_commandLine, m_argc, m_argv) );
    validateExceptionThrownAndUsageStringDisplayed(outOfMemoryException);
}
