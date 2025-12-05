//
// Created by porte on 6/19/2025.
//

#include "utilities.h"

#include <stdbool.h>
#include <stdio.h>
#include <windows.h>

#include "disk.h"
#include "pages.h"

ULONG64 CurrentUserThreadCount = NUMBER_USER_THREADS;

PageTableEntry* VAToPageTableEntry(void* virtualAddress)
{
    ULONG64 index = ((ULONG64)virtualAddress - (ULONG64)vaStartLoc) / PAGE_SIZE;

    PageTableEntry* entry = &pageTable[index];

    return entry;
}


void* PageTableEntryToVA(PageTableEntry* entry)
{
    // To get the location VA thingy, subtract start of page table from entry
    ULONG64 index = (entry - pageTable);

    ULONG64 pointer = (index * PAGE_SIZE) + (ULONG64) vaStartLoc;

    return (void*)pointer;
}

VOID validateFrameList(frameListHead* head)
{
    memset (DebugCheckPageArray, 0, physical_page_count * sizeof (ULONG_PTR));

    ULONG64 count = 0;
    ULONG64 foundIndex = 0;
    Frame* current = head->headFrame;
    while (current)
    {
        BOOL foundValid = false;
        for (foundIndex = 0; foundIndex < physical_page_count; foundIndex++)
        {
            if (physical_page_numbers[foundIndex] == findFrameNumberFromFrame(current))
            {
                foundValid = true;
                count++;
                break;
            }
        }

        if (!foundValid)
        {
            DebugBreak();
            // Busted frame
        }
        if (DebugCheckPageArray[foundIndex] == 1)
        {
            DebugBreak();
            // Found duplicate!!
        }

        DebugCheckPageArray[foundIndex] = 1;

        current = current->nextPFN;
    }
    if (count != head->length)
    {
        DebugBreak();
        // Length doesn't match count!
    }
}

// Returns the new head of the list.
VOID addToFrameList(frameListHead* head, Frame* item)
{
    head->length++;

    if (!item)
    {
        DebugBreak();
        item = head->headFrame;
    }
    else
    {
        item->nextPFN = head->headFrame;
    }

    item->prevPFN = NULL;

    if (head->headFrame != NULL)
    {
        head->headFrame->prevPFN = item;
    }

    head->headFrame = item;
    if (head->tailFrame == NULL)
    {
        head->tailFrame = item;
    }

    // Maintain flags
    if (head == &modifiedList)
    {
        item->isOnModifiedList = 1;
        item->isOnStandbyList = 0;
    }
    else if (head == &standbyList)
    {
        item->isOnStandbyList = 1;
        item->isOnModifiedList = 0;
    }
}

// Removes `item` from the list `head` (if present).
// Returns the new head of the list.
// Removes `item` from the list `head` (if present).
// Returns the new head of the list.
// Optimized for O(1) removal using doubly linked list
VOID removeFromFrameList(frameListHead* headList, Frame* item)
{
    if (!item)
    {
        DebugBreak();
        return;
    }

    // Unlink from previous
    if (item->prevPFN)
    {
        item->prevPFN->nextPFN = item->nextPFN;
    }
    else
    {
        // It was the head
        if (headList->headFrame == item)
        {
            headList->headFrame = item->nextPFN;
        }
        else
        {
            // Error: item has no prev but is not head? 
            // Possibly not in this list or list corruption.
             printf("removeFromFrameList: item %p not at head but has no prev\n", item);
             // DebugBreak(); 
        }
    }

    // Unlink from next
    if (item->nextPFN)
    {
        item->nextPFN->prevPFN = item->prevPFN;
    }
    else
    {
        // It was the tail
        if (headList->tailFrame == item)
        {
            headList->tailFrame = item->prevPFN;
        }
    }

    item->nextPFN = NULL;
    item->prevPFN = NULL;
    headList->length--;

    if (headList->headFrame == NULL) {
        headList->tailFrame = NULL;
    }

    if (headList == &modifiedList) {
        item->isOnModifiedList = 0;
    } else if (headList == &standbyList) {
        item->isOnStandbyList = 0;
    }
}

// Pops the first Frame* from *headPtr, returns the popped frame (or NULL if empty).
Frame* popFirstFrame(frameListHead* headPtr)
{
    Frame* frameToPop = headPtr->headFrame;

    if (!frameToPop)
    {
        return NULL;
        // Popping from empty list!
    }

    headPtr->length--;


    headPtr->headFrame = frameToPop->nextPFN;
    
    if (headPtr->headFrame)
    {
        headPtr->headFrame->prevPFN = NULL;
    }
    else
    {
        headPtr->tailFrame = NULL;
    }
    
    frameToPop->nextPFN = NULL;
    frameToPop->prevPFN = NULL;

    if (headPtr == &modifiedList)
    {
        frameToPop->isOnModifiedList = 0;
    }
    else if (headPtr == &standbyList)
    {
        frameToPop->isOnStandbyList = 0;
    }

    return frameToPop;
}

BOOL listContains(frameListHead* headList, Frame* item)
{
    Frame* cur = headList->headFrame;
    while (cur) {
        if (cur == item) return true;
        cur = cur->nextPFN;
    }
    return false;
}

