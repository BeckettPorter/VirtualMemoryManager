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

        // If we don't have any free disk slots, break out of the loop.
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
        EnterCriticalSection(&modifiedListLock);
        Frame* currentFrame = popFirstFrame(&modifiedList);

        if (currentFrame == NULL)
        {
            LeaveCriticalSection(&modifiedListLock);
            numPagesToActuallySwap = i;
            break;  // If we don't have any more frames to swap, break out of the loop.
        }


        modifiedListLength--;
        LeaveCriticalSection(&modifiedListLock);

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

        EnterCriticalSection(&standbyListLock);
        standbyList = addToFrameList(standbyList, currentFrame);
        LeaveCriticalSection(&standbyListLock);

        // Set the disk space we just copied to false to set it as occupied.
        EnterCriticalSection(&diskSpaceLock);
        freeDiskSpace[currentDiskSlot] = false;
        LeaveCriticalSection(&diskSpaceLock);
    }

    if (!MapUserPhysicalPages(transferVA, numPagesToActuallySwap, NULL)) {
        printf("swapToDisk: Failed to unmap user physical pages!");
        exit(-1);
    }
}

// DECLSPEC_NOINLINE
ULONG64 findFreeDiskSlot()
{
    EnterCriticalSection(&diskSpaceLock);

    // Make this start at last found slot.
    ULONG64 currentSearchIndex = diskSearchStartIndex;

    for (ULONG64 i = 0; i < NUMBER_OF_VIRTUAL_PAGES; i++)
    {
        if (freeDiskSpace[currentSearchIndex] == true)
        {
            diskSearchStartIndex = currentSearchIndex + 1;

            // Wrap our search index around if we go past the end of the array.
            if (diskSearchStartIndex >= NUMBER_OF_VIRTUAL_PAGES)
            {
                diskSearchStartIndex = 0;
            }

            LeaveCriticalSection(&diskSpaceLock);
            return currentSearchIndex;
        }
        currentSearchIndex++;

        // Wrap our current search index around if we go past the end of the array.
        if (currentSearchIndex >= NUMBER_OF_VIRTUAL_PAGES)
        {
            currentSearchIndex = 0;
        }
    }

    LeaveCriticalSection(&diskSpaceLock);
    // Return -1 if we couldn't find any free disk slots.
    return -1;
}

VOID swapFromDisk(Frame* frameToFill, ULONG64 diskIndexToTransferFrom)
{
    // Use same transfer VA for now, multithreaded might need a different one
    ULONG64 frameNumber = findFrameNumberFromFrame(frameToFill);

    PVOID transferVAToUse = acquireTransferVA();

    if (MapUserPhysicalPages (transferVAToUse, 1, &frameNumber) == FALSE) {
        printf ("swapFromDisk : could not map VA %p to page %llX\n", transferVAToUse,
            frameNumber);

        return;
    }

    // If the slot in disk space we are trying to fill from is not already in use, debug break
    // because the contents should be there.
    EnterCriticalSection(&diskSpaceLock);
    if (freeDiskSpace[diskIndexToTransferFrom] == true)
    {
        DebugBreak();
    }

    // Set the disk space we just copied from to true to clear it from being used.
    freeDiskSpace[diskIndexToTransferFrom] = true;
    LeaveCriticalSection(&diskSpaceLock);

    memcpy(transferVAToUse, totalDiskSpace + diskIndexToTransferFrom * PAGE_SIZE, PAGE_SIZE);
}