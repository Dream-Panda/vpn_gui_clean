#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "ProcessRunner.h"
#include <vector>
#include <cassert>

namespace {
    inline void CloseIf(HANDLE& h) { if (h) { ::CloseHandle(h); h = nullptr; } }
}

ProcessRunner::ProcessRunner() {}
ProcessRunner::~ProcessRunner() { Stop(true); }

bool ProcessRunner::MakePipe_(HANDLE& hRead, HANDLE& hWrite, bool writeInheritable) {
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = nullptr;
    sa.bInheritHandle = TRUE;
    if (!::CreatePipe(&hRead, &hWrite, &sa, 0)) return false;
    ::SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);
    if (!writeInheritable) ::SetHandleInformation(hWrite, HANDLE_FLAG_INHERIT, 0);
    return true;
}

std::wstring ProcessRunner::QuoteIfNeeded_(const std::wstring& s) {
    if (s.empty()) return L"";
    if (s.find(L' ') != std::wstring::npos || s.find(L'\"') != std::wstring::npos) {
        std::wstring out = L"\"";
        for (wchar_t ch : s) { if (ch == L'\"') out += L"\\\""; else out.push_back(ch); }
        out += L"\"";
        return out;
    }
    return s;
}

std::string ProcessRunner::BytesToUtf8Fallback_(const char* data, size_t n) {
    if (n == 0) return {};
    int wlen = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, data, (int)n, nullptr, 0);
    if (wlen > 0) {
        std::wstring w(wlen, 0);
        ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, data, (int)n, &w[0], wlen);
        int u8 = ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(), wlen, nullptr, 0, nullptr, nullptr);
        std::string s(u8, 0);
        ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(), wlen, &s[0], u8, nullptr, nullptr);
        return s;
    }
    int wlen2 = ::MultiByteToWideChar(CP_OEMCP, 0, data, (int)n, nullptr, 0);
    std::wstring w2(wlen2, 0);
    ::MultiByteToWideChar(CP_OEMCP, 0, data, (int)n, &w2[0], wlen2);
    int u82 = ::WideCharToMultiByte(CP_UTF8, 0, w2.c_str(), wlen2, nullptr, 0, nullptr, nullptr);
    std::string s2(u82, 0);
    ::WideCharToMultiByte(CP_UTF8, 0, w2.c_str(), wlen2, &s2[0], u82, nullptr, nullptr);
    return s2;
}

bool ProcessRunner::Start(const std::wstring& exePath,
    const std::wstring& args,
    const std::wstring& workingDir) {
    if (running_) return false;
    Stop(true);

    if (!MakePipe_(hStdOutR_, hStdOutW_, true)) return false;
    if (!MakePipe_(hStdErrR_, hStdErrW_, true)) { Stop(true); return false; }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdOutput = hStdOutW_;
    si.hStdError = hStdErrW_;
    si.hStdInput = NULL;

    std::wstring cmd = QuoteIfNeeded_(exePath);
    if (!args.empty()) { cmd.push_back(L' '); cmd += args; }
    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(L'\0'); // mutable buffer required

    PROCESS_INFORMATION pi{};
    DWORD flags = CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT;

    BOOL ok = ::CreateProcessW(
        nullptr,
        cmdBuf.empty() ? nullptr : &cmdBuf[0], // must be LPWSTR (writable)
        nullptr, nullptr,
        TRUE,
        flags,
        nullptr,
        workingDir.empty() ? nullptr : workingDir.c_str(),
        &si, &pi
    );

    CloseIf(hStdOutW_);
    CloseIf(hStdErrW_);

    if (!ok) {
        Stop(true);
        return false;
    }

    hProcess_ = pi.hProcess;
    hThread_ = pi.hThread;
    running_ = true;
    stopRequested_ = false;

    tOut_ = std::thread(&ProcessRunner::ReaderThreadProc_, this, hStdOutR_);
    tErr_ = std::thread(&ProcessRunner::ReaderThreadProc_, this, hStdErrR_);
    tWait_ = std::thread(&ProcessRunner::WaitThreadProc_, this, hProcess_);

    return true;
}

void ProcessRunner::Stop(bool force, unsigned /*killTimeoutMs*/) {
    if (!running_) {
        CloseIf(hStdOutR_); CloseIf(hStdErrR_);
        CloseIf(hStdOutW_); CloseIf(hStdErrW_);
        return;
    }
    stopRequested_ = true;

    if (force && hProcess_) ::TerminateProcess(hProcess_, 1);

    if (tOut_.joinable()) tOut_.join();
    if (tErr_.joinable()) tErr_.join();
    if (tWait_.joinable()) tWait_.join();

    CloseIf(hThread_);
    CloseIf(hProcess_);
    CloseIf(hStdOutR_);
    CloseIf(hStdErrR_);
    CloseIf(hStdOutW_);
    CloseIf(hStdErrW_);

    running_ = false;
}

void ProcessRunner::ReaderThreadProc_(HANDLE readHandle) {
    std::vector<char> buf(4096);
    DWORD n = 0;
    while (true) {
        if (!::ReadFile(readHandle, buf.data(), (DWORD)buf.size(), &n, nullptr) || n == 0) break;
        std::string chunk = BytesToUtf8Fallback_(buf.data(), n);
        std::lock_guard<std::mutex> lk(logMtx_);
        logBuf_.append(chunk);
    }
}

void ProcessRunner::WaitThreadProc_(HANDLE hProcess) {
    ::WaitForSingleObject(hProcess, INFINITE);
    running_ = false;
}

std::string ProcessRunner::ConsumeNewOutput() {
    std::lock_guard<std::mutex> lk(logMtx_);
    if (consumed_ >= logBuf_.size()) return {};
    std::string out = logBuf_.substr(consumed_);
    consumed_ = logBuf_.size();
    return out;
}

void ProcessRunner::ClearLog() {
    std::lock_guard<std::mutex> lk(logMtx_);
    logBuf_.clear();
    consumed_ = 0;
}