// Append to tail helper for O(1) enqueue
VOID addToFrameListTail(frameListHead* head, Frame* item)
{
    if (!item)
    {
        DebugBreak();
        return;
    }

    item->nextPFN = NULL;
    item->prevPFN = head->tailFrame;

    if (head->tailFrame)
    {
        head->tailFrame->nextPFN = item;
    }
    else
    {
        head->headFrame = item;
    }

    head->tailFrame = item;
    head->length++;

    if (head == &modifiedList)
    {
        item->isOnModifiedList = 1;
        item->isOnStandbyList = 0;
    }
    else if (head == &standbyList)
    {
        item->isOnStandbyList = 1;
        item->isOnModifiedList = 0;
    }
}


VOID
checkVa(PULONG64 va) {
    va = (PULONG64) ((ULONG64)va & ~(PAGE_SIZE - 1));
    for (int i = 0; i < PAGE_SIZE / 8; ++i) {
        if (!(*va == 0 || *va == (ULONG64) va)) {
            DebugBreak();
        }
        va += 1;
    }
}

boolean wipePage(Frame* frameToWipe, PTHREAD_INFO context)
{
    ULONG64 frameNumberToWipe = findFrameNumberFromFrame(frameToWipe);

    PVOID transferVAToUse = acquireTransferVA(context);

    //ASSERT (FALSE);
    if (MapUserPhysicalPages (transferVAToUse, 1, &frameNumberToWipe) == FALSE) {

        printf ("wipePage : could not map transferVA %p to frame num %llX\n", transferVAToUse,
            frameNumberToWipe);

        DebugBreak();
    }

 //   ASSERT (false);

    memset(transferVAToUse, 0, PAGE_SIZE);

    return true;
}

PVOID acquireTransferVA(PTHREAD_INFO context)
{
    ULONG64* transferVAIndex = &context->transferVAIndex;
    PVOID allThreadTransferVAs = context->perThreadTransferVAs;

    // Check bounds before incrementing
    if (*transferVAIndex > TRANSFER_VA_COUNT - 1)
    {
        flushTransferVAs(allThreadTransferVAs);
        *transferVAIndex = 0;
    }


    PVOID result = (PVOID) ((ULONG_PTR)allThreadTransferVAs + *transferVAIndex * PAGE_SIZE);

    (*transferVAIndex)++;
    return result;
}

// Flush all of the transfer VA's that we have mapped.
VOID flushTransferVAs(PVOID allThreadTransferVAs)
{
    if (!MapUserPhysicalPages(allThreadTransferVAs, TRANSFER_VA_COUNT, NULL)) {
        printf("flushTransferVAs: Failed to unmap transfer VA's!");
        exit(-1);
    }
}

VOID shutdownUserThread(int userThreadIndex)
{
    AcquireThreadCountLock();
    numActiveUserThreads--;
    if (numActiveUserThreads == 0)
    {
        SetEvent(shutdownProgramEvent);
        printf ("full_virtual_memory_test : finished accessing random virtual addresses\n");

    }
    ReleaseThreadCountLock();
}

CRITICAL_SECTION* GetPTELock(PageTableEntry* pte)
{
    ULONG64 index = pte - pageTable;
    
    // Add bounds checking to prevent array out-of-bounds access
    if (index >= NUMBER_OF_VIRTUAL_PAGES) {
        printf("GetPTELock: Invalid PTE index %llu >= %u\n", index, NUMBER_OF_VIRTUAL_PAGES);
        DebugBreak();
        return NULL;
    }

    ULONG64 regionNum = index / PTE_REGION_SIZE;
    
    return &pteLockTable[regionNum];
}

VOID acquireLock(CRITICAL_SECTION* lock)
{
    // printf("Acquiring lock %p\n", lock);
    EnterCriticalSection(lock);
}

VOID releaseLock(CRITICAL_SECTION* lock)
{
    // printf("Releasing lock %p\n", lock);
    LeaveCriticalSection(lock);
}

BOOL tryAcquireLock(CRITICAL_SECTION* lock) {
    return TryEnterCriticalSection(lock);
}

VOID AcquireFreeListLock()
{
    EnterCriticalSection(&freeListLock);
}

VOID ReleaseFreeListLock()
{
    LeaveCriticalSection(&freeListLock);
}

VOID AcquireActiveListLock()
{
    EnterCriticalSection(&activeListLock);
}

VOID ReleaseActiveListLock()
{
    LeaveCriticalSection(&activeListLock);
}

VOID AcquireModifiedListLock()
{
    EnterCriticalSection(&modifiedListLock);
}

VOID ReleaseModifiedListLock()
{
    LeaveCriticalSection(&modifiedListLock);
}

VOID AcquireStandbyListLock()
{
    EnterCriticalSection(&standbyListLock);
}

VOID ReleaseStandbyListLock()
{
    LeaveCriticalSection(&standbyListLock);
}

VOID AcquireThreadCountLock()
{
    EnterCriticalSection(&threadCountLock);
}

VOID ReleaseThreadCountLock()
{
    LeaveCriticalSection(&threadCountLock);
}

VOID AcquirePTELock(CRITICAL_SECTION* lock)
{
    EnterCriticalSection(lock);
}

VOID ReleasePTELock(CRITICAL_SECTION* lock)
{
    LeaveCriticalSection(lock);
}