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

    ULONG64 currentFreeDiskSlot = findFreeDiskSlot();

    if (currentFreeDiskSlot == -1)
    {
        printf("No free disk slots available at all when tried to swap to disk!");
        DebugBreak();
    }

    while (currentFreeDiskSlot != -1 && numPagesToActuallySwap < MAX_WRITE_PAGES)
    {
        diskSlotsToBatch[numPagesToActuallySwap] = currentFreeDiskSlot;
        numPagesToActuallySwap++;
        currentFreeDiskSlot = findFreeDiskSlot();
    }

    Frame* currentFrame;
    ULONG64 swapFrameNumbers[MAX_WRITE_PAGES];

    for (int i = 0; i < numPagesToActuallySwap; i++)
    {
        currentFrame = popFirstFrame(&modifiedList);
        if (!currentFrame)
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

    for (int i = 0; i < numPagesToActuallySwap; i++)
    {
        ULONG64 currentDiskSlot = diskSlotsToBatch[i];

        PVOID localVA = (PVOID) ((ULONG_PTR) transferVA + (i * PAGE_SIZE));

        memcpy(totalDiskSpace + currentDiskSlot * PAGE_SIZE, localVA, PAGE_SIZE);

        // Set the disk space we just copied to to false so we know it is in use.
        freeDiskSpace[currentDiskSlot] = false;

        currentFrame = findFrameFromFrameNumber(swapFrameNumbers[i]);

        currentFrame->diskIndex = currentDiskSlot;

        currentFrame->PTE->transitionFormat.isTransitionFormat = 0;
    }

    if (!MapUserPhysicalPages(transferVA, numPagesToActuallySwap, NULL)) {
        printf("swapToDisk: Failed to unmap user physical pages!");
        exit(-1);
    }
}

// DECLSPEC_NOINLINE
ULONG64 findFreeDiskSlot()
{
    if (diskSlotsArrayList.flink == &diskSlotsArrayList) {
        printf("No free disk slots available!\n");
        return -1;
    }

    ULONG64Node* node = CONTAINING_RECORD(diskSlotsArrayList.flink, ULONG64Node, listEntry);
    removeListEntry(&node->listEntry);

    ULONG64 value = node->value;
    free(node);

    return value;
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


    // Create a new node for this slot and add it back to the list
    ULONG64Node* freedNode = createNode(diskIndexToTransferFrom);
    if (!freedNode) {
        printf("Memory allocation failed for freed ULONG64Node in swapFromDisk!\n");
        exit(-1);
    }
    addListEntry(&diskSlotsArrayList, &freedNode->listEntry);


    if (!MapUserPhysicalPages(transferVA, 1, NULL)) {
        printf("swapFromDisk: Failed to map user physical pages!");
        exit(-1);
    }
}

ULONG64Node* createNode(ULONG64 value)
{
    ULONG64Node* node = malloc(sizeof(ULONG64Node));
    if (node) {
        node->value = value;
        node->listEntry.flink = NULL;  // Initialize to NULL, to be set by addListEntry
        node->listEntry.blink = NULL;  // Initialize to NULL
    }
    return node;

}