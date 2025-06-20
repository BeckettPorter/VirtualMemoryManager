//
// Created by porte on 6/20/2025.
//

#include "pages.h"

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
    freeList = freeList->nextPFN;

    // Add to active list
    frame->nextPFN = activeList;
    activeList = frame;

    return frame;
}

VOID releaseFrame(Frame* frame)
{
    // Add back to free list
    frame->nextPFN = freeList;
    freeList = frame;

    // Now need to remove from active list
    evictFrame();
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

    return currentFrame;
}

VOID trimPage()
{
    Frame* victim = evictFrame();

    // unmap the old VA
    MapUserPhysicalPages(PageTableEntryToVA(victim->PTE), 1, NULL);

    // Swap the victim to disk
    swapToDisk(victim->PTE);


    // Return to the free list
    victim->nextPFN = freeList;
    freeList = victim;
}
