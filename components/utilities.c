//
// Created by porte on 6/19/2025.
//

#include "utilities.h"

#include <stdbool.h>
#include <stdio.h>
#include <windows.h>

#include "disk.h"
#include "pages.h"


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

    head->headFrame = item;
    if (head->tailFrame == NULL)
    {
        head->tailFrame = item;
    }
}

// Removes `item` from the list `head` (if present).
// Returns the new head of the list.
VOID removeFromFrameList(frameListHead* headList, Frame* item)
{
    Frame* head = headList->headFrame;

    if (!head || !item)
    {
        DebugBreak();
        return;
        // Removing something not on list!
    }



    // If the item to remove is the head, pop it off.
    if (head == item) {
        headList->headFrame = item->nextPFN;
        item->nextPFN = NULL;
        headList->length--;
        return;
    }

    // Otherwise, walk the list looking for the item.
    Frame* prev = head;
    Frame* cur  = head->nextPFN;
    while (cur) {
        if (cur == item) {
            prev->nextPFN = cur->nextPFN;
            cur->nextPFN  = NULL;
            headList->length--;
            return;
        }
        prev = cur;
        cur  = cur->nextPFN;
    }

    // If this occurs we didn't find what we were looking for.
    DebugBreak();
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
    frameToPop->nextPFN = NULL;

    if (headPtr->headFrame == NULL)
    {
        headPtr->tailFrame = NULL;
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
    acquireLock(&threadCountLock);
    numActiveUserThreads--;
    if (numActiveUserThreads == 0)
    {
        SetEvent(shutdownProgramEvent);
        printf ("full_virtual_memory_test : finished accessing random virtual addresses\n");

    }
    releaseLock(&threadCountLock);
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
    
    return &pteLockTable[index];
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