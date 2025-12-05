//
// Created by porte on 6/20/2025.
//

#include "disk.h"

#include <stdbool.h>
#include <stdio.h>
#include <intrin.h>

#include "pages.h"

static ULONG64 DiskSlotToWordIndex(ULONG64 slotIndex)
{
    return slotIndex >> 6;
}

static LONG DiskSlotToBitOffset(ULONG64 slotIndex)
{
    return (LONG)(slotIndex & 63ULL);
}

static BOOLEAN TryReserveDiskSlotInternal(ULONG64 slotIndex)
{
    ASSERT(slotIndex < NUMBER_OF_DISK_SLOTS);
    ULONG64 wordIndex = DiskSlotToWordIndex(slotIndex);
    ASSERT(wordIndex < diskSlotBitmapLength);

    volatile LONG64* word = &diskSlotBitmap[wordIndex];
    LONG bit = DiskSlotToBitOffset(slotIndex);

    BOOLEAN wasInUse = _interlockedbittestandset64(word, bit);
    return wasInUse == FALSE;
}

BOOLEAN IsDiskSlotInUse(ULONG64 slotIndex)
{
    if (slotIndex == INVALID_DISK_SLOT)
    {
        return FALSE;
    }

    if (slotIndex >= NUMBER_OF_DISK_SLOTS)
    {
        return FALSE;
    }

    ULONG64 wordIndex = DiskSlotToWordIndex(slotIndex);
    if (wordIndex >= diskSlotBitmapLength)
    {
        return FALSE;
    }

    ULONG64 snapshot = (ULONG64)diskSlotBitmap[wordIndex];
    return ((snapshot >> DiskSlotToBitOffset(slotIndex)) & 1ULL) != 0;
}

VOID ReleaseDiskSlot(ULONG64 slotIndex)
{
    if (slotIndex == INVALID_DISK_SLOT)
    {
        return;
    }

    ASSERT(slotIndex < NUMBER_OF_DISK_SLOTS);
    ULONG64 wordIndex = DiskSlotToWordIndex(slotIndex);
    ASSERT(wordIndex < diskSlotBitmapLength);

    volatile LONG64* word = &diskSlotBitmap[wordIndex];
    LONG bit = DiskSlotToBitOffset(slotIndex);

    BOOLEAN wasInUse = _interlockedbittestandreset64(word, bit);
    ASSERT(wasInUse == TRUE);
}

// In future, can add number of pages to swap here and pass array
VOID swapToDisk()
{
    ULONG64 diskSlotsToBatch[MAX_WRITE_PAGES];

    // Check my modified list, I want to write all of those out in one batch
    // But first check if we have enough disk slots to do this, otherwise only get as many off the modified
    // list as I have free disk slots.
    ULONG64 numPagesToActuallySwap = 0;
    ULONG64 numSlotsFound = 0;

    while (numSlotsFound < MAX_WRITE_PAGES)
    {
        ULONG64 currentFreeDiskSlot = findFreeDiskSlot();

        // If we don't have any free disk slots, break out of the loop.
        if (currentFreeDiskSlot == -1)
        {
            break;
        }
        diskSlotsToBatch[numSlotsFound] = currentFreeDiskSlot;

        numSlotsFound++;
    }

    ULONG64 swapFrameNumbers[MAX_WRITE_PAGES];

    AcquireModifiedListLock();
    for (ULONG64 i = 0; i < numSlotsFound; i++)
    {

        Frame* currentFrame = popFirstFrame(&modifiedList);

        if (currentFrame == NULL)
        {
            break;  // If we don't have any more frames to swap, break out of the loop.
        }

        // Then set this frame to not be on the modified list anymore.
        currentFrame->isOnModifiedList = 0;
        currentFrame->isBeingWritten = 1;

        swapFrameNumbers[i] = findFrameNumberFromFrame(currentFrame);
        numPagesToActuallySwap++;
    }

    // Release list lock before performing IO
    ReleaseModifiedListLock();

    // If there are no pages to swap, free any reserved disk slots and return.
    if (numPagesToActuallySwap == 0)
    {
        for (ULONG64 i = 0; i < numSlotsFound; i++)
        {
            ReleaseDiskSlot(diskSlotsToBatch[i]);
        }
        return;
    }

    // Clear out unused disk slots
    for (ULONG64 i = numPagesToActuallySwap; i < numSlotsFound; i++)
    {
        ReleaseDiskSlot(diskSlotsToBatch[i]);
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

        AcquireStandbyListLock();

        // Move frames still being written to the standby list.
        if (currentFrame->isBeingWritten == 1)
        {
            currentFrame->isBeingWritten = 0;
            currentFrame->diskIndex = currentDiskSlot;
            addToFrameList(&standbyList, currentFrame);

            ASSERT(IsDiskSlotInUse(currentDiskSlot) == TRUE);

            ReleaseStandbyListLock();
        }
        else
        {
            ReleaseStandbyListLock();

            // Free disk slot if the frame was rescued before this write.
            ASSERT(IsDiskSlotInUse(currentDiskSlot) == TRUE);
            ReleaseDiskSlot(currentDiskSlot);

            currentFrame->diskIndex = INVALID_DISK_SLOT;
        }
    }

    if (!MapUserPhysicalPages(writeTransferVA, numPagesToActuallySwap, NULL))
    {
        printf("swapToDisk: Failed to unmap user physical pages!");
        exit(-1);
    }
}

