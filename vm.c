#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <windows.h>

#include "components/disk.h"
#include "components/pages.h"
#include "components/utilities.h"

static VOID ResetFrameListsForNewRun(VOID);
static BOOL ParseUnsignedArgument(const char* text, ULONG64* value);
static VOID RunThreadSweep(ULONG64 maxThreads, ULONG64 runsPerCount);
static VOID PrintUsage(const char* programName);

static const char* ThreadSweepResultsFile = "thread_sweep_results.txt";

//
// This define enables code that lets us create multiple virtual address
// mappings to a single physical page.  We only/need want this if/when we
// start using reference counts to avoid holding locks while performing
// pagefile I/Os - because otherwise disallowing this makes it easier to
// detect and fix unintended failures to unmap virtual addresses properly.
//

#define SUPPORT_MULTIPLE_VA_TO_SAME_PAGE 1

#pragma comment(lib, "advapi32.lib")

#if SUPPORT_MULTIPLE_VA_TO_SAME_PAGE
#pragma comment(lib, "onecore.lib")
#endif


BOOL GetPrivilege  (VOID)
{
    struct {
        DWORD Count;
        LUID_AND_ATTRIBUTES Privilege [1];
    } Info;

    //
    // This is Windows-specific code to acquire a privilege.
    // Understanding each line of it is not so important for
    // our efforts.
    //

    HANDLE hProcess;
    HANDLE Token;
    BOOL Result;

    //
    // Open the token.
    //

    hProcess = GetCurrentProcess ();

    Result = OpenProcessToken (hProcess,
                               TOKEN_ADJUST_PRIVILEGES,
                               &Token);

    if (Result == FALSE) {
        printf ("Cannot open process token.\n");
        return FALSE;
    }

    //
    // Enable the privilege.
    //

    Info.Count = 1;
    Info.Privilege[0].Attributes = SE_PRIVILEGE_ENABLED;

    //
    // Get the LUID.
    //

    Result = LookupPrivilegeValue (NULL,
                                   SE_LOCK_MEMORY_NAME,
                                   &(Info.Privilege[0].Luid));

    if (Result == FALSE) {
        printf ("Cannot get privilege\n");
        return FALSE;
    }

    //
    // Adjust the privilege.
    //

    Result = AdjustTokenPrivileges (Token,
                                    FALSE,
                                    (PTOKEN_PRIVILEGES) &Info,
                                    0,
                                    NULL,
                                    NULL);

    //
    // Check the result.
    //

    if (Result == FALSE) {
        printf ("Cannot adjust token privileges %u\n", GetLastError ());
        return FALSE;
    }

    if (GetLastError () != ERROR_SUCCESS) {
        printf ("Cannot enable the SE_LOCK_MEMORY_NAME privilege - check local policy\n");
        return FALSE;
    }

    CloseHandle (Token);

    return TRUE;
}

#if SUPPORT_MULTIPLE_VA_TO_SAME_PAGE

HANDLE
CreateSharedMemorySection (
    VOID
    )
{
    HANDLE section;
    MEM_EXTENDED_PARAMETER parameter = { 0 };

    //
    // Create an AWE section.  Later we deposit pages into it and/or
    // return them.
    //

    parameter.Type = MemSectionExtendedParameterUserPhysicalFlags;
    parameter.ULong = 0;

    section = CreateFileMapping2 (INVALID_HANDLE_VALUE,
                                  NULL,
                                  SECTION_MAP_READ | SECTION_MAP_WRITE,
                                  PAGE_READWRITE,
                                  SEC_RESERVE,
                                  0,
                                  NULL,
                                  &parameter,
                                  1);

    return section;
}

#endif

