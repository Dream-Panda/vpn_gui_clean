#pragma once
#include "vpn/ProcessRunner.h"
#include <string>
#include <functional>
#include <atomic>

struct OpenVpnConfig {
    std::string openvpnExe;   // e.g. C:/Program Files/OpenVPN/bin/openvpn.exe
    std::string ovpnFile;     // e.g. C:/vpn/profiles/my.ovpn
    std::string workDir;      // e.g. C:/vpn/temp
    std::string authUser;     // ��ѡ
    std::string authPass;     // ��ѡ
    int verb = 3;             // ��־����(3~6����)
};

class OpenVpnRunner {
public:
    using LogCallback = std::function<void(const std::string&)>;

    bool start(const OpenVpnConfig& cfg, LogCallback cb);
    void stop();
    bool running() const { return running_; }

private:
    ProcessRunner pr_;
    std::atomic<bool> running_{ false };
};
