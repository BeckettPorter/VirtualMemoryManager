//
// Created by porte on 6/20/2025.
//

#include "disk.h"

#include <stdbool.h>
#include <stdio.h>

#include "pages.h"

// In future, can add number of pages to swap here and pass array
VOID swapToDisk(PageTableEntry* pageToSwap)
{
    ULONG64 frameNumber = pageToSwap->transitionFormat.pageFrameNumber;

    Frame* frameToRemove = findFrameFromFrameNumber(frameNumber);

    modifiedList = removeFromList(modifiedList, frameToRemove);
    frameToRemove->isOnModifiedList = 0;

    if (MapUserPhysicalPages (transferVA, 1, &frameNumber) == FALSE) {
        printf ("swapToDisk : could not map VA %p to page %llX\n", transferVA,
            frameNumber);

        return;
    }

    ULONG64 freeDiskSlot = 0;

    // Go through our boolean array to find free a free disk slot.
    while (freeDiskSlot < NUMBER_OF_VIRTUAL_PAGES && !freeDiskSpace[freeDiskSlot])
    {
        freeDiskSlot++;
    }
    // 1 to 1, fix later
    if (freeDiskSlot == NUMBER_OF_VIRTUAL_PAGES)
    {
        printf("swapToDisk: No free disk slots!");
        // exit(-1);
    }

    memcpy(totalDiskSpace + freeDiskSlot * PAGE_SIZE, transferVA, PAGE_SIZE);
    // Set the disk space we just copied to to false so we know it is in use.
    freeDiskSpace[freeDiskSlot] = false;


    findFrameFromFrameNumber(pageToSwap->transitionFormat.pageFrameNumber)->diskIndex = freeDiskSlot;

    pageToSwap->transitionFormat.isTransitionFormat = 0;


    if (!MapUserPhysicalPages(transferVA, 1, NULL)) {
        printf("swapToDisk: Failed to map user physical pages!");
        exit(-1);
    }
}

VOID swapFromDisk(Frame* frameToFill, ULONG64 diskIndexToTransferFrom)
{
    // Use same transfer VA for now, multithreaded might need a different one
    ULONG64 frameNumber = frameToFill->physicalFrameNumber;

    if (MapUserPhysicalPages (transferVA, 1, &frameNumber) == FALSE) {
        printf ("swapFromDisk : could not map VA %p to page %llX\n", transferVA,
            frameNumber);

        return;
    }

    // If the slot in disk space we are trying to fill from is not already in use, debug break
    // because the contents should be there.
    if (freeDiskSpace[diskIndexToTransferFrom] == true)
    {
        DebugBreak();
    }

    memcpy(transferVA, totalDiskSpace + diskIndexToTransferFrom * PAGE_SIZE, PAGE_SIZE);
    // Set the disk space we just copied from to true to clear it from being used.
    freeDiskSpace[diskIndexToTransferFrom] = true;


    if (!MapUserPhysicalPages(transferVA, 1, NULL)) {
        printf("swapFromDisk: Failed to map user physical pages!");
        exit(-1);
    }
}
