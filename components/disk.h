//
// Created by porte on 6/20/2025.
//

#ifndef DISK_H
#define DISK_H
#include "initialize.h"


unsigned char* totalDiskSpace;
boolean* freeDiskSpace;
void* transferVA;


VOID swapToDisk(PageTableEntry* pageToSwap);

ULONG64 findFreeDiskSlot();

VOID swapFromDisk(Frame* frameToFill, ULONG64 diskIndexToTransferFrom);

typedef struct ULONG64Node
{
    ULONG64 value;
    ListEntry listEntry;
} ULONG64Node;

ULONG64Node* createNode(ULONG64 value);

ListEntry diskSlotsArrayList;

#endif //DISK_H
