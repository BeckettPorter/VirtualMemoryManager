//
// Created by porte on 6/19/2025.
//

#ifndef UTILITIES_H
#define UTILITIES_H


#define PAGE_SIZE                   4096
#define MB(x)                       ((x) * 1024 * 1024)
#define VIRTUAL_ADDRESS_SIZE        MB(16)
#define VIRTUAL_ADDRESS_SIZE_IN_UNSIGNED_CHUNKS        (VIRTUAL_ADDRESS_SIZE / sizeof (ULONG_PTR))
#define NUMBER_OF_PHYSICAL_PAGES   ((VIRTUAL_ADDRESS_SIZE / PAGE_SIZE) / 64)
#define NUMBER_OF_VIRTUAL_PAGES (VIRTUAL_ADDRESS_SIZE / PAGE_SIZE)
#define MAX_WRITE_PAGES 16
#define TRANSFER_VA_COUNT 16
#define NUMBER_OF_DISK_SLOTS 4096

// Threads
#define AUTO_RESET              FALSE
#define MANUAL_RESET            TRUE
#define WAIT_FOR_ALL            TRUE
#define WAIT_FOR_ONE            FALSE
#define DEFAULT_SECURITY        ((LPSECURITY_ATTRIBUTES) NULL)
#define DEFAULT_STACK_SIZE      0
#define DEFAULT_CREATION_FLAGS  0
#define EVENT_START_ON          TRUE
#define EVENT_START_OFF         FALSE

#define NUMBER_USER_THREADS 1
#define NUMBER_TRIM_THREADS 1
#define NUMBER_DISK_THREADS 1
#define TOTAL_NUMBER_OF_THREADS (NUMBER_USER_THREADS + NUMBER_TRIM_THREADS + NUMBER_DISK_THREADS)

// Thread types
#define USER_THREAD 0
#define TRIM_THREAD 1
#define DISK_THREAD 2

#define SHUTDOWN_PROGRAM_EVENT_INDEX 1

#define ASSERT(x) if (!(x)) { DebugBreak(); }
#include <windows.h>


// Structs
typedef struct {
    ULONG64 isValid: 1;

    ULONG64 isTransitionFormat: 1;

    ULONG64 pageFrameNumber: 40;
} validPTE;

typedef struct {
    ULONG64 mustBeZero: 1;

    ULONG64 isTransitionFormat: 1;

    ULONG64 diskIndex: 40;
} invalidPTE;

typedef struct {
    ULONG64 mustBeZero: 1;

    ULONG64 isTransitionFormat: 1;

    ULONG64 pageFrameNumber: 40;
} transitionPTE;

// PTE Union
typedef union
{
    validPTE validFormat;
    invalidPTE invalidFormat;
    transitionPTE transitionFormat;
    ULONG64 entireFormat;
} PageTableEntry;

// Frame is on ram, PAGES are on ram, but then copied to disk when
typedef struct Frame
{
    struct Frame* nextPFN;
    PageTableEntry* PTE;
    // If this is 1, we are on the modified list, otherwise on the standby list.
    ULONG64 isOnModifiedList: 1;
    ULONG64 diskIndex: 40;
} Frame;


typedef struct _THREAD_INFO {

    ULONG ThreadNumber;

    ULONG ThreadId;
    HANDLE ThreadHandle;

    ULONG ThreadType;

#if 0

    //
    // What effect would consuming extra space here have ?
    //

    volatile UCHAR Pad[32];

#endif

} THREAD_INFO, *PTHREAD_INFO;

// Variables
PageTableEntry* pageTable;
void* vaStartLoc;
PULONG_PTR physical_page_numbers;
ULONG_PTR physical_page_count;
Frame* pfnArray;

Frame* freeList;
Frame* activeList;
Frame* modifiedList;
ULONG64 modifiedListLength;
Frame* standbyList;

void* transferVA;
ULONG64 currentTransferVAIndex;

// Timer variables
ULONG64 startTime;
ULONG64 endTime;

extern MEM_EXTENDED_PARAMETER sharablePhysicalPages;

// Thread variables
THREAD_INFO threadInfoArray[TOTAL_NUMBER_OF_THREADS];


// Thread events
HANDLE trimEvent;
HANDLE stopTrimmingEvent;
HANDLE modWriteEvent;
HANDLE finishedModWriteEvent;

HANDLE shutdownProgramEvent;



// Functions
PageTableEntry* VAToPageTableEntry(void* virtualAddress);
void* PageTableEntryToVA(PageTableEntry* entry);

Frame* addToFrameList(Frame* head, Frame* item);
Frame* removeFromFrameList(Frame* head, Frame* item);
Frame* popFirstFrame(Frame** headPtr);


VOID checkVa(PULONG64 va);

boolean wipePage(Frame* frameToWipe);

PVOID acquireTransferVA();
VOID flushTransferVAs();


#endif //UTILITIES_H
