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
#include <signal.h>
#include "mriPlatformBaseTest.h"

TEST_GROUP_BASE(registerTests, mriPlatformBase)
{
    void setup()
    {
        mriPlatformBase::setup();
        // Set IPSR to DebugMonitor exception to generate SIGTRAP exception code.
        setIPSR(12);
    }

    void teardown()
    {
        mriPlatformBase::teardown();
    }
};


TEST(registerTests, ReadRegisters)
{
    for (int i = 0 ; i < 15 ; i++)
        m_context.R[i] = 0x11111111 * i;
    m_context.R[PC] = 0xFFFFFFFE;

    mockIComm_InitReceiveChecksummedData("+$g#", "+$c#");
        mriPlatform_Run(mockIComm_Get());
    appendExpectedTPacket(SIGTRAP, 0xCCCCCCCC, 0xDDDDDDDD, 0xEEEEEEEE, 0xFFFFFFFE);
    appendExpectedString("+$00000000111111112222222233333333"
                           "44444444555555556666666677777777"
                           "8888888899999999aaaaaaaabbbbbbbb"
                           "ccccccccddddddddeeeeeeeefeffffff"
                           "00000001#+");
    STRCMP_EQUAL(checksumExpected(), mockIComm_GetTransmittedData());
}

TEST(registerTests, WriteRegisters)
{
    memset(&m_context, 0xa5, 16 * sizeof(uint32_t));
    mockIComm_InitReceiveChecksummedData("+$G00000000111111112222222233333333"
                                            "44444444555555556666666677777777"
                                            "8888888899999999aaaaaaaabbbbbbbb"
                                            "ccccccccddddddddeeeeeeeefeffffff"
                                            "ffffffff#", "+$c#");
        mriPlatform_Run(mockIComm_Get());
    appendExpectedTPacket(SIGTRAP, 0xA5A5A5A5, 0xA5A5A5A5, 0xA5A5A5A5, 0xA5A5A5A5);
    appendExpectedString("+$OK#+");
    STRCMP_EQUAL(checksumExpected(), mockIComm_GetTransmittedData());

    for (int i = 0 ; i < 15 ; i++)
        CHECK_EQUAL(0x11111111U * i, m_context.R[i]);
    CHECK_EQUAL(0xFFFFFFFEU, m_context.R[PC]);
    CHECK_EQUAL(0xFFFFFFFFU, m_context.R[XPSR]);
}