// DECLSPEC_NOINLINE
ULONG64 findFreeDiskSlot()
{
    ASSERT(diskSlotBitmapLength > 0);
    ULONG64 totalSlots = NUMBER_OF_DISK_SLOTS;
    ULONG64 currentSearchIndex = diskSearchStartIndex;
    ULONG64 scanned = 0;

    while (scanned < totalSlots)
    {
        ULONG64 wordIndex = DiskSlotToWordIndex(currentSearchIndex);
        LONG bitOffset = DiskSlotToBitOffset(currentSearchIndex);

        if (diskSlotBitmap[wordIndex] == -1LL)
        {
            ULONG64 skip = 64ULL - bitOffset;
            if (skip == 0)
            {
                skip = 64ULL;
            }

            ULONG64 remaining = totalSlots - scanned;
            if (skip > remaining)
            {
                skip = remaining;
            }

            scanned += skip;
            currentSearchIndex += skip;
            if (currentSearchIndex >= totalSlots)
            {
                currentSearchIndex -= totalSlots;
            }
            continue;
        }

        if (TryReserveDiskSlotInternal(currentSearchIndex))
        {
            diskSearchStartIndex = currentSearchIndex + 1;
            if (diskSearchStartIndex >= totalSlots)
            {
                diskSearchStartIndex = 0;
            }
            return currentSearchIndex;
        }

        currentSearchIndex++;
        scanned++;
        if (currentSearchIndex >= totalSlots)
        {
            currentSearchIndex = 0;
        }
    }

    // Return -1 if we couldn't find any free disk slots.
    return -1;
}

VOID swapFromDisk(Frame* frameToFill, ULONG64 diskIndexToTransferFrom, PVOID context)
{
    // Add bounds checking to prevent memory corruption
    if (diskIndexToTransferFrom >= NUMBER_OF_DISK_SLOTS) {
        printf("swapFromDisk: Invalid disk index %llu >= %u\n", diskIndexToTransferFrom, NUMBER_OF_DISK_SLOTS);
        DebugBreak();
        return;
    }
    
    // Use same transfer VA for now, multithreaded might need a different one
    ULONG64 frameNumber = findFrameNumberFromFrame(frameToFill);

    PVOID transferVAToUse = acquireTransferVA(context);

    if (MapUserPhysicalPages (transferVAToUse, 1, &frameNumber) == FALSE) {
        printf ("swapFromDisk : could not map VA %p to page %llX\n", transferVAToUse,
            frameNumber);

        return;
    }

    // If the slot in disk space we are trying to fill from is not already in use, debug break
    // because the contents should be there.
    ASSERT(IsDiskSlotInUse(diskIndexToTransferFrom) == TRUE);
    ReleaseDiskSlot(diskIndexToTransferFrom);

    frameToFill->diskIndex = INVALID_DISK_SLOT;

    memcpy(transferVAToUse, totalDiskSpace + diskIndexToTransferFrom * PAGE_SIZE, PAGE_SIZE);
}