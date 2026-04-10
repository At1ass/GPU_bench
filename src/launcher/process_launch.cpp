#include "launcher/process_launch.h"
#include "platform/data_path.h"
#include "platform/logger.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#include <signal.h>
#endif

#ifdef _WIN32

bool launchProcess(const std::string& exe_name, const std::vector<std::string>& args) {
    std::string exe_dir = getExeDir();
    std::string exe_path = exe_dir + exe_name + ".exe";

    // Build command line: "path\to\exe.exe" arg1 arg2 ...
    std::string cmd = "\"" + exe_path + "\"";
    for (size_t i = 0; i < args.size(); ++i) {
        cmd += " ";
        if (args[i].find(' ') != std::string::npos)
            cmd += "\"" + args[i] + "\"";
        else
            cmd += args[i];
    }

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));

    BOOL ok = CreateProcessA(
        NULL,                       // lpApplicationName
        &cmd[0],                    // lpCommandLine (mutable)
        NULL, NULL,                 // security attributes
        FALSE,                      // inherit handles
        0,                          // creation flags
        NULL,                       // environment (inherit)
        exe_dir.c_str(),            // working directory
        &si, &pi
    );

    if (ok) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        LOG_INF("Launched %s (pid %lu)", exe_name.c_str(), pi.dwProcessId);
        return true;
    }

    LOG_ERR("CreateProcess failed for '%s': error %lu", exe_path.c_str(), GetLastError());
    return false;
}

#else // POSIX

bool launchProcess(const std::string& exe_name, const std::vector<std::string>& args) {
    std::string exe_dir = getExeDir();
    std::string exe_path = exe_dir + exe_name;

    // Build argv array for execv
    std::vector<const char*> argv;
    argv.push_back(exe_path.c_str());
    for (size_t i = 0; i < args.size(); ++i)
        argv.push_back(args[i].c_str());
    argv.push_back(NULL);

    pid_t pid = fork();
    if (pid < 0) {
        LOG_ERR("fork() failed");
        return false;
    }
    if (pid == 0) {
        // Child process
        execv(exe_path.c_str(), const_cast<char* const*>(argv.data()));
        // execv only returns on failure
        _exit(127);
    }
    // Parent continues
    LOG_INF("Launched %s (pid %d)", exe_name.c_str(), static_cast<int>(pid));
    return true;
}

#endif
