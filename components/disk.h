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

VOID swapFromDisk(Frame* frameToFill);

#endif //DISK_H
