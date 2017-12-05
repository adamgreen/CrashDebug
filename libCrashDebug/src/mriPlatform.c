/*  Copyright (C) 2017  Adam Green (https://github.com/adamgreen)

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
#include <gdb_console.h>
#include <IMemory.h>
#include <signal.h>
#include <string.h>
#include <MemorySim.h>
#include <mri.h>
#include <mriPlatform.h>
#include <platforms.h>
#include <posix4win.h>
#include <printfSpy.h>
#include <semihost.h>


/* NOTE: This is the original version of the following XML which has had things stripped to reduce the amount of
         FLASH consumed by the debug monitor.  This includes the removal of the copyright comment.
<?xml version="1.0"?>
<!-- Copyright (C) 2010, 2011 Free Software Foundation, Inc.

     Copying and distribution of this file, with or without modification,
     are permitted in any medium without royalty provided the copyright
     notice and this notice are preserved.  -->

<!DOCTYPE feature SYSTEM "gdb-target.dtd">
<feature name="org.gnu.gdb.arm.m-profile">
  <reg name="r0" bitsize="32"/>
  <reg name="r1" bitsize="32"/>
  <reg name="r2" bitsize="32"/>
  <reg name="r3" bitsize="32"/>
  <reg name="r4" bitsize="32"/>
  <reg name="r5" bitsize="32"/>
  <reg name="r6" bitsize="32"/>
  <reg name="r7" bitsize="32"/>
  <reg name="r8" bitsize="32"/>
  <reg name="r9" bitsize="32"/>
  <reg name="r10" bitsize="32"/>
  <reg name="r11" bitsize="32"/>
  <reg name="r12" bitsize="32"/>
  <reg name="sp" bitsize="32" type="data_ptr"/>
  <reg name="lr" bitsize="32"/>
  <reg name="pc" bitsize="32" type="code_ptr"/>
  <reg name="xpsr" bitsize="32" regnum="25"/>
</feature>
*/
static const char g_targetXML[] =
    "<?xml version=\"1.0\"?>\n"
    "<!DOCTYPE feature SYSTEM \"gdb-target.dtd\">\n"
    "<target>\n"
    "<feature name=\"org.gnu.gdb.arm.m-profile\">\n"
    "<reg name=\"r0\" bitsize=\"32\"/>\n"
    "<reg name=\"r1\" bitsize=\"32\"/>\n"
    "<reg name=\"r2\" bitsize=\"32\"/>\n"
    "<reg name=\"r3\" bitsize=\"32\"/>\n"
    "<reg name=\"r4\" bitsize=\"32\"/>\n"
    "<reg name=\"r5\" bitsize=\"32\"/>\n"
    "<reg name=\"r6\" bitsize=\"32\"/>\n"
    "<reg name=\"r7\" bitsize=\"32\"/>\n"
    "<reg name=\"r8\" bitsize=\"32\"/>\n"
    "<reg name=\"r9\" bitsize=\"32\"/>\n"
    "<reg name=\"r10\" bitsize=\"32\"/>\n"
    "<reg name=\"r11\" bitsize=\"32\"/>\n"
    "<reg name=\"r12\" bitsize=\"32\"/>\n"
    "<reg name=\"sp\" bitsize=\"32\" type=\"data_ptr\"/>\n"
    "<reg name=\"lr\" bitsize=\"32\"/>\n"
    "<reg name=\"pc\" bitsize=\"32\" type=\"code_ptr\"/>\n"
    "<reg name=\"xpsr\" bitsize=\"32\" regnum=\"25\"/>\n"
    "</feature>\n"
    "<feature name=\"org.gnu.gdb.arm.m-system\">\n"
    "<reg name=\"msp\" bitsize=\"32\" regnum=\"26\"/>\n"
    "<reg name=\"psp\" bitsize=\"32\" regnum=\"27\"/>\n"
    "</feature>\n"
    "</target>\n";

