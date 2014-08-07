/*  Copyright (C) 2012  Adam Green (https://github.com/adamgreen)

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
    #include "FileFailureInject.h"
}

// Include C++ headers for test harness.
#include "CppUTest/TestHarness.h"

static const char* testFilename = "FileFailureInjectTestCpp.S";

TEST_GROUP(FileFailureInject)
{
    FILE* m_pFile;

    void setup()
    {
        m_pFile = NULL;
    }

    void teardown()
    {
        if (m_pFile)
            fclose(m_pFile);
        remove(testFilename);
    }

    void openFile()
    {
        m_pFile = hook_fopen(testFilename, "wb");
    }

    void createSmallTestFile()
    {
        openFile();
        LONGS_EQUAL(5, fwrite("12345", 1, 5, m_pFile));
        fclose(m_pFile);
        m_pFile = fopen(testFilename, "rb");
        CHECK(m_pFile != NULL);
    }
};


TEST(FileFailureInject, SuccessfulFOpen)
{
    openFile();
    CHECK(m_pFile != NULL);
}

TEST(FileFailureInject, FailFOpen)
{
    fopenFail(NULL);
    openFile();
    fopenRestore();
    POINTERS_EQUAL(NULL, m_pFile);
}

TEST(FileFailureInject, SuccessfulFSeek)
{
    openFile();
    LONGS_EQUAL(0, hook_fseek(m_pFile, 0, SEEK_SET));
}

TEST(FileFailureInject, FailFSeek)
{
    openFile();
    fseekSetFailureCode(-1);
    fseekSetCallsBeforeFailure(0);
    LONGS_EQUAL(-1, hook_fseek(m_pFile, 0, SEEK_SET));
    fseekRestore();
}

TEST(FileFailureInject, FailSecondFSeek)
{
    openFile();
    fseekSetFailureCode(-1);
    fseekSetCallsBeforeFailure(1);
    LONGS_EQUAL(0, hook_fseek(m_pFile, 0, SEEK_SET));
    LONGS_EQUAL(-1, hook_fseek(m_pFile, 0, SEEK_SET));
    fseekRestore();
}

TEST(FileFailureInject, SuccessfulFTell)
{
    openFile();
    LONGS_EQUAL(0, hook_ftell(m_pFile));
}

TEST(FileFailureInject, FailFTell)
{
    openFile();
    ftellFail(-1);
    LONGS_EQUAL(-1, hook_ftell(m_pFile));
    ftellRestore();
}

TEST(FileFailureInject, SuccessfulFWrite)
{
    openFile();
    LONGS_EQUAL(1, hook_fwrite(" ", 1, 1, m_pFile));
}

TEST(FileFailureInject, FailFWrite)
{
    openFile();
    fwriteFail(0);
    LONGS_EQUAL(0, hook_fwrite(" ", 1, 1, m_pFile));
    fwriteRestore();
}

TEST(FileFailureInject, SuccessfulFRead)
{
    char buffer[16];

    createSmallTestFile();
    LONGS_EQUAL(1, hook_fread(buffer, 1, 1, m_pFile));
}

TEST(FileFailureInject, FailFRead)
{
    char buffer[16];

    createSmallTestFile();
    freadFail(0);
    LONGS_EQUAL(0, hook_fread(buffer, 1, 1, m_pFile));
    freadRestore();
}

TEST(FileFailureInject, FailSecondOutOfThreeFReads)
{
    char buffer[16];

    createSmallTestFile();
    freadFail(0);
    freadToFail(2);
    LONGS_EQUAL(1, hook_fread(buffer, 1, 1, m_pFile));
    LONGS_EQUAL(0, hook_fread(buffer, 1, 1, m_pFile));
    LONGS_EQUAL(1, hook_fread(buffer, 1, 1, m_pFile));
    freadRestore();
}
