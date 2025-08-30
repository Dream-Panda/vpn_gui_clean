#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <algorithm>
#include "process_runner.h"

// ---- 状态与模型（维持原 API 不变）----
enum class VpnState { Disconnected, Connecting, Verifying, Connected, Disconnecting, Failed };

inline const char* ToCString(VpnState s) {
    switch (s) {
    case VpnState::Disconnected:  return "Disconnected";
    case VpnState::Connecting:    return "Connecting";
    case VpnState::Verifying:     return "Verifying";
    case VpnState::Connected:     return "Connected";
    case VpnState::Disconnecting: return "Disconnecting";
    case VpnState::Failed:        return "Failed";
    default:                      return "?";
    }
}

struct VpnModel {
    VpnState state = VpnState::Disconnected;
    std::vector<std::string> logs;
    bool pending = false; // 兼容旧字段

    void log(std::string s) {
        logs.emplace_back(std::move(s));
        if (logs.size() > 1000) logs.erase(logs.begin(), logs.begin() + (logs.size() - 1000));
    }
};

// 内部运行时（单例）
struct VpnRuntime {
    ProcessRunner proc;
    bool saw_connected = false; // 是否看到了“连接成功”的关键输出
};

inline VpnRuntime& R() { static VpnRuntime r; return r; }

// ---- MVP-1: 真实子进程逻辑 ----
// 先用 ping 做演示：点 Connect -> 启动 ping -> 实时日志输出 -> 识别关键字判定 Connected
// 后面把 exe/args 换成 openvpn 即可（例如 openvpn.exe + --config path）



inline void StartConnect(VpnModel& m) {
    if (m.state == VpnState::Disconnected || m.state == VpnState::Failed) {
        // 先用 ping 演示（直起 ping.exe，不走 cmd /C）
        /*const std::wstring exe = L"C:\\Program Files\\OpenVPN\\bin\\openvpn.exe";
        const std::wstring args = L"--config C:\\path\\to\\your.ovpn --verb 4";*/
        // vpn_logic.h -> StartConnect(...)
        const std::wstring exe = L"C:\\Windows\\System32\\ping.exe";
        // 关键：命令行以程序名开头（最好加引号）
        const std::wstring args = L"\"C:\\Windows\\System32\\ping.exe\" -n 5 1.1.1.1";
        m.log("Spawning backend: ping.exe -n 5 1.1.1.1");


        if (R().proc.start(exe, args)) {
            m.state = VpnState::Connecting;
            m.pending = true;
            R().saw_connected = false;
            // ✅ 不要再打印 “cmd.exe /C …” 那句旧日志了
        }
        else {
            DWORD e = GetLastError();
            char buf[256] = {};
            FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr, e, 0, buf, sizeof(buf), nullptr);
            m.state = VpnState::Failed;
            m.pending = false;
            m.log(std::string("Failed to spawn process. WinErr=") + std::to_string(e) + " " + buf);
        }
    }
}


inline void StartDisconnect(VpnModel& m) {
    if (m.state == VpnState::Connected || m.state == VpnState::Verifying || m.state == VpnState::Connecting) {
        m.state = VpnState::Disconnecting;
        m.pending = true;
        m.log("Stopping backend...");
        R().proc.stop();
    }
}

// 识别“成功/失败”的关键词（先覆盖常见中英输出）
inline bool lineLooksConnected_(const std::string& s) {
    std::string t = s;
    std::transform(t.begin(), t.end(), t.begin(), [](unsigned char c) {
        return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : (char)c;
        });
    return t.find("reply from") != std::string::npos
        || t.find("time=") != std::string::npos
        || t.find("ttl=") != std::string::npos
        || t.find(u8"来自") != std::string::npos
        || t.find(u8"时间=") != std::string::npos
        || t.find(u8"字节=") != std::string::npos;


}


inline bool lineLooksError_(const std::string& s) {
    std::string t = s;
    std::transform(t.begin(), t.end(), t.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return t.find("general failure") != std::string::npos
        || t.find("request timed out") != std::string::npos
        || t.find(u8"无法访问目标主机") != std::string::npos;

}

inline void Tick(VpnModel& m) {
    // 1) 拉取后台输出
    for (auto& line : R().proc.drain_lines()) {
        m.log(line);

        // 看到“像是连通”的输出，进入 Verifying/Connected
        if (!R().saw_connected && lineLooksConnected_(line)) {
            R().saw_connected = true;
            // 先给个过渡态（可保留/可直接置 Connected）
            if (m.state == VpnState::Connecting) {
                m.state = VpnState::Verifying;
                m.log("Verifying connectivity...");
            }
        }
        if (lineLooksError_(line) && m.state == VpnState::Connecting) {
            m.state = VpnState::Failed;
            m.pending = false;
        }
    }

    // 2) 若进程仍在跑，并且已经看到连通线索，则置 Connected
    if (m.state == VpnState::Verifying && R().saw_connected) {
        m.state = VpnState::Connected;
        m.pending = false;
        m.log("Connectivity OK -> Connected.");
    }

    // 3) 进程退出后的收尾逻辑
    bool alive = R().proc.running();
    if (!alive) {
        if (m.state == VpnState::Disconnecting) {
            m.state = VpnState::Disconnected;
            m.pending = false;
            m.log("Disconnected.");
        }
        else if (m.state == VpnState::Connecting || m.state == VpnState::Verifying) {
            // 还没连上就结束了
            m.state = VpnState::Failed;
            m.pending = false;
            m.log("Backend exited before connected.");
        }
    }
}
