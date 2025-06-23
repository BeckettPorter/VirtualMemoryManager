//
// Created by porte on 6/20/2025.
//

#include "disk.h"

#include <stdbool.h>
#include <stdio.h>


VOID swapToDisk(PageTableEntry* pageToSwap)
{
    if (pageToSwap == NULL) {
        printf("swapToDisk: NULL pageToSwap passed\n");
        return;
    }

    ULONG64 frameNumber = pageToSwap->transitionFormat.pageFrameNumber;

    if (MapUserPhysicalPages (transferVA, 1, &frameNumber) == FALSE) {
        printf ("swapToDisk : could not map VA %p to page %llX\n", transferVA,
            frameNumber);

        // Don't return - try to continue with the operation
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
        exit(-1);
    }

    memcpy(totalDiskSpace + freeDiskSlot * PAGE_SIZE, transferVA, PAGE_SIZE);
    // Set the disk space we just copied to to false so we know it is in use.
    freeDiskSpace[freeDiskSlot] = false;


    pageToSwap->transitionFormat.disk_index = freeDiskSlot;


    if (!MapUserPhysicalPages(transferVA, 1, NULL)) {
        printf("swapToDisk: Failed to map user physical pages!");
        exit(-1);
    }
}

VOID swapFromDisk(Frame* frameToFill)
{
    if (frameToFill == NULL || frameToFill->PTE == NULL) {
        printf("swapFromDisk: NULL frameToFill or PTE passed\n");
        return;
    }

    // Use same transfer VA for now, multithreaded might need a different one
    ULONG64 frameNumber = frameToFill->physicalFrameNumber;

    if (MapUserPhysicalPages (transferVA, 1, &frameNumber) == FALSE) {
        printf ("swapFromDisk : could not map VA %p to page %llX\n", transferVA,
            frameNumber);

        // Continue execution rather than returning
    }

    ULONG64 diskIndexToTransferFrom = frameToFill->PTE->transitionFormat.disk_index;

    memcpy(transferVA, totalDiskSpace + diskIndexToTransferFrom * PAGE_SIZE, PAGE_SIZE);
    // Set the disk space we just copied from to true to clear it from being used.
    freeDiskSpace[diskIndexToTransferFrom] = true;


    if (!MapUserPhysicalPages(transferVA, 1, NULL)) {
        printf("swapToDisk: Failed to map user physical pages!");
        exit(-1);
    }
}
