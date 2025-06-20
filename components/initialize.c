//
// Created by porte on 6/20/2025.
//

#include "initialize.h"

#include "disk.h"
#include "utilities.h"


VOID initLists()
{
    ULONG64 count = physical_page_count;  // how many pages AllocateUserPhysicalPages actually returned

    pageTable = malloc(VIRTUAL_ADDRESS_SIZE / PAGE_SIZE * sizeof(PageTableEntry));
    memset(pageTable, 0, VIRTUAL_ADDRESS_SIZE / PAGE_SIZE * sizeof(PageTableEntry));

    // build PFN list only from actually owned pages
    pfnArray = malloc(count * sizeof(Frame));
    freeList = NULL;
    activeList = NULL;

    for (ULONG64 i = 0; i < count; ++i) {
        pfnArray[i].physicalFrameNumber = physical_page_numbers[i];
        pfnArray[i].nextPFN = freeList;
        freeList = &pfnArray[i];
    }
}

VOID initDiskSpace()
{
    totalDiskSpace = malloc(VIRTUAL_ADDRESS_SIZE);
    freeDiskSpace = malloc(NUMBER_OF_VIRTUAL_PAGES * sizeof(*freeDiskSpace));

    for (ULONG64 i = 0; i < NUMBER_OF_VIRTUAL_PAGES; i++) {
        freeDiskSpace[i] = TRUE;
    }
}