//
// Created by porte on 6/20/2025.
//

#ifndef PAGES_H
#define PAGES_H
#include "initialize.h"


Frame* getFreeFrame();

VOID releaseFrame();

Frame* evictFrame();

Frame* findFrameFromFrameNumber(ULONG64 frameNumber);

// Processes all pages in the modified list, writing them to disk and moving them to the standby list
VOID modifiedPageWrite();











#endif //PAGES_H