#include "OpenVpnRunner.h"
#include <fstream>

static std::string q(const std::string& s) { return "\"" + s + "\""; }

bool OpenVpnRunner::start(const OpenVpnConfig& cfg, LogCallback cb) {
    if (running_) return false;

    ProcessOptions opt;
    opt.exePath = cfg.openvpnExe;
    opt.workDir = cfg.workDir;

    std::vector<std::string> args = { "--config", q(cfg.ovpnFile), "--verb", std::to_string(cfg.verb) };

    std::string authFile;
    if (!cfg.authUser.empty() || !cfg.authPass.empty()) {
        authFile = cfg.workDir + "/auth.txt";
        std::ofstream f(authFile, std::ios::binary);
        f << cfg.authUser << "\n" << cfg.authPass << "\n";
        f.close();
        args.push_back("--auth-user-pass");
        args.push_back(q(authFile));
    }
    opt.args = std::move(args);

    bool ok = pr_.start(opt,
        [cb](const std::string& line) { if (cb) cb(line); },
        [cb](const std::string& line) { if (cb) cb(std::string("[ERR] ") + line); }
    );
    running_ = ok;
    return ok;
}

void OpenVpnRunner::stop() {
    if (!running_) return;
    pr_.stop();
    running_ = false;
}