static const char g_targetFpuXML[] =
    "<?xml version=\"1.0\"?>\n"
    "<!DOCTYPE feature SYSTEM \"gdb-target.dtd\">\n"
    "<target>\n"
    "<feature name=\"org.gnu.gdb.arm.m-profile\">\n"
    "<reg name=\"r0\" bitsize=\"32\"/>\n"
    "<reg name=\"r1\" bitsize=\"32\"/>\n"
    "<reg name=\"r2\" bitsize=\"32\"/>\n"
    "<reg name=\"r3\" bitsize=\"32\"/>\n"
    "<reg name=\"r4\" bitsize=\"32\"/>\n"
    "<reg name=\"r5\" bitsize=\"32\"/>\n"
    "<reg name=\"r6\" bitsize=\"32\"/>\n"
    "<reg name=\"r7\" bitsize=\"32\"/>\n"
    "<reg name=\"r8\" bitsize=\"32\"/>\n"
    "<reg name=\"r9\" bitsize=\"32\"/>\n"
    "<reg name=\"r10\" bitsize=\"32\"/>\n"
    "<reg name=\"r11\" bitsize=\"32\"/>\n"
    "<reg name=\"r12\" bitsize=\"32\"/>\n"
    "<reg name=\"sp\" bitsize=\"32\" type=\"data_ptr\"/>\n"
    "<reg name=\"lr\" bitsize=\"32\"/>\n"
    "<reg name=\"pc\" bitsize=\"32\" type=\"code_ptr\"/>\n"
    "<reg name=\"xpsr\" bitsize=\"32\" regnum=\"25\"/>\n"
    "</feature>\n"
    "<feature name=\"org.gnu.gdb.arm.m-system\">\n"
    "<reg name=\"msp\" bitsize=\"32\" regnum=\"26\"/>\n"
    "<reg name=\"psp\" bitsize=\"32\" regnum=\"27\"/>\n"
    "</feature>\n"
    "<feature name=\"org.gnu.gdb.arm.vfp\">\n"
    "<reg name=\"d0\" bitsize=\"64\" type=\"ieee_double\"/>\n"
    "<reg name=\"d1\" bitsize=\"64\" type=\"ieee_double\"/>\n"
    "<reg name=\"d2\" bitsize=\"64\" type=\"ieee_double\"/>\n"
    "<reg name=\"d3\" bitsize=\"64\" type=\"ieee_double\"/>\n"
    "<reg name=\"d4\" bitsize=\"64\" type=\"ieee_double\"/>\n"
    "<reg name=\"d5\" bitsize=\"64\" type=\"ieee_double\"/>\n"
    "<reg name=\"d6\" bitsize=\"64\" type=\"ieee_double\"/>\n"
    "<reg name=\"d7\" bitsize=\"64\" type=\"ieee_double\"/>\n"
    "<reg name=\"d8\" bitsize=\"64\" type=\"ieee_double\"/>\n"
    "<reg name=\"d9\" bitsize=\"64\" type=\"ieee_double\"/>\n"
    "<reg name=\"d10\" bitsize=\"64\" type=\"ieee_double\"/>\n"
    "<reg name=\"d11\" bitsize=\"64\" type=\"ieee_double\"/>\n"
    "<reg name=\"d12\" bitsize=\"64\" type=\"ieee_double\"/>\n"
    "<reg name=\"d13\" bitsize=\"64\" type=\"ieee_double\"/>\n"
    "<reg name=\"d14\" bitsize=\"64\" type=\"ieee_double\"/>\n"
    "<reg name=\"d15\" bitsize=\"64\" type=\"ieee_double\"/>\n"
    "<reg name=\"fpscr\" bitsize=\"32\" type=\"int\" group=\"float\"/>\n"
    "</feature>\n"
    "</target>\n";


/* Addresses of fault status registers in System Control Block. */
#define CFSR  0xE000ED28
#define HFSR  0xE000ED2C
#define MMFAR 0xE000ED34
#define BFAR  0xE000ED38


static RegisterContext* g_pContext;
static IMemory*         g_pMemory;
static IComm*           g_pComm;
static char             g_packetBuffer[16 * 1024];
static int              g_memoryFaultEncountered;
static int              g_shouldWaitForGdbConnect = TRUE;


/* Core MRI function not exposed in public header since typically called by ASM. */
void __mriDebugException(void);

/* Forward static function declarations. */
static uint32_t getCurrentlyExecutingExceptionNumber(void);
static void displayHardFaultCauseToGdbConsole(void);
static void displayMemFaultCauseToGdbConsole(void);
static void displayBusFaultCauseToGdbConsole(void);
static void displayUsageFaultCauseToGdbConsole(void);
static void sendRegisterForTResponse(Buffer* pBuffer, uint8_t registerOffset, uint32_t registerValue);
static void writeBytesToBufferAsHex(Buffer* pBuffer, void* pBytes, size_t byteCount);
static int hasFPURegisters();
static void readBytesFromBufferAsHex(Buffer* pBuffer, void* pBytes, size_t byteCount);


