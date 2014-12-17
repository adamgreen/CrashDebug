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
#ifndef _MRI_PLATFORM_H_
#define _MRI_PLATFORM_H_

#include <IComm.h>
#include <IMemory.h>
#include <try_catch.h>

/* Register names / indices into the RegisterContext::R array of registers. */
#define R0              0
#define R1              1
#define R2              2
#define R3              3
#define R4              4
#define R5              5
#define R6              6
#define R7              7
#define R8              8
#define R9              9
#define R10             10
#define R11             11
#define R12             12
#define SP              13
#define LR              14
#define PC              15
#define XPSR            16
#define TOTAL_REG_COUNT (XPSR + 1)

typedef struct RegisterContext
{
    uint32_t R[TOTAL_REG_COUNT];
    uint32_t exceptionPSR;
} RegisterContext;


__throws void mriPlatform_Init(RegisterContext* pContext, IMemory* pMem);
         void mriPlatform_Run(IComm* pComm);

#endif /* _MRI_PLATFORM_H_ */
