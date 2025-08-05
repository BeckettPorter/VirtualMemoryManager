//
// Created by porte on 7/25/2025.
//

#include "trimThread.h"

#include <sal.h>
#include <stdbool.h>
#include <windows.h>
#include "../pages.h"


ULONG trimThread(_In_ PVOID Context)
{
    HANDLE eventsToWaitOn[2];
    eventsToWaitOn[0] = trimEvent;
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

        evictFrame();

        // #todo bp: if we want to add another trim thread, will probably need to make this manual reset.
        SetEvent(modWriteEvent);
    }

    return 0;
}