__throws void mriPlatform_Init(RegisterContext* pContext, IMemory* pMem)
{
    g_pContext = pContext;
    g_pMemory = pMem;
    g_memoryFaultEncountered = FALSE;

    __mriInit("");
}

void mriPlatform_Run(IComm* pComm)
{
    g_pComm = pComm;
    do
    {
        __mriDebugException();
    } while (!IComm_ShouldStopRun(pComm));
}

/* This routine is just used for unit testing so that it can run tests where a T response packet and/or exception
   information returned to GDB is expected.  This only happens on first mriPlatform_Run() when it doesn't need to wait 
   for GDB connection. 
*/
void mriPlatform_setWaitForGdbConnect(int setting)
{
    g_shouldWaitForGdbConnect = setting;
}




void Platform_Init(Token* pParameterTokens)
{
}

char* Platform_GetPacketBuffer(void)
{
    return g_packetBuffer;
}

uint32_t  Platform_GetPacketBufferSize(void)
{
    return sizeof(g_packetBuffer);
}

void Platform_EnteringDebugger(void)
{
    Platform_DisableSingleStep();
}

void Platform_LeavingDebugger(void)
{
}

uint32_t Platform_MemRead32(const void* pv)
{
    uint32_t retVal = 0;
    __try
        retVal = IMemory_Read32(g_pMemory, (uint32_t)(unsigned long)pv);
    __catch
        g_memoryFaultEncountered++;
    return retVal;
}

uint16_t Platform_MemRead16(const void* pv)
{
    uint16_t retVal = 0;
    __try
        retVal = IMemory_Read16(g_pMemory, (uint32_t)(unsigned long)pv);
    __catch
        g_memoryFaultEncountered++;
    return retVal;
}

uint8_t Platform_MemRead8(const void* pv)
{
    uint8_t retVal = 0;
    __try
        retVal = IMemory_Read8(g_pMemory, (uint32_t)(unsigned long)pv);
    __catch
        g_memoryFaultEncountered++;
    return retVal;
}

void Platform_MemWrite32(void* pv, uint32_t value)
{
    __try
        IMemory_Write32(g_pMemory, (uint32_t)(unsigned long)pv, value);
    __catch
        g_memoryFaultEncountered++;
}

void Platform_MemWrite16(void* pv, uint16_t value)
{
    __try
        IMemory_Write16(g_pMemory, (uint32_t)(unsigned long)pv, value);
    __catch
        g_memoryFaultEncountered++;
}

void Platform_MemWrite8(void* pv, uint8_t value)
{
    __try
        IMemory_Write8(g_pMemory, (uint32_t)(unsigned long)pv, value);
    __catch
        g_memoryFaultEncountered++;
}

uint32_t Platform_CommHasReceiveData(void)
{
    return IComm_HasReceiveData(g_pComm);
}

int Platform_CommReceiveChar(void)
{
    return IComm_ReceiveChar(g_pComm);
}

void Platform_CommSendChar(int character)
{
    IComm_SendChar(g_pComm, character);
}

int Platform_CommCausedInterrupt(void)
{
    return FALSE;
}

void Platform_CommClearInterrupt(void)
{
}

int Platform_CommShouldWaitForGdbConnect(void)
{
    return g_shouldWaitForGdbConnect;
}

int Platform_CommSharingWithApplication(void)
{
    return FALSE;
}

void Platform_CommPrepareToWaitForGdbConnection(void)
{
}

int Platform_CommIsWaitingForGdbToConnect(void)
{
    return !IComm_IsGdbConnected(g_pComm);
}

void Platform_CommWaitForReceiveDataToStop(void)
{
}

uint8_t Platform_DetermineCauseOfException(void)
{
    uint32_t exceptionNumber = getCurrentlyExecutingExceptionNumber();

    switch(exceptionNumber)
    {
    case 2:
        /* NMI */
        return SIGINT;
    case 3:
        /* HardFault */
        return SIGSEGV;
    case 4:
        /* MemManage */
        return SIGSEGV;
    case 5:
        /* BusFault */
        return SIGBUS;
    case 6:
        /* UsageFault */
        return SIGILL;
    case 12:
        /* Debug Monitor */
        return SIGTRAP;
    default:
        /* NOTE: Catch all signal will be SIGSTOP. */
        return SIGSTOP;
    }
}

static uint32_t getCurrentlyExecutingExceptionNumber(void)
{
    return (g_pContext->exceptionPSR & 0xFF);
}

