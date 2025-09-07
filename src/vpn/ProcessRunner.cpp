#include "ProcessRunner.h"
#include <sstream>

static inline void appendQuoted(std::wstringstream& ss, const std::wstring& s) {
    ss << L'"';
    for (auto c : s) { if (c == L'"') ss << L'\\'; ss << c; }
    ss << L'"';
}
std::wstring ProcessRunner::buildCmdLine(const ProcessOptions& opt) {
    std::wstringstream ss; appendQuoted(ss, opt.exe);
    for (const auto& a : opt.args) { ss << L' '; appendQuoted(ss, a); }
    return ss.str();
}
void ProcessRunner::closeHandleSafe(HANDLE& h) { if (h && h != INVALID_HANDLE_VALUE) { CloseHandle(h); h = nullptr; } }

ProcessRunner::ProcessRunner() { ZeroMemory(&pi_, sizeof(pi_)); job_ = CreateJobObjectW(nullptr, nullptr); }
ProcessRunner::~ProcessRunner() { stop(); closeHandleSafe(job_); }

bool ProcessRunner::start(const ProcessOptions& opt, std::wstring* lastError) {
    stop();
    STARTUPINFOW si{}; si.cb = sizeof(si);
    if (opt.hidden) { si.dwFlags |= STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE; }
    std::wstring cmd = buildCmdLine(opt);
    std::wstring work = opt.workingDir;

    BOOL ok = CreateProcessW(opt.exe.c_str(), cmd.data(), nullptr, nullptr,
        opt.inheritHandles ? TRUE : FALSE,
        CREATE_UNICODE_ENVIRONMENT | (opt.hidden ? CREATE_NO_WINDOW : 0),
        nullptr, work.empty() ? nullptr : work.c_str(), &si, &pi_);
    if (!ok) {
        if (lastError) {
            DWORD e = GetLastError(); wchar_t* buf = nullptr;
            FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr, e, 0, (LPWSTR)&buf, 0, nullptr);
            *lastError = buf ? buf : L"CreateProcess failed"; if (buf) LocalFree(buf);
        }
        ZeroMemory(&pi_, sizeof(pi_)); return false;
    }
    if (job_) AssignProcessToJobObject(job_, pi_.hProcess);
    return true;
}
void ProcessRunner::stop(DWORD code) {
    if (!pi_.hProcess) return;
    TerminateProcess(pi_.hProcess, code);
    WaitForSingleObject(pi_.hProcess, 3000);
    closeHandleSafe(pi_.hThread);
    closeHandleSafe(pi_.hProcess);
    ZeroMemory(&pi_, sizeof(pi_));
}
bool ProcessRunner::running() const {
    if (!pi_.hProcess) return false; DWORD c = 0;
    if (!GetExitCodeProcess(pi_.hProcess, &c)) return false;
    return c == STILL_ACTIVE;
}
