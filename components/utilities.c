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

boolean wipePage(Frame* frameToWipe)
{
    ULONG64 frameNumberToWipe = findFrameNumberFromFrame(frameToWipe);
    if (MapUserPhysicalPages (transferVA, 1, &frameNumberToWipe) == FALSE) {

        printf ("wipePage : could not map transferVA %p to frame num %llX\n", transferVA,
            frameNumberToWipe);

        DebugBreak();
        return false;
    }

    memset(transferVA, 0, PAGE_SIZE);

    if (MapUserPhysicalPages (transferVA, 1, NULL) == FALSE) {

        printf ("wipePage : could not unmap transferVA %p from frame num %llX\n", transferVA,
            frameNumberToWipe);

        return false;
    }

    return true;
}


VOID addListEntry(ListEntry* head, ListEntry* entry) {
    // If the list is empty, add the entry to the head and tail.
    if (head->flink == NULL)
    {
        head->flink = head;
        head->blink = head;
    }

    entry->flink      = head;
    entry->blink      = head->blink;
    head->blink->flink = entry;
    head->blink       = entry;
}


ListEntry* removeListEntry(ListEntry* entry) {
    entry->blink->flink = entry->flink;
    entry->flink->blink = entry->blink;
    entry->flink = entry->blink = NULL;
    return entry;
}