VOID malloc_test (VOID)
{
    unsigned i;
    PULONG_PTR p;
    unsigned random_number;

    p = malloc (VIRTUAL_ADDRESS_SIZE);

    if (p == NULL) {
        printf ("malloc_test : could not malloc memory\n");
        return;
    }

    for (i = 0; i < MB (1); i += 1) {

        //
        // Randomly access different portions of the virtual address
        // space we obtained above.
        //
        // If we have never accessed the surrounding page size (4K)
        // portion, the operating system will receive a page fault
        // from the CPU and proceed to obtain a physical page and
        // install a PTE to map it - thus connecting the end-to-end
        // virtual address translation.  Then the operating system
        // will tell the CPU to repeat the instruction that accessed
        // the virtual address and this time, the CPU will see the
        // valid PTE and proceed to obtain the physical contents
        // (without faulting to the operating system again).
        //

        random_number = (unsigned) (ReadTimeStampCounter() >> 4);

        random_number %= VIRTUAL_ADDRESS_SIZE_IN_UNSIGNED_CHUNKS;

        //
        // Write the virtual address into each page.  If we need to
        // debug anything, we'll be able to see these in the pages.
        //

        *(p + random_number) = (ULONG_PTR) p;
    }

    printf ("malloc_test : finished accessing %u random virtual addresses\n", i);

    //
    // Now that we're done with our memory we can be a good
    // citizen and free it.
    //

    free (p);

    return;
}

VOID commit_at_fault_time_test (VOID)
{
    unsigned i;
    PULONG_PTR p;
    PULONG_PTR committed_va;
    unsigned random_number;
    BOOL page_faulted;

    p = VirtualAlloc (NULL,
                      VIRTUAL_ADDRESS_SIZE,
                      MEM_RESERVE,
                      PAGE_NOACCESS);

    if (p == NULL) {
        printf ("commit_at_fault_time_test : could not reserve memory\n");
        return;
    }

    for (i = 0; i < MB (1); i += 1) {

        //
        // Randomly access different portions of the virtual address
        // space we obtained above.
        //
        // If we have never accessed the surrounding page size (4K)
        // portion, the operating system will receive a page fault
        // from the CPU and proceed to obtain a physical page and
        // install a PTE to map it - thus connecting the end-to-end
        // virtual address translation.  Then the operating system
        // will tell the CPU to repeat the instruction that accessed
        // the virtual address and this time, the CPU will see the
        // valid PTE and proceed to obtain the physical contents
        // (without faulting to the operating system again).
        //

        random_number = (unsigned) (ReadTimeStampCounter() >> 4);

        random_number %= VIRTUAL_ADDRESS_SIZE_IN_UNSIGNED_CHUNKS;

        //
        // Write the virtual address into each page.  If we need to
        // debug anything, we'll be able to see these in the pages.
        //

        page_faulted = FALSE;

        __try {

            *(p + random_number) = (ULONG_PTR) p;

        } __except (EXCEPTION_EXECUTE_HANDLER) {

            page_faulted = TRUE;
        }

        if (page_faulted) {

            //
            // Commit the virtual address now - if that succeeds then
            // we'll be able to access it from now on.
            //

            committed_va = p + random_number;

            committed_va = VirtualAlloc (committed_va,
                                         sizeof (ULONG_PTR),
                                         MEM_COMMIT,
                                         PAGE_READWRITE);

            if (committed_va == NULL) {
                printf ("commit_at_fault_time_test : could not commit memory\n");
                return;
            }

            //
            // No exception handler needed now since we are guaranteed
            // by virtue of our commit that the operating system will
            // honor our access.
            //

            *committed_va = (ULONG_PTR) committed_va;
        }
    }

    printf ("commit_at_fault_time_test : finished accessing %u random virtual addresses\n", i);

    //
    // Now that we're done with our memory we can be a good
    // citizen and free it.
    //

    VirtualFree (p, 0, MEM_RELEASE);

    return;
}

static VOID ResetFrameListsForNewRun(VOID)
{
    memset(&freeList, 0, sizeof(freeList));
    memset(&activeList, 0, sizeof(activeList));
    memset(&modifiedList, 0, sizeof(modifiedList));
    memset(&standbyList, 0, sizeof(standbyList));
    numActiveUserThreads = 0;
}

