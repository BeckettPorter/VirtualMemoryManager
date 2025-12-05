//
// Created by porte on 6/20/2025.
//

#include "pages.h"

#include <stdbool.h>
#include <stdio.h>

#include "disk.h"

Frame* getFreeFrame()
{
    AcquireFreeListLock();

    // If our list is empty, return NULL because we couldn't find a free frame.
    if (freeList.headFrame == NULL)
    {
        ReleaseFreeListLock();
        return NULL;
    }

    // Get first frame from free list and set head to next one.
    Frame* frame = popFirstFrame(&freeList);
    ReleaseFreeListLock();

    return frame;
}

VOID evictFrame()
{
    Frame* evictFrames[MAX_WRITE_PAGES];
    void* evictVAs[MAX_WRITE_PAGES];
    ULONG64 numPagesToEvict = 0;

    AcquireActiveListLock();
    while (numPagesToEvict < MAX_WRITE_PAGES)
    {
        // Break from the while loop if we don't have any active frames to evict
        if (activeList.headFrame == NULL)
        {
            break;
        }

        Frame* currentFrame = popFirstFrame(&activeList);

        evictFrames[numPagesToEvict] = currentFrame;
        evictVAs[numPagesToEvict] = PageTableEntryToVA(currentFrame->PTE);
        numPagesToEvict++;
    }
    ReleaseActiveListLock();

    // unmap the old VA (batched)
    if (MapUserPhysicalPagesScatter (evictVAs, numPagesToEvict, NULL) == FALSE) {

        printf ("evictFrame : could not unmap %llu VAs at %p from their pages",
            numPagesToEvict, evictVAs);

        DebugBreak();
    }

    for (ULONG64 i = 0; i < numPagesToEvict; i++)
    {
        Frame* currentFrameToFix = evictFrames[i];

        PageTableEntry* victimPTE = currentFrameToFix->PTE;
        
        // Check if victimPTE is valid before proceeding
        if (victimPTE == NULL)
        {
            DebugBreak();
        }

        CRITICAL_SECTION* PTELock = GetPTELock(victimPTE);
        
        // Check if PTELock is valid before proceeding
        if (PTELock == NULL)
        {
            DebugBreak();
        }

        AcquirePTELock(PTELock);

        PageTableEntry PTEContents = *victimPTE;

        PTEContents.entireFormat = 0;
        PTEContents.transitionFormat.isTransitionFormat = 1;
        PTEContents.transitionFormat.mustBeZero = 0;
        PTEContents.transitionFormat.pageFrameNumber = findFrameNumberFromFrame(currentFrameToFix);

        findFrameFromFrameNumber(PTEContents.transitionFormat.pageFrameNumber)->diskIndex = 0;

        victimPTE->entireFormat = PTEContents.entireFormat;

        ReleasePTELock(PTELock);

        // NEED TO ADD TO MODIFIED LIST
        AcquireModifiedListLock();
        addToFrameList(&modifiedList, currentFrameToFix);

        // Set to be on the modified list in the frame.
        currentFrameToFix->isOnModifiedList = 1;
        ReleaseModifiedListLock();
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