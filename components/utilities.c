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
    return;
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
VOID addToFrameList(frameListHead* head, Frame* item) {

    validateFrameList(head);

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

    validateFrameList(head);
}

// Removes `item` from the list `head` (if present).
// Returns the new head of the list.
VOID removeFromFrameList(frameListHead* headList, Frame* item) {

    validateFrameList(headList);

    Frame* head = headList->headFrame;

    if (!head || !item)
    {
        DebugBreak();
        return;
        // Removing something not on list!
    }

    headList->length--;

    // If the item to remove is the head, pop it off.
    if (head == item) {
        headList->headFrame = item->nextPFN;

        validateFrameList(headList);
        return;
    }

    // Otherwise, walk the list looking for the item.
    Frame* prev = head;
    Frame* cur  = head->nextPFN;
    while (cur) {
        if (cur == item) {
            prev->nextPFN = cur->nextPFN;
            cur->nextPFN  = NULL;
            break;
        }
        prev = cur;
        cur  = cur->nextPFN;
    }

    ASSERT (cur != NULL);

    validateFrameList(headList);
}

// Pops the first Frame* from *headPtr, returns the popped frame (or NULL if empty).
Frame* popFirstFrame(frameListHead* headPtr) {

    validateFrameList(headPtr);

    Frame* frameToPop = headPtr->headFrame;

    if (!frameToPop)
    {
        return NULL;
        // Popping from empty list!
    }

    headPtr->length--;


    headPtr->headFrame = frameToPop->nextPFN;
    frameToPop->nextPFN = NULL;

    validateFrameList(headPtr);
    return frameToPop;
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

boolean wipePage(Frame* frameToWipe)
{
    ULONG64 frameNumberToWipe = findFrameNumberFromFrame(frameToWipe);

    PVOID transferVAToUse = acquireTransferVA();

    //ASSERT (FALSE);
    if (MapUserPhysicalPages (transferVAToUse, 1, &frameNumberToWipe) == FALSE) {

        printf ("wipePage : could not map transferVA %p to frame num %llX\n", transferVAToUse,
            frameNumberToWipe);

        DebugBreak();
    }

 //   ASSERT (false);

    memset(transferVAToUse, 0, PAGE_SIZE);

    releaseTransferVALock();

    return true;
}

PVOID acquireTransferVA()
{
    acquireLock(&transferVALock);

    // Check bounds before incrementing
    if (currentTransferVAIndex >= TRANSFER_VA_COUNT - 1)
    {
        flushTransferVAs();
        currentTransferVAIndex = 0;
    }
    
    currentTransferVAIndex++;

    PVOID result = (PVOID) ((ULONG_PTR)transferVA + currentTransferVAIndex * PAGE_SIZE);

    return result;
}

VOID releaseTransferVALock()
{
    releaseLock(&transferVALock);
}

// Flush all of the transfer VA's that we have mapped.
VOID flushTransferVAs()
{
    if (!MapUserPhysicalPages(transferVA, currentTransferVAIndex + 1, NULL)) {
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