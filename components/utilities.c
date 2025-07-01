//
// Created by porte on 6/19/2025.
//

#include "utilities.h"

#include <stdbool.h>
#include <stdio.h>
#include <windows.h>

#include "disk.h"


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
Frame* addToList(Frame* head, Frame* item) {
    if (!item) return head;
    item->nextPFN = head;
    return item;
}

// Removes `item` from the list `head` (if present).
// Returns the new head of the list.
Frame* removeFromList(Frame* head, Frame* item) {
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
    if (!headPtr || !*headPtr) return NULL;
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

boolean wipePage(ULONG64 frameNumber)
{
    if (MapUserPhysicalPages (transferVA, 1, &frameNumber) == FALSE) {

        printf ("wipePage : could not map transferVA %p to frame num address %llX\n", transferVA,
            frameNumber);

        DebugBreak();
        return false;
    }

    memset(transferVA, 0, PAGE_SIZE);

    if (MapUserPhysicalPages (transferVA, 1, NULL) == FALSE) {

        printf ("wipePage : could not unmap transferVA %p to page address %llX\n", transferVA,
            frameNumber);

        return false;
    }

    return true;
}