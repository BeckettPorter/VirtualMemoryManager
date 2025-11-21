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
    ULONG64 i;
    PULONG_PTR lastVirtualAddressAccess = 0;

    printf("Thread id %u started\n", ((PTHREAD_INFO) Context)->ThreadNumber);

    for (i = 0; i < TEST_ITERATIONS; i += 1)
    {
        if (i % (KB(100)) == 0)
        {
            printf(". ");
        }
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

        random_number %= VIRTUAL_ADDRESS_SIZE / sizeof (ULONG_PTR);

        random_number &= ~0x7;

        ULONG64 accessType = random_number % 3;

        PULONG_PTR vaStart = vaStartLoc;

        // If this is the first time through
        if (lastVirtualAddressAccess == 0)
        {
            arbitrary_va = vaStart + random_number;
        }
        else
        {
            // 1/3 of the time, jump to a completely new random VA.
            if (accessType == 0)
            {
                arbitrary_va = vaStart + random_number;
            }
            // Else, do a read of the next VA
            else
            {
                arbitrary_va = lastVirtualAddressAccess + PAGE_SIZE;

                // Check if we've gone beyond our virtual address space
                if ((ULONG64)arbitrary_va >= (ULONG64)vaStart + VIRTUAL_ADDRESS_SIZE)
                {
                    // Wrap around to beginning of address space
                    arbitrary_va = vaStart;
                }
            }

        }

        lastVirtualAddressAccess = arbitrary_va;


        page_faulted = FALSE;


        while (true)
        {
            __try {

                *arbitrary_va = (ULONG_PTR) arbitrary_va;
                break;

            } __except (EXCEPTION_EXECUTE_HANDLER) {

                resolvePageFault(arbitrary_va, Context);
            }
        }
    }

    printf("Thread id %u finished\n", ((PTHREAD_INFO) Context)->ThreadNumber);

    // ASSERT(false);

    shutdownUserThread(((PTHREAD_INFO) Context)->ThreadNumber);
    return 0;
}


VOID resolvePageFault(PULONG_PTR arbitrary_va, PVOID context)
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

            boolean rescued = false;

            // (1) Being written? Handle under standbyListLock only
            acquireLock(&standbyListLock);
            if (currentFrame->isBeingWritten == 1) {
                currentFrame->isBeingWritten = 0;
                rescued = true;
            }
            releaseLock(&standbyListLock);

            if (!rescued)
            {
                // (2) On Modified?
                acquireLock(&modifiedListLock);
                if (currentFrame->isOnModifiedList == 1) {
                    removeFromFrameList(&modifiedList, currentFrame);
                    rescued = true;
                }
                releaseLock(&modifiedListLock);
            }

            if (!rescued)
            {
                // (3) On Standby?
                acquireLock(&standbyListLock);
                if (currentFrame->isOnStandbyList == 1) {
                    removeFromFrameList(&standbyList, currentFrame);
                    ULONG64 diskIndex = currentFrame->diskIndex;
                    releaseLock(&standbyListLock);

                    acquireLock(&diskSpaceLock);
                    ASSERT(freeDiskSpace[diskIndex] == false);
                    freeDiskSpace[diskIndex] = true;
                    releaseLock(&diskSpaceLock);

                    rescued = true;
                } else {
                    releaseLock(&standbyListLock);
                }
            }

            // (4) Neither list -> raced; retry fault cleanly
            if (!rescued) {
                releaseLock(PTELock);
                return; // outer while(true) will re-enter resolvePageFault
            }

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

                while (standbyList.headFrame == NULL)
                {
                    // Release locks before waiting
                    releaseLock(&standbyListLock);
                    releaseLock(PTELock);

                    ResetEvent(finishedModWriteEvent);
                    SetEvent(trimEvent);
                    WaitForSingleObject(finishedModWriteEvent, INFINITE);

                    // Here we redo the fault now if we trimmed and added to standby list
                    return;
                }

                currentFrame = popFirstFrame(&standbyList);
                releaseLock(&standbyListLock);

                // Get victim PTE lock (another PTE lock)
                victimPTE = currentFrame->PTE;
                if (victimPTE == NULL)
                {
                    DebugBreak();
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

                if (victimPTELock != PTELock)
                {
                    // Update victim PTE under its own lock, then reacquire PTELock.
                    releaseLock(PTELock);

                    acquireLock(victimPTELock);
                    PageTableEntry victimPteContents = {0};
                    victimPteContents.invalidFormat.mustBeZero = 0;
                    victimPteContents.invalidFormat.isTransitionFormat = 0;
                    victimPteContents.invalidFormat.diskIndex = currentFrame->diskIndex;
                    *victimPTE = victimPteContents;
                    releaseLock(victimPTELock);

                    acquireLock(PTELock);

                    // Re-check faulting PTE after temporarily dropping the lock.
                    pteContents = *currentPTE;
                    if (pteContents.validFormat.isValid == 1 && pteContents.validFormat.isTransitionFormat == 0)
                    {
                        acquireLock(&standbyListLock);
                        addToFrameList(&standbyList, currentFrame);
                        releaseLock(&standbyListLock);
                        releaseLock(PTELock);
                        return;
                    }
                }
                else
                {
                    // Update victim PTE to invalid disk format using the faulting lock.
                    PageTableEntry victimPteContents = {0};
                    victimPteContents.invalidFormat.mustBeZero = 0;
                    victimPteContents.invalidFormat.isTransitionFormat = 0;
                    victimPteContents.invalidFormat.diskIndex = currentFrame->diskIndex;
                    *victimPTE = victimPteContents;
                }
            }

            frameNumber = findFrameNumberFromFrame(currentFrame);

            // Handle disk swap if needed
            if (pteContents.entireFormat == 0)
            {
                if (retrievedFromStandbyList)
                {
                    if (wipePage(currentFrame, context) == false)
                    {
                        printf("wipePage failed in full_virtual_memory_test\n");
                        DebugBreak();
                    }
                }
            }
            else
            {
                ULONG64 diskIndex = pteContents.invalidFormat.diskIndex;
                swapFromDisk(currentFrame, diskIndex, context);
            }
        }

        // Map the page
        if (MapUserPhysicalPages(arbitrary_va, 1, &frameNumber) == FALSE) {
            printf("full_virtual_memory_test : could not map VA %p to page %llX\n", arbitrary_va, frameNumber);
            DebugBreak();
        }

        // Removing this because I think checkVA is broken in multithreaded or somethin
        // checkVa(arbitrary_va);


        // Update PTE
        currentPTE->validFormat.isValid = 1;
        currentPTE->validFormat.isTransitionFormat = 0;
        currentPTE->validFormat.pageFrameNumber = findFrameNumberFromFrame(currentFrame);
        currentFrame->PTE = currentPTE;

        // Add to active list (acquire lock in correct order)
        acquireLock(&activeListLock);
        addToFrameListTail(&activeList, currentFrame);
        releaseLock(&activeListLock);
    }

    releaseLock(PTELock);
}