//
// Created by porte on 6/20/2025.
//

#include "pages.h"

#include <stdbool.h>
#include <stdio.h>

#include "disk.h"

Frame* getFreeFrame()
{
    // If our list is empty, return NULL because we couldn't find a free frame.
    if (freeList == NULL)
    {
        return NULL;
    }

    // Get first frame from free list and set head to next one.
    Frame* frame = freeList;
    popFirstFrame(&freeList);

    return frame;
}

Frame* evictFrame()
{
    // Return NULL if we don't have any active frames to evict
    if (activeList == NULL)
    {
        return NULL;
    }

    Frame* previousFrame = NULL;
    Frame* currentFrame = activeList;

    // Walk through our PFNs until we reach the end of the array, moving them back one spot because we removed one.
    while (currentFrame->nextPFN != NULL) {
        previousFrame = currentFrame;
        currentFrame = currentFrame->nextPFN;
    }

    if (previousFrame == NULL) {
        activeList = NULL;
    } else {
        previousFrame->nextPFN = NULL;
    }


    // unmap the old VA -  this will need to batch in future bc very slow
    if (MapUserPhysicalPages (PageTableEntryToVA(currentFrame->PTE), 1, NULL) == FALSE) {

        printf ("evictFrame : could not unmap VA %p to page %llX\n", PageTableEntryToVA(currentFrame->PTE),
            currentFrame->physicalFrameNumber);

        DebugBreak();
    }

    PageTableEntry* victimPTE = currentFrame->PTE;

    PageTableEntry PTEContents = *victimPTE;

    PTEContents.entireFormat = 0;
    PTEContents.transitionFormat.isTransitionFormat = 1;
    PTEContents.transitionFormat.mustBeZero = 0;
    PTEContents.transitionFormat.pageFrameNumber = currentFrame->physicalFrameNumber;

    findFrameFromFrameNumber(PTEContents.transitionFormat.pageFrameNumber)->diskIndex = 0;

    victimPTE->entireFormat = PTEContents.entireFormat;

    // NEED TO ADD TO MODIFIED LIST
    modifiedList = addToList(modifiedList, currentFrame);

    // Set to be on modified list in frame.
    currentFrame->isOnModifiedList = 1;

    return currentFrame;
}

VOID modifiedPageWrite(Frame* frameToWrite)
{
    // while modifiedList != empty, swapToDisk, add to free list
    // TODO bp: fix this
    // while (modifiedList != NULL)
    // {
    //     // Swap the victim to disk
    //     PageTableEntry* PTEToSwap = popFirstFrame(&modifiedList)->PTE;
    //     swapToDisk(PTEToSwap);
    //
    //     freeList = addToList(freeList, findFrameFromFrameNumber
    //         (PTEToSwap->transitionFormat.pageFrameNumber));
    // }

    swapToDisk(frameToWrite->PTE);
}

Frame* findFrameFromFrameNumber(ULONG64 frameNumber)
{
    // how many pages AllocateUserPhysicalPages actually returned
    ULONG64 count = physical_page_count;

    for (ULONG64 i = 0; i < count; ++i) {

        if (pfnArray[i].physicalFrameNumber == frameNumber) {
            return &pfnArray[i];
        }
    }
    DebugBreak();
    exit(-2);
    // write fatal print here
}