//
// Created by porte on 6/20/2025.
//

#include "disk.h"

#include <stdbool.h>
#include <stdio.h>


VOID swapToDisk(PageTableEntry* pageToSwap)
{

    void* transferVA = PageTableEntryToVA(pageToSwap);

    ULONG64 freeDiskSlot = 0;

    // Go through our boolean array to find free a free disk slot.
    while (freeDiskSlot < NUMBER_OF_VIRTUAL_PAGES && !freeDiskSpace[freeDiskSlot])
    {
        ++freeDiskSlot;
    }
    if (freeDiskSlot == NUMBER_OF_VIRTUAL_PAGES)
    {
        printf("swapToDisk: No free disk slots!");
        exit(-1);
    }

    memcpy(totalDiskSpace + freeDiskSlot * PAGE_SIZE, transferVA, PAGE_SIZE);
    // Set the disk space we just copied to to false so we know it is in use.
    freeDiskSpace[freeDiskSlot] = false;


    pageToSwap->disk_index = freeDiskSlot;
    pageToSwap->isValid = false;
    pageToSwap->pageFrameNumber = -1;


    if (!MapUserPhysicalPages(transferVA, 1, NULL)) {
        printf("swapToDisk: Failed to map user physical pages!");
        exit(-1);
    }

}