#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

class ProcessRunner {
public:
    ProcessRunner();
    ~ProcessRunner();

    // exePath/args/workingDir are wide strings.
    bool Start(const std::wstring& exePath,
        const std::wstring& args = L"",
        const std::wstring& workingDir = L"");
    void Stop(bool force = true, unsigned killTimeoutMs = 1500);
    bool IsRunning() const { return running_; }

    // Return newly appended output as UTF-8.
    std::string ConsumeNewOutput();
    void ClearLog();

private:
    void ReaderThreadProc_(HANDLE readHandle);
    void WaitThreadProc_(HANDLE hProcess);
    static bool MakePipe_(HANDLE& hRead, HANDLE& hWrite, bool writeInheritable);
    static std::wstring QuoteIfNeeded_(const std::wstring& s);
    static std::string BytesToUtf8Fallback_(const char* data, size_t n);

private:
    std::atomic<bool> running_{ false };
    std::atomic<bool> stopRequested_{ false };

    HANDLE hProcess_{ nullptr };
    HANDLE hThread_{ nullptr };

    HANDLE hStdOutR_{ nullptr }, hStdOutW_{ nullptr };
    HANDLE hStdErrR_{ nullptr }, hStdErrW_{ nullptr };

    std::thread tOut_, tErr_, tWait_;

    std::mutex logMtx_;
    std::string logBuf_;
    size_t consumed_{ 0 };
};
