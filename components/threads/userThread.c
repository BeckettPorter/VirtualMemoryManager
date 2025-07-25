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


    for (ULONG64 i = 0; i < MB (1); i += 1) {

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
        PageTableEntry* currentPTE = VAToPageTableEntry(arbitrary_va);
        ULONG64 frameNumber;
        Frame* currentFrame;

        __try {

            *arbitrary_va = (ULONG_PTR) arbitrary_va;

        } __except (EXCEPTION_EXECUTE_HANDLER) {

            page_faulted = TRUE;
        }

        if (page_faulted)
        {
            // Else, 2 possibilities, could be in transition (transition bit 1) or not
            if (currentPTE->transitionFormat.isTransitionFormat == 1)
            {
                frameNumber = currentPTE->transitionFormat.pageFrameNumber;
                currentFrame = findFrameFromFrameNumber(frameNumber);

                // NEED TO REMOVE FROM MODIFIED LIST IF WE GRAB BACK
                if (currentFrame->isOnModifiedList == 1)
                {
                    modifiedList = removeFromFrameList(modifiedList, currentFrame);
                }
                else
                {
                    // If our current frame is not on the modified list, but it IS in transition,
                    // we need to remove it from the standby list.
                    standbyList = removeFromFrameList(standbyList, currentFrame);

                    ULONG64 diskIndex = currentFrame->diskIndex;

                    freeDiskSpace[diskIndex] = true;
                }
            }
            else
                // Else if we don't rescue (either a fresh VA, or one that was trimmed and written to disk)
            {
                boolean retrievedFromStandbyList = false;
                currentFrame = getFreeFrame();

                if (currentFrame == NULL) {
                    retrievedFromStandbyList = true;

                    // Check if we can get a frame from the standby list
                    if (standbyList == NULL)
                    {
                        // If we can't get any from the standby list
                        // Batch evict frames from the active list and add them to the modified list
                        evictFrame();

                        ASSERT(modifiedListLength != 0);

                        modifiedPageWrite();
                    }
                    // Now we know the standby list is not empty, either because it wasn't empty
                    // before or we just evicted frames and added them to it.

                    // So now we know we can get a frame from the standby list.
                    currentFrame = popFirstFrame(&standbyList);

                    if (currentFrame == NULL)
                    {
                        DebugBreak();
                    }

                    PageTableEntry *victimPTE = currentFrame->PTE;

                    // Victim PTE is in transition format, need to turn to invalid disk format
                    PageTableEntry pteContents;

                    pteContents.entireFormat = 0;
                    pteContents.invalidFormat.mustBeZero = 0;
                    pteContents.invalidFormat.isTransitionFormat = 0;

                    pteContents.invalidFormat.diskIndex = currentFrame->diskIndex;

                    *victimPTE = pteContents;
                }

                frameNumber = findFrameNumberFromFrame(currentFrame);

                // Now we have a page. Now we decide to swap from disk or not.
                if (currentPTE->entireFormat == 0)
                {
                    // In this case we have a brand new PTE, nothing to read from disk (continue as usual)
                    // We just need to wipe it to get rid of previous contents IF we got it from the standby list.
                    if (retrievedFromStandbyList)
                    {
                        if (wipePage(currentFrame) == false)
                        {
                            printf("wipePage failed in full_virtual_memory_test\n");
                            return -1;
                        }
                    }
                }
                else
                {
                    // Else we are on disk, so we need to swap from disk.
                    ULONG64 diskIndex = currentPTE->invalidFormat.diskIndex;

                    swapFromDisk(currentFrame, diskIndex);
                }
            }

            //mupp (va,number of pages,frameNumber) - THIS MAKES IT VISIBLE TO USER AGAIN
            if (MapUserPhysicalPages (arbitrary_va, 1, &frameNumber) == FALSE) {

                printf ("full_virtual_memory_test : could not map VA %p to page %llX\n", arbitrary_va,
                    frameNumber);

                return -1;
            }

            checkVa(arbitrary_va);


            // Fill in fields for PTE (valid bit, page frame number)
            currentPTE->validFormat.isValid = 1;
            currentPTE->validFormat.isTransitionFormat = 0;
            currentPTE->validFormat.pageFrameNumber = findFrameNumberFromFrame(currentFrame);

            currentFrame->PTE = currentPTE;

            // Add to active list
            activeList = addToFrameList(activeList, currentFrame);

            //
            // No exception handler needed now since we have connected
            // the virtual address above to one of our physical pages
            // so no subsequent fault can occur.
            //

            *arbitrary_va = (ULONG_PTR) arbitrary_va;
        }
    }

    printf ("full_virtual_memory_test : finished accessing random virtual addresses\n");


    return 0;
}