ULONGLONG full_virtual_memory_test(ULONG64 userThreadCount)
{
    BOOL allocated;
    BOOL privilege;
    BOOL physicalPagesAllocated = FALSE;
    HANDLE physical_page_handle = NULL;
    ULONGLONG duration = 0;
    ULONG64 totalThreadsThisRun = 0;

    if (userThreadCount == 0 || userThreadCount > NUMBER_USER_THREADS)
    {
        printf("full_virtual_memory_test : invalid user thread count %llu (max %u)\n",
               userThreadCount,
               NUMBER_USER_THREADS);
        return 0;
    }

    CurrentUserThreadCount = userThreadCount;
    ResetFrameListsForNewRun();

    // Start our timer
    startTime = GetTickCount64();

    privilege = GetPrivilege ();

    if (privilege == FALSE) {
        printf ("full_virtual_memory_test : could not get privilege\n");
        goto cleanup;
    }

#if SUPPORT_MULTIPLE_VA_TO_SAME_PAGE

    physical_page_handle = CreateSharedMemorySection ();

    if (physical_page_handle == NULL) {
        printf ("CreateFileMapping2 failed, error %#x\n", GetLastError ());
        goto cleanup;
    }

#else

    physical_page_handle = GetCurrentProcess ();

#endif

    physical_page_count = NUMBER_OF_PHYSICAL_PAGES;

    physical_page_numbers = malloc (physical_page_count * sizeof (ULONG_PTR));
    DebugCheckPageArray = malloc (physical_page_count * sizeof (ULONG_PTR));

    if (physical_page_numbers == NULL || DebugCheckPageArray == NULL) {
        printf ("full_virtual_memory_test : could not allocate array to hold physical page numbers\n");
        goto cleanup;
    }

    allocated = AllocateUserPhysicalPages (physical_page_handle,
                                           &physical_page_count,
                                           physical_page_numbers);

    if (allocated == FALSE) {
        printf ("full_virtual_memory_test : could not allocate physical pages\n");
        goto cleanup;
    }

    physicalPagesAllocated = TRUE;

    if (physical_page_count != NUMBER_OF_PHYSICAL_PAGES) {

        printf ("full_virtual_memory_test : allocated only %llu pages out of %u pages requested\n",
                physical_page_count,
                NUMBER_OF_PHYSICAL_PAGES);
    }

#if SUPPORT_MULTIPLE_VA_TO_SAME_PAGE

    sharablePhysicalPages.Type = MemExtendedParameterUserPhysicalHandle;
    sharablePhysicalPages.Handle = physical_page_handle;

    vaStartLoc = VirtualAlloc2 (NULL,
                       NULL,
                       VIRTUAL_ADDRESS_SIZE,
                       MEM_RESERVE | MEM_PHYSICAL,
                       PAGE_READWRITE,
                       &sharablePhysicalPages,
                       1);

#else

    vaStartLoc = VirtualAlloc (NULL,
                      VIRTUAL_ADDRESS_SIZE,
                      MEM_RESERVE | MEM_PHYSICAL,
                      PAGE_READWRITE);

#endif

    if (vaStartLoc == NULL) {

        printf ("full_virtual_memory_test : could not reserve memory %x\n",
                GetLastError ());

        goto cleanup;
    }

    initListsAndPFNs();
    initDiskSpace();
    initThreads();

    totalThreadsThisRun = CurrentUserThreadCount + NUMBER_TRIM_THREADS + NUMBER_DISK_THREADS;

    HANDLE threadFinishedEvents[TOTAL_NUMBER_OF_THREADS] = { 0 };

    for (ULONG64 i = 0; i < totalThreadsThisRun; i++)
    {
        threadFinishedEvents[i] = threadInfoArray[i].ThreadHandle;
    }

    WaitForMultipleObjects((DWORD)totalThreadsThisRun, threadFinishedEvents, TRUE, INFINITE);

    endTime = GetTickCount64();
    duration = endTime - startTime;
    printf ("full_virtual_memory_test : time taken %llums\n", duration);

cleanup:
    cleanupThreadContexts(totalThreadsThisRun);
    destroyEvents();
    deleteCriticalSections();

    if (vaStartLoc != NULL) {
        VirtualFree (vaStartLoc, 0, MEM_RELEASE);
        vaStartLoc = NULL;
    }

    if (transferVA != NULL) {
        VirtualFree (transferVA, 0, MEM_RELEASE);
        transferVA = NULL;
    }

    if (writeTransferVA != NULL) {
        VirtualFree (writeTransferVA, 0, MEM_RELEASE);
        writeTransferVA = NULL;
    }

    if (totalDiskSpace != NULL) {
        free(totalDiskSpace);
        totalDiskSpace = NULL;
    }

    if (diskSlotBitmap != NULL) {
        free((void*)diskSlotBitmap);
        diskSlotBitmap = NULL;
        diskSlotBitmapLength = 0;
    }

    diskSearchStartIndex = 0;
    currentTransferVAIndex = 0;

    if (pageTable != NULL) {
        free(pageTable);
        pageTable = NULL;
    }

    if (pfnArray != NULL) {
        VirtualFree(pfnArray, 0, MEM_RELEASE);
        pfnArray = NULL;
    }

    if (DebugCheckPageArray != NULL) {
        free(DebugCheckPageArray);
        DebugCheckPageArray = NULL;
    }

    if (physical_page_numbers != NULL) {
        if (physicalPagesAllocated && physical_page_handle != NULL) {
            ULONG_PTR pagesToFree = physical_page_count;
            FreeUserPhysicalPages(physical_page_handle, &pagesToFree, physical_page_numbers);
        }
        free (physical_page_numbers);
        physical_page_numbers = NULL;
    }

    if (physical_page_handle != NULL && physical_page_handle != GetCurrentProcess ()) {
        CloseHandle (physical_page_handle);
    }

    ResetFrameListsForNewRun();

    return duration;
}

