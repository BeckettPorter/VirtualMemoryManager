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

// Evicts a page and moves it from the active list to the free list.
VOID tryModifiedPageWrite();

ULONG64 findFrameNumberFromFrame(Frame* frame);










#endif //PAGES_H