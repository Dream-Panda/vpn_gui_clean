#pragma once
#include <windows.h>
#include <string>
#include "ProcessOptions.h"

class ProcessRunner {
public:
    ProcessRunner();
    ~ProcessRunner();

    bool start(const ProcessOptions& opt, std::wstring* lastError = nullptr);
    void stop(DWORD exitCode = 0);
    bool running() const;
    DWORD pid() const { return pi_.dwProcessId; }

private:
    PROCESS_INFORMATION pi_{};
    HANDLE job_{ nullptr };
    static std::wstring buildCmdLine(const ProcessOptions& opt);
    static void closeHandleSafe(HANDLE& h);
};
