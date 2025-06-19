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
#include <windows.h>


// Structs
typedef struct {
    // Can change to uint8_t later maybe to save space, fine for now tho.
    // Also, if it is true, this is a valid PTE. If false, means it isn't or was swapped to disk.
    boolean isValid;

    // Physical frame number, or -1 if not mapped
    int pageFrameNumber;

    // If swapped to disk, where is it, or -1 if not swapped
    int disk_index;
} PageTableEntry;


// Frame is on ram, PAGES are on ram, but then copied to disk when
typedef struct Frame
{
    ULONG64 physicalFrameNumber;
    struct Frame* nextPFN;
    PageTableEntry* PTE;
} Frame;


// Variables
PageTableEntry* pageTable;
void* vaStartLoc;
PULONG_PTR physical_page_numbers;
ULONG_PTR physical_page_count;
Frame* pfnArray;

Frame* freeList;
Frame* activeList;


// Functions
PageTableEntry* VAToPageTableEntry(void* virtualAddress);
void* PageTableEntryToVA(PageTableEntry* entry);

#endif //UTILITIES_H
