#pragma once
// Minimal Windows process runner (header-only)
// CreateProcessW + anonymous pipe to capture stdout/stderr in real-time.
// C++17, Visual Studio 2019+/2022

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <thread>
#include <atomic>

#pragma execution_character_set("utf-8")

class ProcessRunner {
public:
    ProcessRunner() = default;
    ~ProcessRunner() { stop(); }

    // 启动子进程（exe 可为绝对路径或 PATH 可找到的程序；args 只包含参数）
    bool start(const std::wstring& exe,
        const std::wstring& args,
        const std::wstring& workdir = L"");

    // 进程是否仍在运行
    bool running() const;

    // 终止并清理
    void stop();

    // 取出目前累计的所有输出行（UTF-8），并清空队列
    std::vector<std::string> drain_lines();

private:
    static std::string ansiToUtf8_(const char* bytes, int len);
    void readLoop_();
    void closePipeHandles_();

private:
    HANDLE proc_ = nullptr;
    HANDLE hRead_ = nullptr;   // 父进程读
    HANDLE hWrite_ = nullptr;   // 子进程写（父进程会尽快关闭）

    std::thread reader_;
    std::atomic<bool> running_{ false };

    std::mutex mu_;
    std::deque<std::string> lines_;
};

// ----------- impl -----------

inline bool ProcessRunner::start(const std::wstring& exe,
    const std::wstring& args,
    const std::wstring& workdir) {
    // 若已有进程在跑，先停掉
    stop();

    // 1) 建立 stdout/stderr 管道：子进程写 -> 父进程读
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    if (!CreatePipe(&hRead_, &hWrite_, &sa, 0)) {
        return false;
    }
    // 父进程读端不允许被继承，避免被子进程占用导致管道不结束
    SetHandleInformation(hRead_, HANDLE_FLAG_INHERIT, 0);

    // 2) 配置启动信息（把三把句柄传给子进程）
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite_;
    si.hStdError = hWrite_;

    // GUI 进程默认没有有效的标准输入；给子进程准备一个可继承的 NUL 作为 stdin
    SECURITY_ATTRIBUTES saIn{};
    saIn.nLength = sizeof(saIn);
    saIn.bInheritHandle = TRUE;
    saIn.lpSecurityDescriptor = nullptr;

    HANDLE hNulIn = CreateFileW(L"NUL",
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        &saIn,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    si.hStdInput = (hNulIn == INVALID_HANDLE_VALUE) ? nullptr : hNulIn;

    // 3) 准备可写命令行缓冲区（CreateProcessW 的第二参必须是可写 LPWSTR）
    std::vector<wchar_t> cmdline(args.begin(), args.end());
    cmdline.push_back(L'\0');

    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(
        exe.c_str(),                                   // 可执行文件
        cmdline.empty() ? nullptr : &cmdline[0],       // 仅参数（可写缓冲区）
        nullptr, nullptr,
        TRUE,                                          // 允许句柄继承（与 STARTF_USESTDHANDLES 配合）
        CREATE_NO_WINDOW,                              // 后台运行
        nullptr,
        workdir.empty() ? nullptr : workdir.c_str(),
        &si, &pi
    );

    // 父进程不再需要 NUL 句柄（子进程已继承）
    if (hNulIn != nullptr && hNulIn != INVALID_HANDLE_VALUE) {
        CloseHandle(hNulIn);
        hNulIn = nullptr;
    }

    if (!ok) {
        closePipeHandles_();
        return false;
    }

    // 父进程不再需要线程句柄 & 写端句柄
    if (pi.hThread) CloseHandle(pi.hThread);
    if (hWrite_) { CloseHandle(hWrite_); hWrite_ = nullptr; }

    // 4) 记录进程句柄，启动读线程
    proc_ = pi.hProcess;
    running_.store(true);
    reader_ = std::thread([this] { readLoop_(); });
    return true;
}

inline bool ProcessRunner::running() const {
    if (!proc_) return false;
    DWORD code = 0;
    if (!GetExitCodeProcess(proc_, &code)) return false;
    return code == STILL_ACTIVE;
}

inline void ProcessRunner::stop() {
    running_.store(false);

    if (proc_) {
        // 等一会儿，若仍未退出则强制结束
        if (WaitForSingleObject(proc_, 150) == WAIT_TIMEOUT) {
            TerminateProcess(proc_, 1);
        }
        WaitForSingleObject(proc_, 500);
        CloseHandle(proc_);
        proc_ = nullptr;
    }

    if (hRead_) { CloseHandle(hRead_);  hRead_ = nullptr; }
    if (hWrite_) { CloseHandle(hWrite_); hWrite_ = nullptr; }

    if (reader_.joinable()) reader_.join();
}

inline std::vector<std::string> ProcessRunner::drain_lines() {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<std::string> out(lines_.begin(), lines_.end());
    lines_.clear();
    return out;
}

// 将系统 ANSI/ACP 文本转 UTF-8（供 ImGui 显示）
inline std::string ProcessRunner::ansiToUtf8_(const char* bytes, int len) {
    if (len <= 0) return {};
    // 控制台输出通常走 OEM 代码页（而非 ANSI）
    int wlen = MultiByteToWideChar(CP_OEMCP, 0, bytes, len, nullptr, 0);
    if (wlen <= 0) return {};
    std::wstring wbuf(wlen, L'\0');
    MultiByteToWideChar(CP_OEMCP, 0, bytes, len, wbuf.empty() ? nullptr : &wbuf[0], wlen);
    int u8len = WideCharToMultiByte(CP_UTF8, 0, wbuf.data(), wlen, nullptr, 0, nullptr, nullptr);
    std::string out(u8len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wbuf.data(), wlen, out.empty() ? nullptr : &out[0], u8len, nullptr, nullptr);
    return out;
}



inline void ProcessRunner::readLoop_() {
    std::string acc; acc.reserve(4096);
    const DWORD BUFSZ = 4096;
    std::vector<char> buf(BUFSZ);

    while (true) {
        if (!hRead_) break;
        DWORD n = 0;
        BOOL ok = ReadFile(hRead_, buf.data(), BUFSZ, &n, nullptr);
        if (!ok || n == 0) break;

        // 转成 UTF-8 追加到累积区
        std::string u8 = ansiToUtf8_(buf.data(), (int)n);
        acc.append(u8);

        // 按行分割（\r\n 或 \n）
        size_t pos = 0;
        for (;;) {
            size_t nl = acc.find('\n', pos);
            if (nl == std::string::npos) {
                // 留下未满一行的尾巴
                acc.erase(0, pos);
                break;
            }
            std::string line = acc.substr(pos, nl - pos);
            if (!line.empty() && line.back() == '\r') line.pop_back();

            {
                std::lock_guard<std::mutex> lk(mu_);
                lines_.push_back(std::move(line));
            }
            pos = nl + 1;
        }
    }

    // 收尾：把最后一段（没有换行的）也算一行
    if (!acc.empty()) {
        std::lock_guard<std::mutex> lk(mu_);
        lines_.push_back(std::move(acc));
    }
}

inline void ProcessRunner::closePipeHandles_() {
    if (hRead_) { CloseHandle(hRead_);  hRead_ = nullptr; }
    if (hWrite_) { CloseHandle(hWrite_); hWrite_ = nullptr; }
}
