//
// Created by porte on 6/20/2025.
//

#include "initialize.h"

#include <stdio.h>

#include "disk.h"
#include "utilities.h"
#include "threads/diskThread.h"
#include "threads/trimThread.h"
#include "threads/userThread.h"


MEM_EXTENDED_PARAMETER sharablePhysicalPages = { 0 };

VOID initListsAndPFNs()
{
    ULONG64 numBytes = NUMBER_OF_VIRTUAL_PAGES * sizeof(PageTableEntry);

    pageTable = malloc(numBytes);

    ASSERT(pageTable);

    memset(pageTable, 0, numBytes);

    // Find the highest PFN in the PFN array.
    ULONG64 maxPFN = 0;
    for (ULONG64 i = 0; i < physical_page_count; ++i) {
        if (physical_page_numbers[i] > maxPFN) {
            maxPFN = physical_page_numbers[i];
        }
    }

    ULONG64 pfnArraySize = (maxPFN + 1) * sizeof(Frame);

    // build PFN list only from actually owned pages
    pfnArray = VirtualAlloc(
        NULL,
        pfnArraySize,
        MEM_RESERVE,
        PAGE_READWRITE);

    // For each physical page we have, commit the memory for it in the sparse array.
    for (ULONG64 i = 0; i < physical_page_count; i++)
    {
        ULONG64 currentFrameNumber = physical_page_numbers[i];

        // Go through our pfnArray and actually commit where we have frames.
        VirtualAlloc (
            pfnArray + currentFrameNumber,
            sizeof(Frame),
            MEM_COMMIT,
            PAGE_READWRITE);

        pfnArray[currentFrameNumber].nextPFN = freeList.headFrame;
        freeList.length++;
        freeList.headFrame = &pfnArray[currentFrameNumber];
        pfnArray[currentFrameNumber].PTE = NULL;
        pfnArray[currentFrameNumber].isOnModifiedList = 0;
        pfnArray[currentFrameNumber].diskIndex = INVALID_DISK_SLOT;
    }
}

VOID initDiskSpace()
{
    currentTransferVAIndex = 0;

    transferVA = VirtualAlloc2 (NULL,
                       NULL,
                       PAGE_SIZE * TRANSFER_VA_COUNT,
                       MEM_RESERVE | MEM_PHYSICAL,
                       PAGE_READWRITE,
                       &sharablePhysicalPages,
                       1);

    writeTransferVA = VirtualAlloc2 (NULL,
                       NULL,
                       PAGE_SIZE * MAX_WRITE_PAGES,
                       MEM_RESERVE | MEM_PHYSICAL,
                       PAGE_READWRITE,
                       &sharablePhysicalPages,
                       1);

    // Sanity: virtual must be larger than physical
    ASSERT(NUMBER_OF_VIRTUAL_PAGES > NUMBER_OF_PHYSICAL_PAGES);

    // Allocate disk space sized to the number of slots
    ULONG64 diskBytes = (ULONG64)NUMBER_OF_DISK_SLOTS * (ULONG64)PAGE_SIZE;
    totalDiskSpace = malloc((size_t)diskBytes);

    diskSlotBitmapLength = (NUMBER_OF_DISK_SLOTS + 63) / 64;
    size_t bitmapBytes = (size_t)diskSlotBitmapLength * sizeof(LONG64);
    diskSlotBitmap = malloc(bitmapBytes);
    ASSERT(diskSlotBitmap != NULL);
    memset((void*)diskSlotBitmap, 0, bitmapBytes);

    diskSearchStartIndex = 0;

    numAttemptedModWrites = 0;
}


VOID initThreads()
{
    createEvents();
    initCriticalSections();
    createThreads();


    WaitForSingleObject (shutdownProgramEvent, INFINITE);

}

VOID createEvents()
{
    // Create the event that will be used to signal the user threads to exit.
    trimEvent = CreateEvent (NULL, AUTO_RESET, EVENT_START_OFF, NULL);
    stopTrimmingEvent = CreateEvent (NULL, AUTO_RESET, EVENT_START_OFF, NULL);
    modWriteEvent = CreateEvent (NULL, AUTO_RESET, EVENT_START_OFF, NULL);
    finishedModWriteEvent = CreateEvent (NULL, MANUAL_RESET, EVENT_START_OFF, NULL);

    shutdownProgramEvent = CreateEvent (NULL, MANUAL_RESET, EVENT_START_OFF, NULL);
}

VOID initCriticalSections()
{
    InitializeCriticalSection (&freeListLock);
    InitializeCriticalSection (&activeListLock);
    InitializeCriticalSection (&modifiedListLock);
    InitializeCriticalSection (&standbyListLock);
    InitializeCriticalSection (&threadCountLock);

    for (ULONG64 i = 0; i < PTE_LOCK_REGION_COUNT; i++)
    {
        InitializeCriticalSection (&pteLockTable[i]);
    }
}

VOID createThreads()
{
    ULONG64 currentThreadNumber = 0;

    PTHREAD_INFO currentThreadInfo;


    // Create the user threads.
    numActiveUserThreads = 0;
    for (ULONG64 i = 0; i < NUMBER_USER_THREADS; i++)
    {
        currentThreadInfo = &threadInfoArray[currentThreadNumber];
        currentThreadInfo->ThreadNumber = currentThreadNumber;

        // Initilize per thread transfer VAs
        currentThreadInfo->perThreadTransferVAs = VirtualAlloc2 (NULL,
                       NULL,
                       PAGE_SIZE * TRANSFER_VA_COUNT,
                       MEM_RESERVE | MEM_PHYSICAL,
                       PAGE_READWRITE,
                       &sharablePhysicalPages,
                       1);

        currentThreadInfo->transferVAIndex = 0;

        currentThreadInfo->ThreadHandle = createNewThread(userThread, currentThreadInfo);

        userThreadHandles[i] = currentThreadInfo->ThreadHandle;

        numActiveUserThreads++;

        currentThreadNumber++;
    }

    // Create the trim threads.
    for (ULONG64 i = 0; i < NUMBER_TRIM_THREADS; i++)
    {
        currentThreadInfo = &threadInfoArray[currentThreadNumber];
        currentThreadInfo->ThreadNumber = currentThreadNumber;

        currentThreadInfo->ThreadHandle = createNewThread(trimThread, currentThreadInfo);

        currentThreadNumber++;
    }

    // Create the disk threads.
    for (ULONG64 i = 0; i < NUMBER_DISK_THREADS; i++)
    {
        currentThreadInfo = &threadInfoArray[currentThreadNumber];
        currentThreadInfo->ThreadNumber = currentThreadNumber;

        currentThreadInfo->ThreadHandle = createNewThread(diskThread, currentThreadInfo);

        currentThreadNumber++;
    }
}





HANDLE createNewThread(LPTHREAD_START_ROUTINE ThreadFunction, PTHREAD_INFO ThreadContext)
{
    HANDLE Handle;
    BOOL ReturnValue;

    Handle = CreateThread(DEFAULT_SECURITY,
                           DEFAULT_STACK_SIZE,
                           ThreadFunction,
                           ThreadContext,
                           DEFAULT_CREATION_FLAGS,
                           &ThreadContext->ThreadId);

    if (Handle == NULL) {
        ReturnValue = GetLastError ();
        printf ("could not create thread %x\n", ReturnValue);
        return NULL;
    }

    return Handle;
}