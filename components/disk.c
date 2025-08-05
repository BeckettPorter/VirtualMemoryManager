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
        acquireLock(&modifiedListLock);
        Frame* currentFrame = popFirstFrame(&modifiedList);

        if (currentFrame == NULL)
        {
            releaseLock(&modifiedListLock);
            numPagesToActuallySwap = i;
            break;  // If we don't have any more frames to swap, break out of the loop.
        }

        modifiedListLength--;

        // Then set this frame to not be on the modified list anymore.
        currentFrame->isOnModifiedList = 0;

        releaseLock(&modifiedListLock);

        swapFrameNumbers[i] = findFrameNumberFromFrame(currentFrame);
    }

    ASSERT(numPagesToActuallySwap != 0);

    // #todo bp: lock transfer write va

    if (MapUserPhysicalPages (writeTransferVA, numPagesToActuallySwap, swapFrameNumbers) == FALSE) {
        printf ("swapToDisk : could not map VA %p to pages\n", writeTransferVA);

        DebugBreak();
        return;
    }

    for (ULONG64 i = 0; i < numPagesToActuallySwap; i++)
    {
        ULONG64 currentDiskSlot = diskSlotsToBatch[i];

        Frame* currentFrame = findFrameFromFrameNumber(swapFrameNumbers[i]);

        PVOID copyVA = (PVOID) ((ULONG_PTR)writeTransferVA + i * PAGE_SIZE);

        // Add bounds checking to prevent memory corruption
        if (currentDiskSlot >= NUMBER_OF_DISK_SLOTS) {
            printf("swapToDisk: Invalid disk slot %llu >= %u\n", currentDiskSlot, NUMBER_OF_DISK_SLOTS);
            DebugBreak();
            return;
        }

        // Copy the contents of the VA to the disk space we are trying to write to.
        memcpy(totalDiskSpace + currentDiskSlot * PAGE_SIZE, copyVA, PAGE_SIZE);

        currentFrame->diskIndex = currentDiskSlot;

        acquireLock(&standbyListLock);
        standbyList = addToFrameList(standbyList, currentFrame);
        releaseLock(&standbyListLock);

        // Set the disk space we just copied to false to set it as occupied.
        acquireLock(&diskSpaceLock);
        ASSERT(freeDiskSpace[currentDiskSlot] == true);
        freeDiskSpace[currentDiskSlot] = false;
        releaseLock(&diskSpaceLock);
    }

    if (!MapUserPhysicalPages(writeTransferVA, numPagesToActuallySwap, NULL)) {
        printf("swapToDisk: Failed to unmap user physical pages!");
        exit(-1);
    }
}

// DECLSPEC_NOINLINE
ULONG64 findFreeDiskSlot()
{
    acquireLock(&diskSpaceLock);

    // Make this start at last found slot.
    ULONG64 currentSearchIndex = diskSearchStartIndex;

    for (ULONG64 i = 0; i < NUMBER_OF_DISK_SLOTS; i++)
    {
        if (freeDiskSpace[currentSearchIndex] == true)
        {
            diskSearchStartIndex = currentSearchIndex + 1;

            // Wrap our search index around if we go past the end of the array.
            if (diskSearchStartIndex >= NUMBER_OF_DISK_SLOTS)
            {
                diskSearchStartIndex = 0;
            }

            releaseLock(&diskSpaceLock);
            return currentSearchIndex;
        }
        currentSearchIndex++;

        // Wrap our current search index around if we go past the end of the array.
        if (currentSearchIndex >= NUMBER_OF_DISK_SLOTS)
        {
            currentSearchIndex = 0;
        }
    }

    releaseLock(&diskSpaceLock);
    // Return -1 if we couldn't find any free disk slots.
    return -1;
}

VOID swapFromDisk(Frame* frameToFill, ULONG64 diskIndexToTransferFrom)
{
    // Add bounds checking to prevent memory corruption
    if (diskIndexToTransferFrom >= NUMBER_OF_DISK_SLOTS) {
        printf("swapFromDisk: Invalid disk index %llu >= %u\n", diskIndexToTransferFrom, NUMBER_OF_DISK_SLOTS);
        DebugBreak();
        return;
    }
    
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
    acquireLock(&diskSpaceLock);
    ASSERT(freeDiskSpace[diskIndexToTransferFrom] == false);

    // Set the disk space we just copied from to true to clear it from being used.
    freeDiskSpace[diskIndexToTransferFrom] = true;
    releaseLock(&diskSpaceLock);

    memcpy(transferVAToUse, totalDiskSpace + diskIndexToTransferFrom * PAGE_SIZE, PAGE_SIZE);

    releaseTransferVALock();
}