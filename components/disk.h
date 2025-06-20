//
// Created by porte on 6/20/2025.
//

#ifndef DISK_H
#define DISK_H
#include "initialize.h"


unsigned char* totalDiskSpace;
boolean* freeDiskSpace;


VOID swapToDisk(PageTableEntry* pageToSwap);


#endif //DISK_H
