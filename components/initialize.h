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
VOID destroyEvents();
VOID deleteCriticalSections();
VOID cleanupThreadContexts(ULONG64 totalThreads);

HANDLE createNewThread(LPTHREAD_START_ROUTINE ThreadFunction, PTHREAD_INFO ThreadContext);

#endif //INITIALIZE_H
