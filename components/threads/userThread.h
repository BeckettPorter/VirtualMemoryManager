//
// Created by porte on 7/25/2025.
//

#ifndef USERTHREAD_H
#define USERTHREAD_H
#include <sal.h>
#include <windows.h>



ULONG userThread(_In_ PVOID Context);

VOID resolvePageFault(PULONG_PTR arbitrary_va);

#endif //USERTHREAD_H