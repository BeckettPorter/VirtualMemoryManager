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

// Returns the new head of the list.
Frame* addToFrameList(Frame* head, Frame* item) {

    if (!item) return head;
    item->nextPFN = head;


    return item;
}

// Removes `item` from the list `head` (if present).
// Returns the new head of the list.
Frame* removeFromFrameList(Frame* head, Frame* item) {

    if (!head || !item) return head;

    // If the item to remove is the head, pop it off.
    if (head == item) {
        Frame* newHead = head->nextPFN;
        head->nextPFN = NULL;
        return newHead;
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


    return head;
}

// Pops the first Frame* from *headPtr, returns the popped frame (or NULL if empty).
Frame* popFirstFrame(Frame** headPtr) {

    if (!*headPtr) return NULL;
    Frame* first = *headPtr;
    *headPtr = first->nextPFN;
    first->nextPFN = NULL;

    return first;
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

    if (MapUserPhysicalPages (transferVAToUse, 1, &frameNumberToWipe) == FALSE) {

        printf ("wipePage : could not map transferVA %p to frame num %llX\n", transferVAToUse,
            frameNumberToWipe);

        DebugBreak();
    }

    memset(transferVAToUse, 0, PAGE_SIZE);

    return true;
}

PVOID acquireTransferVA()
{
    acquireLock(&transferVALock);

    currentTransferVAIndex++;
    if (currentTransferVAIndex == TRANSFER_VA_COUNT)
    {
        flushTransferVAs();
        currentTransferVAIndex = 0;
    }

    PVOID result = (PVOID) ((ULONG_PTR)transferVA + currentTransferVAIndex * PAGE_SIZE);

    releaseLock(&transferVALock);

    return result;
}

// Flush all of the transfer VA's that we have mapped.
VOID flushTransferVAs()
{
    if (!MapUserPhysicalPages(transferVA, currentTransferVAIndex, NULL)) {
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
    // Hash the PTE address to get lock index
    ULONG64 hash = ((ULONG64)pte / sizeof(PageTableEntry)) % PTE_LOCK_TABLE_SIZE;
    return &pteLockTable[hash];
}

VOID acquireLock(CRITICAL_SECTION* lock)
{
    printf("Acquiring lock %p\n", lock);
    EnterCriticalSection(lock);
}

VOID releaseLock(CRITICAL_SECTION* lock)
{
    printf("Releasing lock %p\n", lock);
    LeaveCriticalSection(lock);
}