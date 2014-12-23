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
#include <common.h>
#include <ctype.h>
#include <GdbLogParser.h>
#include <FileFailureInject.h>
#include <stdio.h>
#include <string.h>

typedef struct ParseObject
{
    IMemory*         pMem;
    RegisterContext* pContext;
    uint32_t         regionStart;
    uint32_t         regionSize;
    uint32_t         nextExpectedAddress;
    char             lineText[1024];
} ParseObject;

typedef enum ParseType
{
    TYPE_OTHER,
    TYPE_MEMORY,
    TYPE_REGISTER
} ParseType;

typedef struct ParseResults
{
    union
    {
        struct
        {
            uint32_t  address;
            uint32_t  values[4];
            uint32_t  valueCount;
        };
        struct
        {
            size_t   registerIndex;
            uint32_t registerValue;
        };
    };
    ParseType type;
} ParseResults;


static FILE* openFileAndThrowOnError(const char* pLogFilename);
static void firstPassHandler(ParseObject* pObject, const ParseResults* pParseResults);
static void firstPassMemoryHandler(ParseObject* pObject, const ParseResults* pParseResults);
static void secondPassHandler(ParseObject* pObject, const ParseResults* pParseResults);
static void parseLines(IMemory* pMem,
                       RegisterContext* pContext,
                       FILE* pLogFile,
                       void (*passHandler)(ParseObject*, const ParseResults*));
static ParseResults parseLine(const char* pLine);
static int isMemoryLine(const char* pLine);
static int is8DigitHexValue(const char* pLine);
static int isHexDigit(char c);
static ParseResults parseMemoryLine(const char* pLine);
static const char* parseAddress(ParseResults* pResults, const char* pLine);
static int areMoreValuesToParseOnLine(const ParseResults* pResults, const char* pLine);
static const char* skipWhitespaceSymbolsAndParseNextValue(ParseResults* pResults, const char* pLine);
static const char* skipWhitespaceAndSymbol(const char* pLine);
static const char* skipWhitespace(const char* pLine);
static const char* skipSymbol(const char* pLine);
static const char* parseValue(ParseResults* pResults, const char* pLine);
static int isRegisterLine(const char* pLine, size_t* pRegisterIndex);
static ParseResults parseRegisterLine(const char* pLine, size_t registerIndex);
static ParseResults parseOtherLine(const char* pLine);
static void rewindLogFileAndThrowOnError(FILE* pLogFile);


__throws void GdbLogParse(IMemory* pMem, RegisterContext* pContext, const char* pLogFilename)
{
    FILE* volatile pLogFile = NULL;

    __try
    {
        pLogFile = openFileAndThrowOnError(pLogFilename);
        parseLines(pMem, pContext, pLogFile, firstPassHandler);
        rewindLogFileAndThrowOnError(pLogFile);
        parseLines(pMem, pContext, pLogFile, secondPassHandler);
        fclose(pLogFile);
    }
    __catch
    {
        if (pLogFile)
            fclose(pLogFile);
        __rethrow;
    }
}

static FILE* openFileAndThrowOnError(const char* pLogFilename)
{
    FILE* pLogFile = fopen(pLogFilename, "r");
    if (!pLogFile)
        __throw(fileException);
    return pLogFile;
}

static void firstPassHandler(ParseObject* pObject, const ParseResults* pParseResults)
{
    switch (pParseResults->type)
    {
    case TYPE_MEMORY:
        firstPassMemoryHandler(pObject, pParseResults);
        break;
    case TYPE_REGISTER:
        pObject->pContext->R[pParseResults->registerIndex] = pParseResults->registerValue;
        break;
    case TYPE_OTHER:
    default:
        break;
    }
}

static void firstPassMemoryHandler(ParseObject* pObject, const ParseResults* pParseResults)
{
    if (pParseResults->address != pObject->nextExpectedAddress)
    {
        if (pObject->regionStart != 0xFFFFFFFF)
            MemorySim_CreateRegion(pObject->pMem, pObject->regionStart, pObject->regionSize);
        pObject->regionStart = pParseResults->address;
        pObject->regionSize = 0;
    }
    pObject->regionSize += pParseResults->valueCount * sizeof(uint32_t);
    pObject->nextExpectedAddress = pParseResults->address + pParseResults->valueCount * sizeof(uint32_t);
}

static void secondPassHandler(ParseObject* pObject, const ParseResults* pParseResults)
{
    uint32_t i;
    if (pParseResults->type != TYPE_MEMORY)
        return;
    for (i = 0 ; i < pParseResults->valueCount ; i++)
        IMemory_Write32(pObject->pMem, pParseResults->address + i * sizeof(uint32_t), pParseResults->values[i]);
}

static void parseLines(IMemory* pMem,
                       RegisterContext* pContext,
                       FILE* pLogFile,
                       void (*passHandler)(ParseObject*, const ParseResults*))
{
    ParseObject object;
    object.pMem = pMem;
    object.pContext = pContext;
    object.regionStart = 0xFFFFFFFF;
    object.regionSize = 0;
    object.nextExpectedAddress = 0xFFFFFFFF;

    while (NULL != fgets(object.lineText, sizeof(object.lineText), pLogFile))
    {
        ParseResults parseResults = parseLine(object.lineText);
        passHandler(&object, &parseResults);
    }
    if (object.regionSize != 0)
        MemorySim_CreateRegion(object.pMem, object.regionStart, object.regionSize);
}

