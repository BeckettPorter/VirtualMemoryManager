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

VOID evictFrame()
{
    Frame* evictFrames[MAX_WRITE_PAGES];
    void* evictVAs[MAX_WRITE_PAGES];
    ULONG64 numPagesToEvict = 0;

    while (numPagesToEvict < MAX_WRITE_PAGES)
    {
        // Break from the while loop if we don't have any active frames to evict
        if (activeList == NULL)
        {
            break;
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

        evictFrames[numPagesToEvict] = currentFrame;
        evictVAs[numPagesToEvict] = PageTableEntryToVA(currentFrame->PTE);
        numPagesToEvict++;
    }

    // unmap the old VA (batched)
    if (MapUserPhysicalPages (evictVAs, numPagesToEvict, NULL) == FALSE) {

        printf ("evictFrame : could not unmap %llu VAs at %p from their pages",
            numPagesToEvict, evictVAs);

        DebugBreak();
    }

    for (ULONG64 i = 0; i < numPagesToEvict; i++)
    {
        Frame* currentFrameToFix = evictFrames[i];

        PageTableEntry* victimPTE = currentFrameToFix->PTE;

        PageTableEntry PTEContents = *victimPTE;

        PTEContents.entireFormat = 0;
        PTEContents.transitionFormat.isTransitionFormat = 1;
        PTEContents.transitionFormat.mustBeZero = 0;
        PTEContents.transitionFormat.pageFrameNumber = findFrameNumberFromFrame(currentFrameToFix);

        findFrameFromFrameNumber(PTEContents.transitionFormat.pageFrameNumber)->diskIndex = 0;

        victimPTE->entireFormat = PTEContents.entireFormat;

        // NEED TO ADD TO MODIFIED LIST
        modifiedList = addToFrameList(modifiedList, currentFrameToFix);
        modifiedListLength++;

        // Set to be on the modified list in the frame.
        currentFrameToFix->isOnModifiedList = 1;
    }
}

VOID modifiedPageWrite()
{
    swapToDisk();
}

Frame* findFrameFromFrameNumber(ULONG64 frameNumber)
{
    return &pfnArray[frameNumber];
}

ULONG64 findFrameNumberFromFrame(Frame* frame)
{
    return frame - pfnArray;
}