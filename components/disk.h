//
// Created by porte on 6/20/2025.
//

#ifndef DISK_H
#define DISK_H
#include "initialize.h"


#define INVALID_DISK_SLOT ((ULONG64)-1)

unsigned char* totalDiskSpace;
boolean* freeDiskSpace;
ULONG64 diskSearchStartIndex;
ULONG64 numAttemptedModWrites;



VOID swapToDisk();

ULONG64 findFreeDiskSlot();

VOID swapFromDisk(Frame* frameToFill, ULONG64 diskIndexToTransferFrom, PVOID context);

ULONG64* diskSlotsToBatch;

#endif //DISK_H