VOID 
main (
    int argc,
    char** argv
    )
{
    if (argc >= 3)
    {
        ULONG64 maxUserThreads = 0;
        ULONG64 runsPerThreadCount = 0;

        if (!ParseUnsignedArgument(argv[1], &maxUserThreads) ||
            !ParseUnsignedArgument(argv[2], &runsPerThreadCount))
        {
            PrintUsage(argv[0]);
            return;
        }

        if (maxUserThreads == 0 || maxUserThreads > NUMBER_USER_THREADS)
        {
            printf("Max user threads must be between 1 and %u\n", NUMBER_USER_THREADS);
            return;
        }

        if (runsPerThreadCount == 0)
        {
            printf("Runs per thread count must be at least 1\n");
            return;
        }

        RunThreadSweep(maxUserThreads, runsPerThreadCount);
        return;
    }

    full_virtual_memory_test (NUMBER_USER_THREADS);
    return;
}

static BOOL ParseUnsignedArgument(const char* text, ULONG64* value)
{
    char* end = NULL;
    errno = 0;
    unsigned long long parsed = _strtoui64(text, &end, 10);

    if (errno != 0 || end == text || *end != '\0')
    {
        return FALSE;
    }

    *value = parsed;
    return TRUE;
}

static VOID RunThreadSweep(ULONG64 maxThreads, ULONG64 runsPerCount)
{
    if (maxThreads == 0 || runsPerCount == 0)
    {
        return;
    }

    FILE* resultsFile = NULL;
    errno_t fileError = fopen_s(&resultsFile, ThreadSweepResultsFile, "w");

    if (fileError != 0 || resultsFile == NULL)
    {
        printf("Could not open %s for writing (error %d)\n", ThreadSweepResultsFile, fileError);
    }
    else
    {
        fprintf(resultsFile, "%-14s%-14s%-14s\n", "thread_count", "runs", "avg_ms");
    }

    for (ULONG64 threadCount = 1; threadCount <= maxThreads; threadCount++)
    {
        ULONGLONG totalDuration = 0;

        for (ULONG64 run = 0; run < runsPerCount; run++)
        {
            ULONGLONG duration = full_virtual_memory_test(threadCount);
            totalDuration += duration;
        }

        double averageMs = (double) totalDuration / (double) runsPerCount;
        printf("Average time for %llu user thread(s) across %llu run(s): %.2f ms\n",
               threadCount,
               runsPerCount,
               averageMs);

        if (resultsFile)
        {
            fprintf(resultsFile, "%-14llu%-14llu%-14.2f\n",
                    threadCount,
                    runsPerCount,
                    averageMs);
            fflush(resultsFile);
        }
    }

    if (resultsFile)
    {
        fclose(resultsFile);
        printf("Wrote sweep results to %s\n", ThreadSweepResultsFile);
    }
}

static VOID PrintUsage(const char* programName)
{
    printf("Usage: %s <max_user_threads (1-%u)> <runs_per_thread_count>\n",
           programName,
           NUMBER_USER_THREADS);
}