void Platform_DisplayFaultCauseToGdbConsole(void)
{
    switch (getCurrentlyExecutingExceptionNumber())
    {
    case 3:
        /* HardFault */
        displayHardFaultCauseToGdbConsole();
        break;
    case 4:
        /* MemManage */
        displayMemFaultCauseToGdbConsole();
        break;
    case 5:
        /* BusFault */
        displayBusFaultCauseToGdbConsole();
        break;
    case 6:
        /* UsageFault */
        displayUsageFaultCauseToGdbConsole();
        break;
    default:
        return;
    }
    WriteStringToGdbConsole("\n");
}

static void displayHardFaultCauseToGdbConsole(void)
{
    static const uint32_t debugEventBit = 1 << 31;
    static const uint32_t forcedBit = 1 << 30;
    static const uint32_t vectorTableReadBit = 1 << 1;
    volatile uint32_t     hardFaultStatusRegister = 0;

    WriteStringToGdbConsole("\n**Hard Fault**");

    __try
        hardFaultStatusRegister = IMemory_Read32(g_pMemory, HFSR);
    __catch
        return;
    WriteStringToGdbConsole("\n  Status Register: ");
    WriteHexValueToGdbConsole(hardFaultStatusRegister);

    if (hardFaultStatusRegister & debugEventBit)
        WriteStringToGdbConsole("\n    Debug Event");

    if (hardFaultStatusRegister & vectorTableReadBit)
        WriteStringToGdbConsole("\n    Vector Table Read");

    if (hardFaultStatusRegister & forcedBit)
    {
        WriteStringToGdbConsole("\n    Forced");
        displayMemFaultCauseToGdbConsole();
        displayBusFaultCauseToGdbConsole();
        displayUsageFaultCauseToGdbConsole();
    }
}

static void displayMemFaultCauseToGdbConsole(void)
{
    static const uint32_t MMARValidBit = 1 << 7;
    static const uint32_t FPLazyStatePreservationBit = 1 << 5;
    static const uint32_t stackingErrorBit = 1 << 4;
    static const uint32_t unstackingErrorBit = 1 << 3;
    static const uint32_t dataAccess = 1 << 1;
    static const uint32_t instructionFetch = 1;
    uint32_t volatile     memManageFaultStatusRegister = 0;

    /* Check to make sure that there is a memory fault to display. */
    __try
        memManageFaultStatusRegister = IMemory_Read32(g_pMemory, CFSR) & 0xFF;
    __catch
        return;
    if (memManageFaultStatusRegister == 0)
        return;

    WriteStringToGdbConsole("\n**MPU Fault**");
    WriteStringToGdbConsole("\n  Status Register: ");
    WriteHexValueToGdbConsole(memManageFaultStatusRegister);

    if (memManageFaultStatusRegister & MMARValidBit)
    {
        __try
        {
            uint32_t memManageFaultAddressRegister = IMemory_Read32(g_pMemory, MMFAR);
            WriteStringToGdbConsole("\n    Fault Address: ");
            WriteHexValueToGdbConsole(memManageFaultAddressRegister);
        }
        __catch
        {
        }
    }
    if (memManageFaultStatusRegister & FPLazyStatePreservationBit)
        WriteStringToGdbConsole("\n    FP Lazy Preservation");
    if (memManageFaultStatusRegister & stackingErrorBit)
        WriteStringToGdbConsole("\n    Stacking Error");
    if (memManageFaultStatusRegister & unstackingErrorBit)
        WriteStringToGdbConsole("\n    Unstacking Error");
    if (memManageFaultStatusRegister & dataAccess)
        WriteStringToGdbConsole("\n    Data Access");
    if (memManageFaultStatusRegister & instructionFetch)
        WriteStringToGdbConsole("\n    Instruction Fetch");
}

