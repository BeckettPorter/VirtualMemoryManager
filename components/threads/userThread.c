//
// Created by porte on 7/25/2025.
//

#include "userThread.h"

#include <sal.h>
#include <stdbool.h>
#include <stdio.h>
#include <windows.h>
#include "../utilities.h"
#include "../pages.h"
#include "../disk.h"



ULONG userThread(_In_ PVOID Context)
{
    PULONG_PTR arbitrary_va;
    unsigned random_number;
    BOOL page_faulted;
    ULONG_PTR virtual_address_size;
    ULONG64 i;

    printf("Thread id %u started\n", ((PTHREAD_INFO) Context)->ThreadNumber);

    for (i = 0; i < TEST_ITERATIONS; i += 1)
    {
        //
        // Randomly access different portions of the virtual address
        // space we obtained above.
        //
        // If we have never accessed the surrounding page size (4K)
        // portion, the operating system will receive a page fault
        // from the CPU and proceed to obtain a physical page and
        // install a PTE to map it - thus connecting the end-to-end
        // virtual address translation.  Then the operating system
        // will tell the CPU to repeat the instruction that accessed
        // the virtual address and this time, the CPU will see the
        // valid PTE and proceed to obtain the physical contents
        // (without faulting to the operating system again).
        //

        random_number = (unsigned) (ReadTimeStampCounter() >> 4);


        virtual_address_size = 64 * physical_page_count * PAGE_SIZE;

        random_number %= virtual_address_size / sizeof (ULONG_PTR);

        //
        // Write the virtual address into each page.  If we need to
        // debug anything, we'll be able to see these in the pages.
        //

        page_faulted = FALSE;

        //
        // Ensure the write to the arbitrary virtual address doesn't
        // straddle a PAGE_SIZE boundary just to keep things simple for
        // now.
        //

        random_number &= ~0x7;

        PULONG_PTR vaStart = vaStartLoc;
        arbitrary_va = vaStart + random_number;
        // Calc PTE for this VA



        while (true)
        {
            // TODO bp: add while loop over entire thing and if we can't resolve fault, just continue and try again
            __try {

                *arbitrary_va = (ULONG_PTR) arbitrary_va;
                break;

            } __except (EXCEPTION_EXECUTE_HANDLER) {

                resolvePageFault(arbitrary_va);
            }
        }
    }

    printf("Thread id %u finished\n", ((PTHREAD_INFO) Context)->ThreadNumber);

    // ASSERT(false);

    shutdownUserThread(((PTHREAD_INFO) Context)->ThreadNumber);
    return 0;
}


