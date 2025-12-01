//
// Created by porte on 6/20/2025.
//

#ifndef DISK_H
#define DISK_H
#include "initialize.h"


#define INVALID_DISK_SLOT ((ULONG64)-1)

unsigned char* totalDiskSpace;
volatile LONG64* diskSlotBitmap;
ULONG64 diskSlotBitmapLength;
ULONG64 diskSearchStartIndex;
ULONG64 numAttemptedModWrites;



VOID swapToDisk();

ULONG64 findFreeDiskSlot();

VOID swapFromDisk(Frame* frameToFill, ULONG64 diskIndexToTransferFrom, PVOID context);

VOID ReleaseDiskSlot(ULONG64 slotIndex);
BOOLEAN IsDiskSlotInUse(ULONG64 slotIndex);

#endif //DISK_H
