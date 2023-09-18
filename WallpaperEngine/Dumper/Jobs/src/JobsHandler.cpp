// This file is part of the uniquadev/RobloxDumper and is licensed under MIT License; see LICENSE.txt for details

#include "JobsHandler.h"
#include "Jobs.h"
#include "Memory.h"

#include <synchapi.h>

void Dumper::JobsHandler::register_jobs()
{
	// INDIPENDENTS JOBS
	register_job("Misc", dump_misc);
	register_job("Taskscheduler", dump_taskscheduler);
	register_job("SCResume", dump_scresume);
	register_job("TaskDefer", dump_taskdefer);
	register_job("Printf", dump_printf);
	register_job("lbase", dump_lbase);

	register_job("lua_openbase", dump_lua_openbase, {
		// DEPENDS ON lua_openbase
		{"tssize", dump_tssize, {
			// DEPENDS ON tssize
			{"tsnext", dump_tsnext},
			{"tsnext", dump_tshash}
		}},
	});

	register_job("GetState", dump_getstate, {
		// DEPENDS ON GetState
		{"scriptstate", dump_scriptstate},
		{"lgt", dump_lgt}
	});

	register_job("LTypeName", dump_ltypename, {
		// DEPENDS ON LTypeName
		{"types", dump_types}
	});

	
	register_job("LuaVM::Load", dump_vm_load, {
		// DEPENDS ON SCRIPTMODULEINTERNAL & VMLOAD
		{"pseudo2addr", dump_pseudo2addr},
		{"vmload_hooks", dump_vmload_hooks}
	});

	register_job("os_date", dump_os_date, {
		// DEPENDS ON os_date
		{"lglobal", dump_lglobal},
		{"ltop", dump_ltop},
		{"dummynode", dump_dummynode}
	});

	// ADDONS
	// TODO subs calling convetions dumper
	// TODO subs retcheck dumper
	// TODO check if luau fast region
}

CRITICAL_SECTION init_section;

void Dumper::JobsHandler::run_job(Dumper::Job job)
{
	if (!job.f(this))	// if job return false then force stop dumping
		push_error("ERROR AT " + job.name);
	else
		for (const auto& child : job.children)
			run_job(child);
}

bool Dumper::JobsHandler::run()
{
	std::ignore = InitializeCriticalSectionAndSpinCount(&init_section, 0x00000200UL);
	std::clock_t started_at = std::clock();
	
	EnterCriticalSection(&init_section);
	for (const auto& job : jobs)
		run_job(job);
	LeaveCriticalSection(&init_section);

	std::stringstream stream2;
	stream2 << 1000.0 * (std::clock() - started_at) / CLOCKS_PER_SEC << " ms";
	output["cputime"] = stream2.str();

	output["credit"] = "Dumper created by Uniqua#6289 and plusgiant5#7930";

	return errors == 0 ? true : false;
}

void Dumper::JobsHandler::format()
{
	const auto path = Dumper::Memory::get_dll_dir((HMODULE)Dumper::Memory::r_module);
	output["version"] = path.filename().string();
}