VOID resolvePageFault(PULONG_PTR arbitrary_va)
{
    PageTableEntry* currentPTE = VAToPageTableEntry(arbitrary_va);
    ULONG64 frameNumber;
    Frame* currentFrame;
    boolean retrievedFromStandbyList = false;
    CRITICAL_SECTION* victimPTELock = NULL;
    PageTableEntry* victimPTE = NULL;

    // Step 1: Acquire PTE lock first (highest priority in hierarchy)
    CRITICAL_SECTION* PTELock = GetPTELock(currentPTE);
    if (PTELock == NULL)
    {
        printf("resolvePageFault: Invalid PTELock for VA %p\n", arbitrary_va);
        return;
    }
    acquireLock(PTELock);

    // Read the PTE contents while we have the lock
    PageTableEntry pteContents = *currentPTE;

    if (pteContents.invalidFormat.mustBeZero == 0)
    {
        if (pteContents.transitionFormat.isTransitionFormat == 1)
        {
            // Case 1: Page is in transition - rescue from modified or standby list
            frameNumber = pteContents.transitionFormat.pageFrameNumber;
            currentFrame = findFrameFromFrameNumber(frameNumber);

            // Acquire list locks in correct order
            acquireLock(&modifiedListLock);
            acquireLock(&standbyListLock);
            acquireLock(&diskSpaceLock);

            if (currentFrame->isOnModifiedList == 1)
            {
                modifiedList = removeFromFrameList(modifiedList, currentFrame);
                currentFrame->isOnModifiedList = 0;
            }
            else
            {
                // Remove from standby list and free disk space
                standbyList = removeFromFrameList(standbyList, currentFrame);
                ULONG64 diskIndex = currentFrame->diskIndex;
                freeDiskSpace[diskIndex] = true;
            }

            releaseLock(&diskSpaceLock);
            releaseLock(&standbyListLock);
            releaseLock(&modifiedListLock);
        }
        else
        {
            // Case 2: Need to get a new frame
            currentFrame = getFreeFrame();

            if (currentFrame == NULL)
            {
                retrievedFromStandbyList = true;

                // Acquire list locks in correct order
                acquireLock(&standbyListLock);

                while (standbyList == NULL)
                {
                    // Release locks before waiting
                    releaseLock(&standbyListLock);
                    releaseLock(PTELock);

                    ResetEvent(finishedModWriteEvent);
                    SetEvent(trimEvent);
                    WaitForSingleObject(finishedModWriteEvent, INFINITE);

                    // Reacquire locks in correct order
                    acquireLock(PTELock);
                    if (pteContents.entireFormat != currentPTE->entireFormat)
                    {
                        releaseLock(PTELock);
                        return;
                    }
                    acquireLock(&standbyListLock);
                }

                currentFrame = popFirstFrame(&standbyList);
                releaseLock(&standbyListLock);

                if (currentFrame == NULL)
                {
                    releaseLock(PTELock);
                    return;
                }

                // Double-check that no other thread has resolved this page fault
                PageTableEntry currentPteContents = *currentPTE;
                if (currentPteContents.validFormat.isValid == 1) {
                    // Another thread has already resolved this page fault
                    // Return the frame to the appropriate list
                    if (currentFrame != NULL) {
                        // Return to free list or standby list as appropriate
                        acquireLock(&freeListLock);
                        freeList = addToFrameList(freeList, currentFrame);
                        releaseLock(&freeListLock);
                    }
                    releaseLock(PTELock);
                    return;
                }

                // Get victim PTE lock (another PTE lock)
                victimPTE = currentFrame->PTE;
                if (victimPTE == NULL)
                {
                    printf("resolvePageFault: victimPTE is NULL for frame %llu\n", findFrameNumberFromFrame(currentFrame));
                    releaseLock(PTELock);
                    return;
                }

                victimPTELock = GetPTELock(victimPTE);
                if (victimPTELock == NULL)
                {
                    printf("resolvePageFault: Invalid victimPTELock for frame %llu\n", findFrameNumberFromFrame(currentFrame));
                    releaseLock(PTELock);
                    return;
                }

                acquireLock(victimPTELock);

                // Update victim PTE to invalid disk format
                PageTableEntry victimPteContents;
                victimPteContents.entireFormat = 0;
                victimPteContents.invalidFormat.mustBeZero = 0;
                victimPteContents.invalidFormat.isTransitionFormat = 0;
                victimPteContents.invalidFormat.diskIndex = currentFrame->diskIndex;
                *victimPTE = victimPteContents;

                releaseLock(victimPTELock);
            }

            frameNumber = findFrameNumberFromFrame(currentFrame);

            // Handle disk swap if needed
            if (pteContents.entireFormat == 0)
            {
                if (retrievedFromStandbyList)
                {
                    if (wipePage(currentFrame) == false)
                    {
                        printf("wipePage failed in full_virtual_memory_test\n");
                        DebugBreak();
                    }
                }
            }
            else
            {
                ULONG64 diskIndex = pteContents.invalidFormat.diskIndex;
                swapFromDisk(currentFrame, diskIndex);
            }
        }

        // Map the page
        if (MapUserPhysicalPages(arbitrary_va, 1, &frameNumber) == FALSE) {
            printf("full_virtual_memory_test : could not map VA %p to page %llX\n", arbitrary_va, frameNumber);
            DebugBreak();
        }

        checkVa(arbitrary_va);

        // Add to active list (acquire lock in correct order)
        acquireLock(&activeListLock);
        activeList = addToFrameList(activeList, currentFrame);
        releaseLock(&activeListLock);

        // Update PTE
        currentPTE->validFormat.isValid = 1;
        currentPTE->validFormat.isTransitionFormat = 0;
        currentPTE->validFormat.pageFrameNumber = findFrameNumberFromFrame(currentFrame);
        currentFrame->PTE = currentPTE;
    }

    releaseLock(PTELock);
}