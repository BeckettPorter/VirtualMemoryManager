//
// Created by porte on 6/19/2025.
//

#ifndef UTILITIES_H
#define UTILITIES_H


#define PAGE_SIZE                   4096
#define MB(x)                       ((x) * 1024 * 1024)
#define VIRTUAL_ADDRESS_SIZE        MB(16)
#define VIRTUAL_ADDRESS_SIZE_IN_UNSIGNED_CHUNKS        (VIRTUAL_ADDRESS_SIZE / sizeof (ULONG_PTR))
#define NUMBER_OF_PHYSICAL_PAGES   ((VIRTUAL_ADDRESS_SIZE / PAGE_SIZE) / 64)
#define NUMBER_OF_VIRTUAL_PAGES (VIRTUAL_ADDRESS_SIZE / PAGE_SIZE)
#include <windows.h>


// Structs
typedef struct {
    ULONG64 isValid: 1;

    ULONG64 isTransitionFormat: 1;

    ULONG64 pageFrameNumber: 40;
} validPTE;

typedef struct {
    ULONG64 mustBeZero: 1;

    ULONG64 isTransitionFormat: 1;

    ULONG64 diskIndex: 40;
} invalidPTE;

typedef struct {
    ULONG64 mustBeZero: 1;

    ULONG64 isTransitionFormat: 1;

    ULONG64 pageFrameNumber: 40;
} transitionPTE;

// PTE Union
typedef union
{
    validPTE validFormat;
    invalidPTE invalidFormat;
    transitionPTE transitionFormat;
    ULONG64 entireFormat;
} PageTableEntry;


// Frame is on ram, PAGES are on ram, but then copied to disk when
typedef struct Frame
{
    ULONG64 physicalFrameNumber;
    struct Frame* nextPFN;
    PageTableEntry* PTE;
    // If this is 1, we are on the modified list, otherwise on the standby list.
    ULONG64 isOnModifiedList: 1;
    ULONG64 diskIndex: 40;
} Frame;


// Variables
PageTableEntry* pageTable;
void* vaStartLoc;
PULONG_PTR physical_page_numbers;
ULONG_PTR physical_page_count;
Frame* pfnArray;

Frame* freeList;
Frame* activeList;
Frame* modifiedList;
Frame* standbyList;


// Functions
PageTableEntry* VAToPageTableEntry(void* virtualAddress);
void* PageTableEntryToVA(PageTableEntry* entry);

Frame* addToList(Frame* head, Frame* item);
Frame* removeFromList(Frame* head, Frame* item);
Frame* popFirstFrame(Frame** headPtr);

VOID checkVa(PULONG64 va);

boolean wipePage(ULONG64 frameNumber);

#endif //UTILITIES_H
