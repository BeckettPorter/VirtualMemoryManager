//
// Created by porte on 6/20/2025.
//

#include "initialize.h"

#include <stdio.h>

#include "disk.h"
#include "utilities.h"


VOID initLists()
{
    ULONG64 count = physical_page_count;  // how many pages AllocateUserPhysicalPages actually returned


    ULONG64 numBytes = VIRTUAL_ADDRESS_SIZE / PAGE_SIZE * sizeof(PageTableEntry);

    pageTable = malloc(numBytes);
    memset(pageTable, 0, numBytes);

    // build PFN list only from actually owned pages
    pfnArray = malloc(count * sizeof(Frame));
    freeList = NULL;
    activeList = NULL;
    modifiedList = NULL;
    standbyList = NULL;

    for (ULONG64 i = 0; i < count; ++i) {
        pfnArray[i].physicalFrameNumber = physical_page_numbers[i];
        pfnArray[i].nextPFN = freeList;
        freeList = &pfnArray[i];
        pfnArray[i].PTE = NULL;
        pfnArray[i].isOnModifiedList = 0;
    }
}

VOID initFrameMap()
{
    ULONG64 maxPFN = 0;
    for (ULONG64 i = 0; i < physical_page_count; ++i) {
        if (pfnArray[i].physicalFrameNumber > maxPFN) {
            maxPFN = pfnArray[i].physicalFrameNumber;
        }
    }

    frameMapSize = (maxPFN + 1) * sizeof(Frame);

    // Allocate the entire space we could have frames, we will then only commit the actual ones we need.
    frameMap = VirtualAlloc(
        NULL,
        frameMapSize,
        MEM_RESERVE,
        PAGE_READWRITE);


    for (ULONG64 i = 0; i < physical_page_count; i++)
    {
        ULONG64 currentFrameNumber = pfnArray[i].physicalFrameNumber;

        // Go through our frameMap array and actually commit where we have frames.
        VirtualAlloc (
            frameMap + currentFrameNumber,
            sizeof(Frame),
            MEM_COMMIT,
            PAGE_READWRITE);
    }


}

VOID initDiskSpace()
{
    transferVA = VirtualAlloc (NULL,
                      PAGE_SIZE,
                      MEM_RESERVE | MEM_PHYSICAL,
                      PAGE_READWRITE);

    totalDiskSpace = malloc(VIRTUAL_ADDRESS_SIZE);
    freeDiskSpace = malloc(NUMBER_OF_VIRTUAL_PAGES * sizeof(*freeDiskSpace));

    diskSlotsArrayList.flink = &diskSlotsArrayList;
    diskSlotsArrayList.blink = &diskSlotsArrayList;

    for (ULONG64 i = 0; i < NUMBER_OF_VIRTUAL_PAGES; i++)
    {
        freeDiskSpace[i] = TRUE;

        ULONG64Node* node = createNode(i);
        if (!node) {
            printf("Memory allocation failed for ULONG64Node!\n");
            exit(-1);
        }

        addListEntry(&diskSlotsArrayList, &node->listEntry);  // Fix: pass &node->listEntry
    }
}