static ParseResults parseLine(const char* pLine)
{
    size_t registerIndex = 0;
    if (isMemoryLine(pLine))
        return parseMemoryLine(pLine);
    else if (isRegisterLine(pLine, &registerIndex))
        return parseRegisterLine(pLine, registerIndex);
    else
        return parseOtherLine(pLine);
}

static int isMemoryLine(const char* pLine)
{
    return (strlen(pLine) > 11 && is8DigitHexValue(pLine));
}

static int is8DigitHexValue(const char* pLine)
{
    return (pLine[0] == '0' &&
            pLine[1] == 'x' &&
            isHexDigit(pLine[2]) &&
            isHexDigit(pLine[3]) &&
            isHexDigit(pLine[4]) &&
            isHexDigit(pLine[5]) &&
            isHexDigit(pLine[6]) &&
            isHexDigit(pLine[7]) &&
            isHexDigit(pLine[8]) &&
            isHexDigit(pLine[9]));
}

static int isHexDigit(char c)
{
    return ((c >= '0' && c <= '9') ||
            (c >= 'a' && c <= 'f') ||
            (c >= 'A' && c <= 'F'));
}

static ParseResults parseMemoryLine(const char* pLine)
{
    ParseResults results;

    results.type = TYPE_MEMORY;
    results.valueCount = 0;
    pLine = parseAddress(&results, pLine);
    while (areMoreValuesToParseOnLine(&results, pLine))
        pLine = skipWhitespaceSymbolsAndParseNextValue(&results, pLine);
    return results;
}

static const char* parseAddress(ParseResults* pResults, const char* pLine)
{
    pResults->address = strtoul(pLine, NULL, 0);
    // Skip 0xXXXXXXXX: address formatted string.
    return pLine + (2 + 8 + 1);
}

static int areMoreValuesToParseOnLine(const ParseResults* pResults, const char* pLine)
{
    return (*pLine && pResults->valueCount < ARRAY_SIZE(pResults->values));
}

static const char* skipWhitespaceSymbolsAndParseNextValue(ParseResults* pResults, const char* pLine)
{
    pLine = skipWhitespaceAndSymbol(pLine);
    pLine = parseValue(pResults, pLine);
    return pLine;
}

static const char* skipWhitespaceAndSymbol(const char* pLine)
{
    pLine = skipWhitespace(pLine);
    pLine = skipSymbol(pLine);
    pLine = skipWhitespace(pLine);
    return pLine;
}

static const char* skipWhitespace(const char* pLine)
{
    while (*pLine && isspace(*pLine))
        pLine++;
    return pLine;
}

static const char* skipSymbol(const char* pLine)
{
    int nestingLevel = 0;

    if (*pLine != '<')
        return pLine;
    while (*pLine)
    {
        if (*pLine == '<')
            nestingLevel++;
        else if (*pLine == '>')
            nestingLevel--;
        else if (isspace(*pLine) && nestingLevel == 0)
            break;
        pLine++;
    }
    return pLine;

}

static const char* parseValue(ParseResults* pResults, const char* pLine)
{
    char* pNext = NULL;
    pResults->values[pResults->valueCount++] = strtoul(pLine, &pNext, 0);
    return pNext;
}

static int isRegisterLine(const char* pLine, size_t* pRegisterIndex)
{
    size_t i;
    struct
    {
        const char* pName;
        size_t      index;
    } registers[] =
    {
        { "r0             ", R0 },
        { "r1             ", R1 },
        { "r2             ", R2 },
        { "r3             ", R3 },
        { "r4             ", R4 },
        { "r5             ", R5 },
        { "r6             ", R6 },
        { "r7             ", R7 },
        { "r8             ", R8 },
        { "r9             ", R9 },
        { "r10            ", R10 },
        { "r11            ", R11 },
        { "r12            ", R12 },
        { "sp             ", SP },
        { "lr             ", LR },
        { "pc             ", PC },
        { "xpsr           ", XPSR }
    };

    for (i = 0 ; i < ARRAY_SIZE(registers) ; i++)
    {
        if (0 == strncmp(registers[i].pName, pLine, 15))
        {
            *pRegisterIndex = registers[i].index;
            return TRUE;
        }
    }
    return FALSE;
}

static ParseResults parseRegisterLine(const char* pLine, size_t registerIndex)
{
    ParseResults results;
    results.type = TYPE_REGISTER;
    results.registerIndex = registerIndex;

    // Register value is found after 15 character long register name field.
    results.registerValue = strtoul(&pLine[15], NULL, 0);
    return results;
}

static ParseResults parseOtherLine(const char* pLine)
{
    ParseResults results;
    results.type = TYPE_OTHER;
    return results;
}

static void rewindLogFileAndThrowOnError(FILE* pLogFile)
{
    int seekResult = fseek(pLogFile, SEEK_SET, 0);
    if (seekResult == -1)
        __throw(fileException);
}
