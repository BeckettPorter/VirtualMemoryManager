//
// Created by porte on 7/25/2025.
//

#include "diskThread.h"

#include <sal.h>
#include <stdbool.h>
#include <windows.h>
#include "../pages.h"


ULONG diskThread(_In_ PVOID Context)
{
    HANDLE eventsToWaitOn[2];
    eventsToWaitOn[0] = modWriteEvent;
    eventsToWaitOn[SHUTDOWN_PROGRAM_EVENT_INDEX] = shutdownProgramEvent;

    while (true)
    {
        ULONG64 eventIndex = WaitForMultipleObjects(
            ARRAYSIZE(eventsToWaitOn), eventsToWaitOn, false, INFINITE);

        // Check if we called the shutdown program event, and return if so.
        if (eventIndex == SHUTDOWN_PROGRAM_EVENT_INDEX)
        {
            return 0;
        }

        modifiedPageWrite();

        // We need to acquire the standby list lock so we can't set the event and
        // immidiately reset it in user thread.
        acquireLock(&standbyListLock);
        SetEvent(finishedModWriteEvent);
        releaseLock(&standbyListLock);
    }

    return 0;
}
