//
// Created by porte on 6/20/2025.
//

#include "initialize.h"

#include <stdio.h>

#include "disk.h"
#include "utilities.h"


VOID initListsAndPFNs()
{
    ULONG64 numBytes = VIRTUAL_ADDRESS_SIZE / PAGE_SIZE * sizeof(PageTableEntry);

    pageTable = malloc(numBytes);
    memset(pageTable, 0, numBytes);

    // Find the highest PFN in the PFN array.
    ULONG64 maxPFN = 0;
    for (ULONG64 i = 0; i < physical_page_count; ++i) {
        if (physical_page_numbers[i] > maxPFN) {
            maxPFN = physical_page_numbers[i];
        }
    }

    ULONG64 pfnArraySize = (maxPFN + 1) * sizeof(Frame);

    // build PFN list only from actually owned pages
    pfnArray = VirtualAlloc(
        NULL,
        pfnArraySize,
        MEM_RESERVE,
        PAGE_READWRITE);

    freeList = NULL;
    activeList = NULL;
    modifiedList = NULL;
    standbyList = NULL;

    // For each physical page we have, commit the memory for it in the sparse arary.
    for (ULONG64 i = 0; i < physical_page_count; i++)
    {
        ULONG64 currentFrameNumber = physical_page_numbers[i];

        // Go through our pfnArray and actually commit where we have frames.
        VirtualAlloc (
            pfnArray + currentFrameNumber,
            sizeof(Frame),
            MEM_COMMIT,
            PAGE_READWRITE);

        pfnArray[currentFrameNumber].nextPFN = freeList;
        freeList = &pfnArray[currentFrameNumber];
        pfnArray[currentFrameNumber].PTE = NULL;
        pfnArray[currentFrameNumber].isOnModifiedList = 0;
    }
}

VOID initDiskSpace()
{
    transferVA = VirtualAlloc (NULL,
                      PAGE_SIZE * MAX_WRITE_PAGES,
                      MEM_RESERVE | MEM_PHYSICAL,
                      PAGE_READWRITE);

    totalDiskSpace = malloc(VIRTUAL_ADDRESS_SIZE);
    freeDiskSpace = malloc(NUMBER_OF_VIRTUAL_PAGES * sizeof(*freeDiskSpace));
    lastUsedFreeDiskSlot = 0;

    for (ULONG64 i = 0; i < NUMBER_OF_VIRTUAL_PAGES; i++)
    {
        freeDiskSpace[i] = TRUE;
    }

    numAttemptedModWrites = 0;
}