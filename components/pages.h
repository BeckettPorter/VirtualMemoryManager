//
// Created by porte on 6/20/2025.
//

#ifndef PAGES_H
#define PAGES_H
#include "initialize.h"


Frame* getFreeFrame();

VOID releaseFrame();

Frame* evictFrame();

// Evicts a page and moves it from the active list to the free list.
VOID trimPage();











#endif //PAGES_H