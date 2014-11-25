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
    #include <mockSock.h>
}

// Include C++ headers for test harness.
#include "CppUTest/TestHarness.h"

TEST_GROUP(mockSock)
{
    void setup()
    {
        mockSock_Init(5);
    }

    void teardown()
    {
        mockSock_Uninit();
    }
};

TEST(mockSock, socket_ReturnNegative1)
{
    mockSock_socketSetReturn(-1);
    CHECK_EQUAL(-1, socket(PF_INET, SOCK_STREAM, 0));
}

TEST(mockSock, socket_ReturnValidSocket)
{
    mockSock_socketSetReturn(4);
    CHECK_EQUAL(4, socket(PF_INET, SOCK_STREAM, 0));
}

TEST(mockSock, setsockopt_CallWithInvalidParams_ShouldBeIgnored)
{
    CHECK_EQUAL(0, setsockopt(-1, -1, -1, NULL, 0));
}

TEST(mockSock, bind_ReturnZero)
{
    mockSock_bindSetReturn(0);
    CHECK_EQUAL(0, bind(-1, NULL, 0));
}

TEST(mockSock, bind_ReturnNegative1)
{
    mockSock_bindSetReturn(-1);
    CHECK_EQUAL(-1, bind(-1, NULL, 0));
}

TEST(mockSock, listen_ReturnZero)
{
    mockSock_listenSetReturn(0);
    CHECK_EQUAL(0, listen(-1, -1));
}

TEST(mockSock, listen_ReturnNegative1)
{
    mockSock_listenSetReturn(-1);
    CHECK_EQUAL(-1, listen(-1, -1));
}

TEST(mockSock, accept_ReturnZero)
{
    mockSock_acceptSetReturn(0);
    CHECK_EQUAL(0, accept(-1, NULL, NULL));
}

TEST(mockSock, accept_ReturnNegative1)
{
    mockSock_acceptSetReturn(-1);
    CHECK_EQUAL(-1, accept(-1, NULL, NULL));
}

TEST(mockSock, select_ReturnZero)
{
    mockSock_selectSetReturn(0);
    CHECK_EQUAL(0, select(-1, NULL, NULL, NULL, NULL));
}


TEST(mockSock, select_ReturnNegative1)
{
    mockSock_selectSetReturn(-1);
    CHECK_EQUAL(-1, select(-1, NULL, NULL, NULL, NULL));
}

TEST(mockSock, close_CallWithInvalidParams_ShouldBeIgnored)
{
    CHECK_EQUAL(0, close(-1));
}

TEST(mockSock, recv_SetOneByteToReturn)
{
    char buffer[2];
    mockSock_recvSetReturnValues(1, 0, 0, 0);
    mockSock_recvSetBuffer("a", 1);
        CHECK_EQUAL(1, recv(-1, buffer, 1, 0));
    buffer[1] = '\0';
    STRCMP_EQUAL("a", buffer);
}

TEST(mockSock, recv_TruncateReadFrom2to1)
{
    char buffer[3]= {0, 0, 0};
    mockSock_recvSetReturnValues(1, 0, 0, 0);
    mockSock_recvSetBuffer("ab", 2);
        CHECK_EQUAL(1, recv(-1, buffer, 2, 0));
    STRCMP_EQUAL("a", buffer);
}

TEST(mockSock, recv_Return4DescendingValues)
{
    char buffer[3];
    mockSock_recvSetReturnValues(2, 1, 0, -1);
    mockSock_recvSetBuffer("abcd", 4);

    CHECK_EQUAL(2, recv(-1, buffer, 2, 0));
    buffer[2] = '\0';
    STRCMP_EQUAL("ab", buffer);

    CHECK_EQUAL(1, recv(-1, buffer, 1, 0));
    buffer[1] = '\0';
    STRCMP_EQUAL("c", buffer);

    CHECK_EQUAL(0, recv(-1, buffer, 1, 0));
    CHECK_EQUAL(-1, recv(-1, buffer, 1, 0));
}

TEST(mockSock, send_FailSecondCall)
{
    mockSock_sendFailIteration(2);
    CHECK_EQUAL(1, send(-1, "a", 1, 0));
    CHECK_EQUAL(-1, send(-1, "b", 1, 0));
}

TEST(mockSock, send_RecordDataFromSingleCall)
{
    CHECK_EQUAL(3, send(-1, "abc", 3, 0));
    STRCMP_EQUAL("abc", mockSock_sendData());
}

TEST(mockSock, send_RecordDataFromTwoCall)
{
    CHECK_EQUAL(2, send(-1, "ab", 2, 0));
    CHECK_EQUAL(1, send(-1, "c", 1, 0));
    STRCMP_EQUAL("abc", mockSock_sendData());
}

TEST(mockSock, send_RecordMaximumData)
{
    // Maximum send data to record was set to 5 in setup().
    CHECK_EQUAL(5, send(-1, "abcde", 5, 0));
    STRCMP_EQUAL("abcde", mockSock_sendData());
}

TEST(mockSock, send_TruncateRecordedDataWhenOverflowing)
{
    // Maximum send data to record was set to 5 in setup().
    CHECK_EQUAL(6, send(-1, "abcdef", 6, 0));
    STRCMP_EQUAL("abcde", mockSock_sendData());
}