static void displayBusFaultCauseToGdbConsole(void)
{
    static const uint32_t BFARValidBit = 1 << 7;
    static const uint32_t FPLazyStatePreservationBit = 1 << 5;
    static const uint32_t stackingErrorBit = 1 << 4;
    static const uint32_t unstackingErrorBit = 1 << 3;
    static const uint32_t impreciseDataAccessBit = 1 << 2;
    static const uint32_t preciseDataAccessBit = 1 << 1;
    static const uint32_t instructionPrefetch = 1;
    uint32_t volatile     busFaultStatusRegister = 0;

    __try
        busFaultStatusRegister = (IMemory_Read32(g_pMemory, CFSR) >> 8) & 0xFF;
    __catch
        return;

    /* Check to make sure that there is a bus fault to display. */
    if (busFaultStatusRegister == 0)
        return;

    WriteStringToGdbConsole("\n**Bus Fault**");
    WriteStringToGdbConsole("\n  Status Register: ");
    WriteHexValueToGdbConsole(busFaultStatusRegister);

    if (busFaultStatusRegister & BFARValidBit)
    {
        __try
        {
            uint32_t busFaultAddressRegister = IMemory_Read32(g_pMemory, BFAR);
            WriteStringToGdbConsole("\n    Fault Address: ");
            WriteHexValueToGdbConsole(busFaultAddressRegister);
        }
        __catch
        {
        }
    }
    if (busFaultStatusRegister & FPLazyStatePreservationBit)
        WriteStringToGdbConsole("\n    FP Lazy Preservation");
    if (busFaultStatusRegister & stackingErrorBit)
        WriteStringToGdbConsole("\n    Stacking Error");
    if (busFaultStatusRegister & unstackingErrorBit)
        WriteStringToGdbConsole("\n    Unstacking Error");
    if (busFaultStatusRegister & impreciseDataAccessBit)
        WriteStringToGdbConsole("\n    Imprecise Data Access");
    if (busFaultStatusRegister & preciseDataAccessBit)
        WriteStringToGdbConsole("\n    Precise Data Access");
    if (busFaultStatusRegister & instructionPrefetch)
        WriteStringToGdbConsole("\n    Instruction Prefetch");
}

static void displayUsageFaultCauseToGdbConsole(void)
{
    static const uint32_t divideByZeroBit = 1 << 9;
    static const uint32_t unalignedBit = 1 << 8;
    static const uint32_t coProcessorAccessBit = 1 << 3;
    static const uint32_t invalidPCBit = 1 << 2;
    static const uint32_t invalidStateBit = 1 << 1;
    static const uint32_t undefinedInstructionBit = 1;
    volatile uint32_t     usageFaultStatusRegister = 0;

    /* Make sure that there is a usage fault to display. */
    __try
        usageFaultStatusRegister = (IMemory_Read32(g_pMemory, CFSR) >> 16) & 0xFFFF;
    __catch
        return;
    if (usageFaultStatusRegister == 0)
        return;

    WriteStringToGdbConsole("\n**Usage Fault**");
    WriteStringToGdbConsole("\n  Status Register: ");
    WriteHexValueToGdbConsole(usageFaultStatusRegister);

    if (usageFaultStatusRegister & divideByZeroBit)
        WriteStringToGdbConsole("\n    Divide by Zero");
    if (usageFaultStatusRegister & unalignedBit)
        WriteStringToGdbConsole("\n    Unaligned Access");
    if (usageFaultStatusRegister & coProcessorAccessBit)
        WriteStringToGdbConsole("\n    Coprocessor Access");
    if (usageFaultStatusRegister & invalidPCBit)
        WriteStringToGdbConsole("\n    Invalid Exception Return State");
    if (usageFaultStatusRegister & invalidStateBit)
        WriteStringToGdbConsole("\n    Invalid State");
    if (usageFaultStatusRegister & undefinedInstructionBit)
        WriteStringToGdbConsole("\n    Undefined Instruction");
}

void Platform_EnableSingleStep(void)
{
}

void Platform_DisableSingleStep(void)
{
}

int Platform_IsSingleStepping(void)
{
    /* Can't really single step so just return FALSE here. */
    return FALSE;
}

void Platform_SetProgramCounter(uint32_t newPC)
{
    /* This is just called when user tries to set new PC value from continue command which we don't really support
       so just ignore this request. */
}

void Platform_AdvanceProgramCounterToNextInstruction(void)
{
    /* This is just called to advance past hardcoded breakpoints before continuing execution so just ignore. */
}

int Platform_WasProgramCounterModifiedByUser(void)
{
    /* Pretend that PC was always modified so that mriCore doesn't check for hardcoded breakpoint on resume. */
    return TRUE;
}

int Platform_WasMemoryFaultEncountered(void)
{
    int memoryFaultEncountered = g_memoryFaultEncountered;
    g_memoryFaultEncountered = 0;
    return memoryFaultEncountered;
}

void Platform_WriteTResponseRegistersToBuffer(Buffer* pBuffer)
{
    sendRegisterForTResponse(pBuffer, 12, g_pContext->R[12]);
    sendRegisterForTResponse(pBuffer, 13, g_pContext->R[13]);
    sendRegisterForTResponse(pBuffer, 14, g_pContext->R[14]);
    sendRegisterForTResponse(pBuffer, 15, g_pContext->R[15]);
}

