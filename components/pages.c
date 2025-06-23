//
// Created by porte on 6/20/2025.
//

#include "pages.h"

#include <stdbool.h>

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


    // Add to active list
    activeList = addToList(activeList, frame);

    return frame;
}

VOID releaseFrame(Frame* frame)
{
    // Add back to free list
    freeList = addToList(freeList, frame);

    // Now need to remove from active list
    evictFrame();
}

Frame* evictFrame()
{
    // Check if we have any frames in standby list to move to free list first
    if (standbyList != NULL) {
        Frame* standbyFrame = popFirstFrame(&standbyList);
        // Add the frame to the free list
        freeList = addToList(freeList, standbyFrame);
        return standbyFrame;
    }

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


    // Protect against NULL PTE
    if (currentFrame->PTE == NULL) {
        printf("evictFrame: Frame had NULL PTE\n");
        // Put frame directly on modified list without unmapping
        modifiedList = addToList(modifiedList, currentFrame);
        return currentFrame;
    }

    // unmap the old VA -  this will need to batch in future bc very slow
    MapUserPhysicalPages(PageTableEntryToVA(currentFrame->PTE), 1, NULL);

    PageTableEntry* victimPTE = currentFrame->PTE;

    PageTableEntry PTEContents = *victimPTE;

    PTEContents.transitionFormat.isTransitionFormat = 1;
    PTEContents.transitionFormat.disk_index = 0;
    PTEContents.transitionFormat.mustBeZero = 0;

    victimPTE->entireFormat = PTEContents.entireFormat;

    // NEED TO ADD TO MODIFIED LIST
    modifiedList = addToList(modifiedList, currentFrame);

    return currentFrame;
}

VOID modifiedPageWrite()
{
    // Continue processing until the modified list is empty
    while (modifiedList != NULL) {
        // Removes from modified list
        Frame* victim = popFirstFrame(&modifiedList);

        if (victim == NULL) {
            break; // This shouldn't happen, but just in case
        }

        // Swap the victim to disk
        swapToDisk(victim->PTE);

        // Add to standby list
        standbyList = addToList(standbyList, victim);
    }
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
    exit(-1);
    // write fatal print here
}