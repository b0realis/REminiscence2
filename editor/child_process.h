// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <spawn.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
extern char **environ;
#endif
namespace editor {
// Exact argv spawning, without a shell. Reap exited children on the next poll.
struct ChildProcess {
#ifdef _WIN32
	HANDLE handle = nullptr;
	~ChildProcess() {
		if (handle)
			CloseHandle(handle);
	}
	bool running() {
		DWORD code = 0;
		if (handle && GetExitCodeProcess(handle, &code) && code == STILL_ACTIVE)
			return true;
		if (handle)
			CloseHandle(handle);
		handle = nullptr;
		return false;
	}
#else
	pid_t pid = -1;
	bool running() {
		if (pid <= 0)
			return false;
		int code;
		const auto result = waitpid(pid, &code, WNOHANG);
		if (result == 0)
			return true;
		pid = -1;
		return false;
	}
#endif
	void launch(const std::vector<std::string> &args, const std::filesystem::path &log) {
		if (running())
			throw std::runtime_error("A native preview is already running");
#ifdef _WIN32
		std::wstring command;
		for (const auto &arg : args) {
			if (!command.empty())
				command += L' ';
			command += L'"';
			size_t slash = 0;
			for (wchar_t c : std::filesystem::u8path(arg).wstring()) {
				if (c == L'\\') {
					++slash;
					continue;
				}
				command.append(slash * (c == L'"' ? 2 : 1), L'\\');
				slash = 0;
				if (c == L'"')
					command += L'\\';
				command += c;
			}
			command.append(slash * 2, L'\\');
			command += L'"';
		}
		STARTUPINFOW startup{};
		startup.cb = sizeof(startup);
		PROCESS_INFORMATION process{};
		if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr,
		                    &startup, &process))
			throw std::runtime_error("Cannot launch native player");
		handle = process.hProcess;
		CloseHandle(process.hThread);
		(void)log;
#else
		std::vector<char *> argv;
		for (const auto &arg : args)
			argv.push_back(const_cast<char *>(arg.c_str()));
		argv.push_back(nullptr);
		posix_spawn_file_actions_t actions;
		posix_spawn_file_actions_init(&actions);
		posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, log.c_str(),
		                                 O_WRONLY | O_CREAT | O_TRUNC, 0600);
		posix_spawn_file_actions_adddup2(&actions, STDOUT_FILENO, STDERR_FILENO);
		const int error =
		    posix_spawn(&pid, args[0].c_str(), &actions, nullptr, argv.data(), environ);
		posix_spawn_file_actions_destroy(&actions);
		if (error) {
			pid = -1;
			throw std::runtime_error("Cannot launch native player (error " + std::to_string(error) +
			                         ")");
		}
#endif
	}
};
} // namespace editor