static void sendRegisterForTResponse(Buffer* pBuffer, uint8_t registerOffset, uint32_t registerValue)
{
    Buffer_WriteByteAsHex(pBuffer, registerOffset);
    Buffer_WriteChar(pBuffer, ':');
    writeBytesToBufferAsHex(pBuffer, &registerValue, sizeof(registerValue));
    Buffer_WriteChar(pBuffer, ';');
}

static void writeBytesToBufferAsHex(Buffer* pBuffer, void* pBytes, size_t byteCount)
{
    uint8_t* pByte = (uint8_t*)pBytes;
    size_t   i;

    for (i = 0 ; i < byteCount ; i++)
        Buffer_WriteByteAsHex(pBuffer, *pByte++);
}

void Platform_CopyContextToBuffer(Buffer* pBuffer)
{
    writeBytesToBufferAsHex(pBuffer, g_pContext->R, sizeof(g_pContext->R));
    if (hasFPURegisters())
        writeBytesToBufferAsHex(pBuffer, g_pContext->FPR, sizeof(g_pContext->FPR));
}

static int hasFPURegisters()
{
    return g_pContext->flags & CRASH_CATCHER_FLAGS_FLOATING_POINT;
}

void Platform_CopyContextFromBuffer(Buffer* pBuffer)
{
    readBytesFromBufferAsHex(pBuffer, g_pContext->R, sizeof(g_pContext->R));
    if (hasFPURegisters())
        readBytesFromBufferAsHex(pBuffer, g_pContext->FPR, sizeof(g_pContext->FPR));
}

static void readBytesFromBufferAsHex(Buffer* pBuffer, void* pBytes, size_t byteCount)
{
    uint8_t* pByte = (uint8_t*)pBytes;
    size_t   i;

    for (i = 0 ; i < byteCount; i++)
        *pByte++ = Buffer_ReadByteAsHex(pBuffer);
}

uint32_t Platform_GetDeviceMemoryMapXmlSize(void)
{
    return strlen(MemorySim_GetMemoryMapXML(g_pMemory));
}

const char* Platform_GetDeviceMemoryMapXml(void)
{
    return MemorySim_GetMemoryMapXML(g_pMemory);
}

uint32_t Platform_GetTargetXmlSize(void)
{
    if (hasFPURegisters())
        return sizeof(g_targetFpuXML) - 1;
    else
        return sizeof(g_targetXML) - 1;
}

const char* Platform_GetTargetXml(void)
{
    if (hasFPURegisters())
        return g_targetFpuXML;
    else
        return g_targetXML;
}

__throws void Platform_SetHardwareBreakpoint(uint32_t address, uint32_t kind)
{
    /* Ignore during post-mortem debugging. */
}

__throws void Platform_ClearHardwareBreakpoint(uint32_t address, uint32_t kind)
{
    /* Ignore during post-mortem debugging. */
}

__throws void Platform_SetHardwareWatchpoint(uint32_t address, uint32_t size, PlatformWatchpointType type)
{
    /* Ignore during post-mortem debugging. */
}

__throws void Platform_ClearHardwareWatchpoint(uint32_t address, uint32_t size,  PlatformWatchpointType type)
{
    /* Ignore during post-mortem debugging. */
}

PlatformInstructionType Platform_TypeOfCurrentInstruction(void)
{
    /* Ignore semihost and hard coded breakpoint instructions during post-mortem debugging. */
    return MRI_PLATFORM_INSTRUCTION_OTHER;
}

PlatformSemihostParameters Platform_GetSemihostCallParameters(void)
{
    /* Ignore during post-mortem debugging. */
    PlatformSemihostParameters parameters;
    memset(&parameters, 0, sizeof(parameters));
    return parameters;
}

void Platform_SetSemihostCallReturnAndErrnoValues(int returnValue, int err)
{
    /* Ignore during post-mortem debugging. */
}

int Semihost_IsDebuggeeMakingSemihostCall(void)
{
    /* Ignore semihost instructions during post-mortem debugging. */
    return FALSE;
}

int Semihost_HandleSemihostRequest(void)
{
    /* Ignore semihost requests during post-mortem debugging. */
    return FALSE;
}

void __mriPlatform_EnteringDebuggerHook(void)
{
}

void __mriPlatform_LeavingDebuggerHook(void)
{
}
