/*  Copyright (C) 2018  Adam Green (https://github.com/adamgreen)

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*/
#include <ElfLoad.h>
#include "ElfPriv.h"
#include <MemorySim.h>
#include <string.h>

typedef struct
{
    const char* pBlob;
    size_t      blobSize;
} SizedBlob;

typedef struct
{
    const Elf32_Ehdr* pElfHeader;
    SizedBlob         sizedBlob;
    uint32_t          entryLoadCount;
    Elf32_Off         pgmHeaderOffset;
} LoadObject;

static LoadObject initLoadObject(const char* pBlob, size_t blobSize);
static SizedBlob initSizedBlob(const char* pBlob, size_t blobSize);
static const void* fetchSizedByteArray(const SizedBlob* pBlob, uint32_t offset, uint32_t size);
static void validateElfHeaderContents(const Elf32_Ehdr* pHeader);
static void loadFlashLoadableEntries(IMemory* pMemory, LoadObject* pObject);
static void loadIfFlashLoadableEntry(IMemory* pMemory, LoadObject* pObject, const Elf32_Phdr* pPgmHeader);
static int isFlashLoadableEntry(const Elf32_Phdr* pHeader);


__throws void ElfLoad_FromMemory(IMemory* pMemory, const void* pElf, size_t elfSize)
{
    LoadObject object = initLoadObject(pElf, elfSize);

    validateElfHeaderContents(object.pElfHeader);
    loadFlashLoadableEntries(pMemory, &object);
    if (object.entryLoadCount == 0)
        __throw(elfFormatException);
}

static LoadObject initLoadObject(const char* pBlob, size_t blobSize)
{
    LoadObject object;

    memset(&object, 0, sizeof(object));
    object.sizedBlob = initSizedBlob(pBlob, blobSize);
    object.pElfHeader = fetchSizedByteArray(&object.sizedBlob, 0, sizeof(*object.pElfHeader));
    return object;
}

static SizedBlob initSizedBlob(const char* pBlob, size_t blobSize)
{
    SizedBlob blob = { pBlob, blobSize };
    return blob;
}

static const void* fetchSizedByteArray(const SizedBlob* pBlob, uint32_t offset, uint32_t size)
{
    if (offset > pBlob->blobSize || (offset + size) > pBlob->blobSize)
        __throw(elfFormatException);
    return (void*)(pBlob->pBlob + offset);
}

static void validateElfHeaderContents(const Elf32_Ehdr* pHeader)
{
    const unsigned char expectedIdent[4] = { ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3 };
    if (memcmp(pHeader->e_ident, expectedIdent, sizeof(expectedIdent)) != 0 ||
        pHeader->e_ident[EI_CLASS] != ELFCLASS32 ||
        pHeader->e_ident[EI_DATA] != ELFDATA2LSB ||
        pHeader->e_type != ET_EXEC ||
        pHeader->e_phoff == 0 ||
        pHeader->e_phnum == 0 ||
        pHeader->e_phentsize < sizeof(Elf32_Phdr))
    {
        __throw(elfFormatException);
    }
}

static void loadFlashLoadableEntries(IMemory* pMemory, LoadObject* pObject)
{
    const Elf32_Phdr*   pPgmHeader = NULL;
    Elf32_Half          i = 0;

    for (i = 0, pObject->pgmHeaderOffset = pObject->pElfHeader->e_phoff ; i < pObject->pElfHeader->e_phnum ; i++)
    {
        pPgmHeader = fetchSizedByteArray(&pObject->sizedBlob, pObject->pgmHeaderOffset, sizeof(*pPgmHeader));
        loadIfFlashLoadableEntry(pMemory, pObject, pPgmHeader);
        pObject->pgmHeaderOffset += pObject->pElfHeader->e_phentsize;
    }
}

static void loadIfFlashLoadableEntry(IMemory* pMemory, LoadObject* pObject, const Elf32_Phdr* pPgmHeader)
{
    const void* pData = NULL;

    if (!isFlashLoadableEntry(pPgmHeader))
        return;

    pData = fetchSizedByteArray(&pObject->sizedBlob, pPgmHeader->p_offset, pPgmHeader->p_filesz);
    MemorySim_CreateRegion(pMemory, pPgmHeader->p_paddr, pPgmHeader->p_filesz);
    MemorySim_LoadFromFlashImage(pMemory, pPgmHeader->p_paddr, pData, pPgmHeader->p_filesz);
    MemorySim_MakeRegionReadOnly(pMemory, pPgmHeader->p_paddr);
    pObject->entryLoadCount++;
}

static int isFlashLoadableEntry(const Elf32_Phdr* pHeader)
{
    return (pHeader->p_type == PT_LOAD &&
            pHeader->p_filesz != 0 &&
            pHeader->p_memsz >= pHeader->p_filesz);
}