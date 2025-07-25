//
// Created by porte on 6/20/2025.
//

#ifndef INITIALIZE_H
#define INITIALIZE_H
#include "utilities.h"


VOID initListsAndPFNs();

VOID initDiskSpace();

VOID initThreads();

VOID createEvents();
VOID initCriticalSections();
VOID createThreads();

HANDLE createNewThread(LPTHREAD_START_ROUTINE ThreadFunction, PTHREAD_INFO ThreadContext);

HANDLE threadFinishedEvents[TOTAL_NUMBER_OF_THREADS];

#endif //INITIALIZE_H
