#pragma once
#include <string>
#include <vector>
#include <chrono>

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
    bool pending = false;
    std::chrono::steady_clock::time_point t0{};

    void log(std::string s) {
        logs.emplace_back(std::move(s));
        if (logs.size() > 500) logs.erase(logs.begin(), logs.begin() + (logs.size() - 500));
    }
};

// ==== 极简“假后端” ====
// 思路：点击 Connect 后进入 Connecting -> 2s 后 Verifying -> 1.5s 后 Connected。
// 点击 Disconnect 进入 Disconnecting -> 0.5s 后 Disconnected。
// 这就让你完整看到 UI <-> 逻辑 的闭环，后面再把时间判定替换成真实事件。

inline void StartConnect(VpnModel& m) {
    if (m.state == VpnState::Disconnected || m.state == VpnState::Failed) {
        m.state = VpnState::Connecting;
        m.pending = true;
        m.t0 = std::chrono::steady_clock::now();
        m.log("Connecting...");
    }
}

inline void StartDisconnect(VpnModel& m) {
    if (m.state == VpnState::Connected || m.state == VpnState::Verifying || m.state == VpnState::Connecting) {
        m.state = VpnState::Disconnecting;
        m.pending = true;
        m.t0 = std::chrono::steady_clock::now();
        m.log("Disconnecting...");
    }
}

inline void Tick(VpnModel& m) {
    using namespace std::chrono;
    if (!m.pending) return;
    const auto now = steady_clock::now();
    const auto dt = duration_cast<milliseconds>(now - m.t0).count();

    switch (m.state) {
    case VpnState::Connecting:
        if (dt > 2000) {
            m.state = VpnState::Verifying;
            m.t0 = now;
            m.log("Handshake OK -> Verifying...");
        }
        break;
    case VpnState::Verifying:
        if (dt > 1500) {
            m.state = VpnState::Connected;
            m.pending = false;
            m.log("Connectivity OK -> Connected.");
        }
        break;
    case VpnState::Disconnecting:
        if (dt > 500) {
            m.state = VpnState::Disconnected;
            m.pending = false;
            m.log("Disconnected.");
        }
        break;
    default: break;
    }
}
