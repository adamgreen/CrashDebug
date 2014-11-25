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
#include <string.h>

extern "C"
{
    #include <mockFileIo.h>
    #include <mockSock.h>
    #include <StandardIComm.h>
}

// Include C++ headers for test harness.
#include "CppUTest/TestHarness.h"

TEST_GROUP(StandardIComm)
{
    IComm* m_pComm;
    void setup()
    {
        clearExceptionCode();
        m_pComm = StandardIComm_Init();
        CHECK(m_pComm != NULL);
        mockSock_Init(5);
        mockFileIo_CreateWriteBuffer(5);
    }

    void teardown()
    {
        CHECK_EQUAL(noException, getExceptionCode());
        StandardIComm_Uninit(m_pComm);
        mockSock_Uninit();
        mockFileIo_Uninit();
    }
};


TEST(StandardIComm, ShouldStopRun_AlwaysReturnFALSE)
{
    CHECK_FALSE(IComm_ShouldStopRun(m_pComm));
}


TEST(StandardIComm, FailSelectCall_HasReceiveData_ShouldThrow)
{
    mockSock_selectSetReturn(-1);
        __try_and_catch( IComm_HasReceiveData(m_pComm) );
    CHECK_EQUAL(fileException, getExceptionCode());
    clearExceptionCode();
}

TEST(StandardIComm, HasReceiveData_SelectReturn1_ShouldReturnTrue)
{
    mockSock_selectSetReturn(1);
    CHECK_TRUE(IComm_HasReceiveData(m_pComm));
}

TEST(StandardIComm, HasReceiveData_SelectReturn0_ShouldReturnFalse)
{
    mockSock_selectSetReturn(0);
    CHECK_FALSE(IComm_HasReceiveData(m_pComm));
}


TEST(StandardIComm, ReceiveChar_ReadReturnNegativeOne_ShouldThrow)
{
    mockSock_selectSetReturn(1);
    mockFileIo_SetReadData("a", 1);
    mockFileIo_SetReadToFail(-1, -1);
        __try_and_catch( IComm_ReceiveChar(m_pComm) );
    CHECK_EQUAL(fileException, getExceptionCode());
    clearExceptionCode();
}

TEST(StandardIComm, ReceiveChar_ReadSingleChar)
{
    mockSock_selectSetReturn(1);
    mockFileIo_SetReadData("a", 1);
    char c = IComm_ReceiveChar(m_pComm);
    CHECK_EQUAL('a', c);
}

TEST(StandardIComm, ReceiveChar_ReadReturnZero_ShouldReturnZero)
{
    mockSock_selectSetReturn(1);
        char c = IComm_ReceiveChar(m_pComm);
    CHECK_EQUAL(0, c);
}


TEST(StandardIComm, SendChar_FailWrite_ShouldThrow)
{
    mockFileIo_SetWriteToFail(-1, -1);
        __try_and_catch( IComm_SendChar(m_pComm, 'z') );
    CHECK_EQUAL(fileException, getExceptionCode());
    clearExceptionCode();
}

TEST(StandardIComm, SendChar_VerifySuccessfullySentBytes)
{
    IComm_SendChar(m_pComm, 'x');
    IComm_SendChar(m_pComm, 'y');
    IComm_SendChar(m_pComm, 'z');
    STRCMP_EQUAL("xyz", mockFileIo_GetStdOutData());
}


TEST(StandardIComm, IsGdbConnected_ShouldReturnTrue)
{
    CHECK_TRUE(IComm_IsGdbConnected(m_pComm));
}
