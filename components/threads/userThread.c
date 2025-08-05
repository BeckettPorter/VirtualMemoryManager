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

    CRITICAL_SECTION* PTELock = GetPTELock(currentPTE);
    
    // Check if PTELock is valid before proceeding
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
        // Else, 2 possibilities, could be in transition (transition bit 1) or not
        if (pteContents.transitionFormat.isTransitionFormat == 1)
        {
            frameNumber = pteContents.transitionFormat.pageFrameNumber;
            currentFrame = findFrameFromFrameNumber(frameNumber);


            // #Todo bp: while loop around this so we can get the locks and THEN check
            // if we can rescue from mod or standby list
            acquireLock(&modifiedListLock);
            if (currentFrame->isOnModifiedList == 1)
            {

                modifiedList = removeFromFrameList(modifiedList, currentFrame);
                currentFrame->isOnModifiedList = 0;

            }
            else
            {
                // If our current frame is not on the modified list, but it IS in transition,
                // we need to remove it from the standby list.
                acquireLock(&standbyListLock);
                standbyList = removeFromFrameList(standbyList, currentFrame);
                ULONG64 diskIndex = currentFrame->diskIndex;
                releaseLock(&standbyListLock);


                acquireLock(&diskSpaceLock);
                freeDiskSpace[diskIndex] = true;
                releaseLock(&diskSpaceLock);
            }
            releaseLock(&modifiedListLock);
        }
        else
            // Else if we don't rescue (either a fresh VA, or one that was trimmed and written to disk)
        {
            boolean retrievedFromStandbyList = false;
            currentFrame = getFreeFrame();

            if (currentFrame == NULL) {
                retrievedFromStandbyList = true;


                acquireLock(&standbyListLock);
                // Check if we can get a frame from the standby list
                while (standbyList == NULL)
                {
                    // If we can't get any from the standby list
                    // Batch evict frames from the active list and add them to the modified list

                    // Reset this event so we can allow the disk mod write to wake us up.
                    ResetEvent(finishedModWriteEvent);

                    // Release standby list lock because we are going to wait and we don't
                    // want to stop others who need the standby list
                    releaseLock(&standbyListLock);

                    // Release PTE lock so another thread can resolve its fault
                    releaseLock(PTELock);

                    SetEvent(trimEvent);

                    WaitForSingleObject(finishedModWriteEvent, INFINITE);


                    // Reacquire PTE lock and continue if another user thread hasn't resolved it.
                    acquireLock(PTELock);
                    if (pteContents.entireFormat != currentPTE->entireFormat)
                    {
                        releaseLock(PTELock);
                        return;
                    }
                    acquireLock(&standbyListLock);
                }

                // Now we know the standby list is not empty, either because it wasn't empty
                // before or we just evicted frames and added them to it.

                // So now we know we can get a frame from the standby list.

                currentFrame = popFirstFrame(&standbyList);
                releaseLock(&standbyListLock);

                // In this case, we can just retry since it means someone else stole all
                // standby pages before we could get them.
                if (currentFrame == NULL)
                {
                    releaseLock(PTELock);
                    return;
                }

                PageTableEntry *victimPTE = currentFrame->PTE;
                
                // Check if victimPTE is valid before proceeding
                if (victimPTE == NULL)
                {
                    printf("resolvePageFault: victimPTE is NULL for frame %llu\n", findFrameNumberFromFrame(currentFrame));
                    releaseLock(PTELock);
                    return;
                }

                CRITICAL_SECTION* victimPTELock = GetPTELock(victimPTE);
                
                // Check if victimPTELock is valid before proceeding
                if (victimPTELock == NULL)
                {
                    printf("resolvePageFault: Invalid victimPTELock for frame %llu\n", findFrameNumberFromFrame(currentFrame));
                    releaseLock(PTELock);
                    return;
                }
                
                acquireLock(victimPTELock);

                // Victim PTE is in transition format, need to turn to invalid disk format
                PageTableEntry victimPteContents;

                victimPteContents.entireFormat = 0;
                victimPteContents.invalidFormat.mustBeZero = 0;
                victimPteContents.invalidFormat.isTransitionFormat = 0;

                victimPteContents.invalidFormat.diskIndex = currentFrame->diskIndex;

                *victimPTE = victimPteContents;

                releaseLock(victimPTELock);
            }

            frameNumber = findFrameNumberFromFrame(currentFrame);

            // Now we have a page. Now we decide to swap from disk or not.
            if (pteContents.entireFormat == 0)
            {
                // In this case we have a brand new PTE, nothing to read from disk (continue as usual)
                // We just need to wipe it to get rid of previous contents IF we got it from the standby list.
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
                // Else we are on disk, so we need to swap from disk.
                ULONG64 diskIndex = pteContents.invalidFormat.diskIndex;

                swapFromDisk(currentFrame, diskIndex);
            }
        }

        //mupp (va,number of pages,frameNumber) - THIS MAKES IT VISIBLE TO USER AGAIN
        if (MapUserPhysicalPages (arbitrary_va, 1, &frameNumber) == FALSE) {

            printf ("full_virtual_memory_test : could not map VA %p to page %llX\n", arbitrary_va,
                frameNumber);

            DebugBreak();
        }

        checkVa(arbitrary_va);

        // Add to active list
        acquireLock(&activeListLock);
        activeList = addToFrameList(activeList, currentFrame);
        releaseLock(&activeListLock);


        // Fill in fields for PTE (valid bit, page frame number)
        currentPTE->validFormat.isValid = 1;
        currentPTE->validFormat.isTransitionFormat = 0;
        currentPTE->validFormat.pageFrameNumber = findFrameNumberFromFrame(currentFrame);

        currentFrame->PTE = currentPTE;
    }


    releaseLock(PTELock);
}