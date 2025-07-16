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
    ULONG64 numPagesToActuallySwap = MAX_WRITE_PAGES;

    // Check my modified list, I want to write all of those out in one batch
    // But first check if we have enough disk slots to do this, otherwise only get as many off the modified
    // list as I have free disk slots.
    ULONG64 freeDiskSlot = findFreeDiskSlot();


    ULONG64 swapFramesArray = pageToSwap->transitionFormat.pageFrameNumber;

    for (int i = 0; i < numPagesToActuallySwap; i++)
    {
        Frame* currentFrame = findFrameFromFrameNumber(swapFramesArray);
        modifiedList = removeFromFrameList(modifiedList, currentFrame);
        swapFramesArray += currentFrame;
    }

    Frame* frameToRemove = findFrameFromFrameNumber(swapFramesArray);

    modifiedList = removeFromFrameList(modifiedList, frameToRemove);
    frameToRemove->isOnModifiedList = 0;

    if (MapUserPhysicalPages (transferVA, numPagesToActuallySwap, &swapFramesArray) == FALSE) {
        printf ("swapToDisk : could not map VA %p to page %llX\n", transferVA,
            swapFramesArray);

        return;
    }


    memcpy(totalDiskSpace + freeDiskSlot * PAGE_SIZE, transferVA, PAGE_SIZE);
    // Set the disk space we just copied to to false so we know it is in use.
    freeDiskSpace[freeDiskSlot] = false;


    frameToRemove->diskIndex = freeDiskSlot;

    pageToSwap->transitionFormat.isTransitionFormat = 0;


    if (!MapUserPhysicalPages(transferVA, 1, NULL)) {
        printf("swapToDisk: Failed to map user physical pages!");
        exit(-1);
    }
}

DECLSPEC_NOINLINE
ULONG64 findFreeDiskSlot()
{
    if (diskSlotsArrayList.flink == &diskSlotsArrayList) {
        printf("No free disk slots available!\n");
        exit(-1);
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