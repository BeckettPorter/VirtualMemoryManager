//
// Created by porte on 6/20/2025.
//

#include "disk.h"

#include <stdbool.h>
#include <stdio.h>

#include "pages.h"

// In future, can add number of pages to swap here and pass array
VOID swapToDisk()
{
    ULONG64 diskSlotsToBatch[MAX_WRITE_PAGES];

    // Check my modified list, I want to write all of those out in one batch
    // But first check if we have enough disk slots to do this, otherwise only get as many off the modified
    // list as I have free disk slots.
    ULONG64 numPagesToActuallySwap = 0;

    while (numPagesToActuallySwap < MAX_WRITE_PAGES)
    {
        ULONG64 currentFreeDiskSlot = findFreeDiskSlot();

        if (currentFreeDiskSlot == -1)
        {
            break;
        }
        diskSlotsToBatch[numPagesToActuallySwap] = currentFreeDiskSlot;

        numPagesToActuallySwap++;
    }

    ULONG64 swapFrameNumbers[MAX_WRITE_PAGES];

    for (ULONG64 i = 0; i < numPagesToActuallySwap; i++)
    {
        Frame* currentFrame = popFirstFrame(&modifiedList);
        modifiedListLength--;
        if (currentFrame == NULL)
        {
            numPagesToActuallySwap = i;
            break;  // If we don't have any more frames to swap, break out of the loop.
        }
        swapFrameNumbers[i] = findFrameNumberFromFrame(currentFrame);

        // Then set this frame to not be on the modified list anymore.
        currentFrame->isOnModifiedList = 0;
    }

    ASSERT(numPagesToActuallySwap != 0);


    if (MapUserPhysicalPages (transferVA, numPagesToActuallySwap, swapFrameNumbers) == FALSE) {
        printf ("swapToDisk : could not map VA %p to page %llX\n", transferVA,
            swapFrameNumbers);

        DebugBreak();
        return;
    }

    for (ULONG64 i = 0; i < numPagesToActuallySwap; i++)
    {
        ULONG64 currentDiskSlot = diskSlotsToBatch[i];

        Frame* currentFrame = findFrameFromFrameNumber(swapFrameNumbers[i]);

        PVOID copyVA = (PVOID) ((ULONG_PTR)transferVA + i * PAGE_SIZE);

        // Copy the contents of the VA to the disk space we are trying to write to.
        memcpy(totalDiskSpace + currentDiskSlot * PAGE_SIZE, copyVA, PAGE_SIZE);

        currentFrame->diskIndex = currentDiskSlot;

        currentFrame->PTE->transitionFormat.isTransitionFormat = 0;
    }

    // Once we do batch write, we need to put them on the standby list
    // Then try to go back around and retry page faulted instead of continuing assuming we got a page

    if (!MapUserPhysicalPages(transferVA, numPagesToActuallySwap, NULL)) {
        printf("swapToDisk: Failed to unmap user physical pages!");
        exit(-1);
    }
}

// DECLSPEC_NOINLINE
ULONG64 findFreeDiskSlot()
{
    // Make this start at last found slot in future
    ULONG64 currentSearchIndex = 0;

    while (currentSearchIndex < NUMBER_OF_VIRTUAL_PAGES && freeDiskSpace[currentSearchIndex] == false)
    {
        currentSearchIndex++;
    }

    if (currentSearchIndex == NUMBER_OF_VIRTUAL_PAGES)
    {
        return -1;
    }

    // #TODO bp: currently just setting this to in use, but it might actually not get used if not enough pages
    freeDiskSpace[currentSearchIndex] = false;

    // lastUsedFreeDiskSlot = currentSearchIndex;

    return currentSearchIndex;
}

VOID swapFromDisk(Frame* frameToFill, ULONG64 diskIndexToTransferFrom)
{
    // Use same transfer VA for now, multithreaded might need a different one
    ULONG64 frameNumber = findFrameNumberFromFrame(frameToFill);

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