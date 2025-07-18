//
// Created by porte on 6/20/2025.
//

#ifndef DISK_H
#define DISK_H
#include "initialize.h"


unsigned char* totalDiskSpace;
boolean* freeDiskSpace;
ULONG64 lastUsedFreeDiskSlot;
void* transferVA;
ULONG64 numAttemptedModWrites;



VOID swapToDisk();

ULONG64 findFreeDiskSlot();

VOID swapFromDisk(Frame* frameToFill, ULONG64 diskIndexToTransferFrom);

ULONG64* diskSlotsToBatch;

#endif //DISK_H
