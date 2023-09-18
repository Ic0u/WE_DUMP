// This file is part of the uniquadev/RobloxDumper and is licensed under MIT License; see LICENSE.txt for details

#include "Process.h"
#include <filesystem>

/*
* simple ExRemoteThread injection method
*/
DWORD InjectLL(const HANDLE Proc, const char* Path)
{
	const auto Loc = VirtualAllocEx(Proc, 0, MAX_PATH, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (Loc == nullptr)
		return 4003;

	if (!WriteProcessMemory(Proc, (LPVOID)Loc, Path, MAX_PATH, 0))
		return 4004;

	const auto Thread = CreateRemoteThread(Proc, 0, 0, (LPTHREAD_START_ROUTINE)LoadLibraryA, Loc, 0, 0);
	if (Thread == nullptr)
		return 4005;

	WaitForSingleObject(Thread, INFINITE);
	DWORD ExitCode = 0;
	GetExitCodeThread(Thread, &ExitCode);
	CloseHandle(Thread);
	VirtualFreeEx(Proc, Loc, 0, MEM_RELEASE);

	return ExitCode;
}

std::filesystem::path GetBinPath()
{
	wchar_t Buffer[MAX_PATH];
	GetModuleFileNameW(NULL, Buffer, MAX_PATH);
	return std::filesystem::path(Buffer).parent_path();
}


int main()
{
	auto dll = GetBinPath() / "Dumper.dll";

	if (!std::filesystem::exists(dll))
	{
		std::cout << "Make sure to compile dll" << std::endl;
		return 404;
	}

	Process* ac = nullptr;
	while (!ac)
		ac = Process::ByName(L"RobloxPlayerBeta.exe");

	
	InjectLL(ac->ph, dll.string().c_str());
}