//
// Created by porte on 6/19/2025.
//

#ifndef UTILITIES_H
#define UTILITIES_H


#define PAGE_SIZE                                       (4096)
#define KB(x)                                           ((x) * 1024)
#define MB(x)                                           ((x) * 1024 * 1024)
#define GB(x)                                           ((x) * 1024ULL * 1024 * 1024)


// ratio of physical to virtual memory
#define PHYS_TO_VIRTUAL_RATIO                           (0.8)

#define NUMBER_OF_PHYSICAL_PAGES                        (GB(1) / PAGE_SIZE)

#define NUMBER_OF_VIRTUAL_PAGES                         ((ULONG64)((NUMBER_OF_PHYSICAL_PAGES) / (PHYS_TO_VIRTUAL_RATIO)))

#define VIRTUAL_ADDRESS_SIZE                            (NUMBER_OF_VIRTUAL_PAGES * PAGE_SIZE)

#define VIRTUAL_ADDRESS_SIZE_IN_UNSIGNED_CHUNKS         (VIRTUAL_ADDRESS_SIZE / sizeof (ULONG_PTR))


#define MAX_WRITE_PAGES                                 (512)
#define TRANSFER_VA_COUNT                               (512)
#define NUMBER_OF_DISK_SLOTS                            (NUMBER_OF_VIRTUAL_PAGES)

#define TEST_ITERATIONS MB                              (111)


#define PTE_LOCK_REGION_COUNT                           (4096)
#define PTE_REGION_SIZE                                 (NUMBER_OF_VIRTUAL_PAGES / PTE_LOCK_REGION_COUNT)


// Threads
#define AUTO_RESET                                      FALSE
#define MANUAL_RESET                                    TRUE
#define WAIT_FOR_ALL                                    TRUE
#define WAIT_FOR_ONE                                    FALSE
#define DEFAULT_SECURITY                                ((LPSECURITY_ATTRIBUTES) NULL)
#define DEFAULT_STACK_SIZE                              0
#define DEFAULT_CREATION_FLAGS                          0
#define EVENT_START_ON                                  TRUE
#define EVENT_START_OFF                                 FALSE

#define NUMBER_USER_THREADS                             (8)
#define NUMBER_TRIM_THREADS                             (1)
#define NUMBER_DISK_THREADS                             (1)
#define TOTAL_NUMBER_OF_THREADS                         (NUMBER_USER_THREADS + NUMBER_TRIM_THREADS + NUMBER_DISK_THREADS)

// Thread types
#define USER_THREAD                                     (0)
#define TRIM_THREAD                                     (1)
#define DISK_THREAD                                     (2)

#define SHUTDOWN_PROGRAM_EVENT_INDEX                    (1)

#define ASSERT(x) if (!(x)) { DebugBreak(); }
#include <windows.h>


// Structs
typedef struct {
    ULONG64 isValid: 1;

    ULONG64 isTransitionFormat: 1;

    ULONG64 pageFrameNumber: 40;

    ULONG64 reserveBuffer: 22;
} validPTE;

typedef struct {
    ULONG64 mustBeZero: 1;

    ULONG64 isTransitionFormat: 1;

    ULONG64 diskIndex: 40;

    ULONG64 reserveBuffer: 22;
} invalidPTE;

typedef struct {
    ULONG64 mustBeZero: 1;

    ULONG64 isTransitionFormat: 1;

    ULONG64 pageFrameNumber: 40;

    ULONG64 reserveBuffer: 22;
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
    ULONG64 isOnStandbyList: 1;
    ULONG64 diskIndex: 40;
    ULONG64 isBeingWritten: 1;
} Frame;


typedef struct _THREAD_INFO {

    ULONG ThreadNumber;

    ULONG ThreadId;
    HANDLE ThreadHandle;

    ULONG ThreadType;

    PVOID perThreadTransferVAs;
    ULONG64 transferVAIndex;

#if 0

    //
    // What effect would consuming extra space here have ?
    //

    volatile UCHAR Pad[32];

#endif

} THREAD_INFO, *PTHREAD_INFO;

typedef struct frameListHead
{
    Frame* headFrame;
    Frame* tailFrame;
    ULONG64 length;
} frameListHead;

// Variables
PageTableEntry* pageTable;
void* vaStartLoc;
PULONG_PTR physical_page_numbers;
PULONG_PTR DebugCheckPageArray;
ULONG_PTR physical_page_count;
Frame* pfnArray;

// Theres a function called CONTAINING_RECORD that takes a pointer, the NAME of a structure, and a field name.
// Return value is the pointer to the start of the structure.
// #TODO bp: Should change this to struct LIST HEAD with List entry, counter for length, and lock.
frameListHead freeList;
frameListHead activeList;
frameListHead modifiedList;
frameListHead standbyList;

void* transferVA;
void* writeTransferVA;
ULONG64 currentTransferVAIndex;

// Timer variables
ULONG64 startTime;
ULONG64 endTime;

extern MEM_EXTENDED_PARAMETER sharablePhysicalPages;

// Thread variables
THREAD_INFO threadInfoArray[TOTAL_NUMBER_OF_THREADS];

HANDLE userThreadHandles[NUMBER_USER_THREADS];


// Thread events
HANDLE trimEvent;
HANDLE stopTrimmingEvent;
HANDLE modWriteEvent;
HANDLE finishedModWriteEvent;

HANDLE shutdownProgramEvent;

ULONG64 numActiveUserThreads;


// Locks
CRITICAL_SECTION freeListLock;
CRITICAL_SECTION activeListLock;
CRITICAL_SECTION modifiedListLock;
CRITICAL_SECTION standbyListLock;
CRITICAL_SECTION threadCountLock;

CRITICAL_SECTION pteLockTable[PTE_LOCK_REGION_COUNT];

CRITICAL_SECTION* GetPTELock(PageTableEntry* pte);

// LOCK HEIRARCHY
// 1. PTE locks
// 2. List locks
// - 2.1. freeListLock
// - 2.2. activeListLock
// - 2.3. modifiedListLock
// - 2.4. standbyListLock




// Functions
PageTableEntry* VAToPageTableEntry(void* virtualAddress);
void* PageTableEntryToVA(PageTableEntry* entry);

VOID addToFrameList(frameListHead* head, Frame* item);
VOID addToFrameListTail(frameListHead* head, Frame* item);
VOID removeFromFrameList(frameListHead* headList, Frame* item);
Frame* popFirstFrame(frameListHead* headPtr);
BOOL listContains(frameListHead* headList, Frame* item);

VOID validateFrameList(frameListHead* head);


VOID checkVa(PULONG64 va);

boolean wipePage(Frame* frameToWipe, PTHREAD_INFO context);

PVOID acquireTransferVA(PTHREAD_INFO context);
VOID releaseTransferVALock();
VOID flushTransferVAs(PVOID allThreadTransferVAs);

VOID shutdownUserThread(int userThreadIndex);

VOID acquireLock(CRITICAL_SECTION* lock);
VOID releaseLock(CRITICAL_SECTION* lock);
BOOL tryAcquireLock(CRITICAL_SECTION* lock);

#endif //UTILITIES_H
