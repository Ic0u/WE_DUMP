// This file is part of the uniquadev/RobloxDumper and is licensed under MIT License; see LICENSE.txt for details

#include "MemoryUtils.h"
#include "Memory.h"
#include "Signature.h"
#include "Zydis.h"

using namespace Dumper::Memory;

uintptr_t Dumper::Memory::get_offset(uintptr_t addr)
{
	return addr - r_module;
}

std::vector<uintptr_t> Dumper::Memory::get_xrefs(uintptr_t addr)
{
    return get_xrefs(addr, SearchSettings(r_module, r_module_end, PAGE_EXECUTE_READ, true, false));
}

std::vector<uintptr_t> Dumper::Memory::get_xrefs(uintptr_t addr, SearchSettings settings)
{
	auto res = std::vector<uintptr_t>();
	auto regions = get_regions(settings.start, settings.protect, settings.end);
	auto step = settings.fast_scan ? sizeof(uintptr_t) : 1;

	for (auto& region : regions)
	{
		DWORD OldProtect = 0;
		VirtualProtect(region.BaseAddress, region.RegionSize, PAGE_EXECUTE_READWRITE, &OldProtect);
		const uintptr_t base = reinterpret_cast<uintptr_t>(region.BaseAddress);
		
		for (size_t i = 0; i < region.RegionSize; i += step)
		{
			if (*(uintptr_t*)(base + i) == addr)
			{
				res.push_back(base + i);
				if (settings.stop_first)
				{
					VirtualProtect(region.BaseAddress, region.RegionSize, OldProtect, &OldProtect);
					return res;
				}
			}
		}
		VirtualProtect(region.BaseAddress, region.RegionSize, OldProtect, &OldProtect);
	}
	return res;
}

std::vector<uintptr_t> Dumper::Memory::find_string(const char* str, SearchSettings settings)
{
	auto res = std::vector<uintptr_t>();
	auto regions = get_regions(settings.start, settings.protect, settings.end);
	auto size = strlen(str);
	auto step = settings.fast_scan ? sizeof(char*) : 1;
	
	for (auto& region : regions)
	{
		DWORD OldProtect = 0;
		VirtualProtect(region.BaseAddress, region.RegionSize, PAGE_EXECUTE_READWRITE, &OldProtect);
		const uintptr_t base = reinterpret_cast<uintptr_t>(region.BaseAddress);
		
		for (size_t i = 0; i < region.RegionSize - sizeof(char*); i += step)
		{
			auto mstr = (const char*)(base + i);
			if (strncmp(mstr, str, size) == 0)
			{
				res.push_back(base + i);
				if (settings.stop_first)
				{
					VirtualProtect(region.BaseAddress, region.RegionSize, OldProtect, &OldProtect);
					return res;
				}
			}
		}
		VirtualProtect(region.BaseAddress, region.RegionSize, OldProtect, &OldProtect);
	}
	return res;
}

std::vector<uintptr_t> Dumper::Memory::find_string(const char* str)
{
	return find_string(str, SearchSettings(r_module, r_module_end, PAGE_READONLY, true, true));
}

uintptr_t Dumper::Memory::next_call(uintptr_t addr, size_t skips)
{
	// get current region
	auto region = get_region(addr);
	if (!region.has_value())
		return 0;
	uintptr_t end = reinterpret_cast<uintptr_t>(region.value().BaseAddress) + region.value().RegionSize;
	// loop until we find a 0xE8 byte call
	for (size_t i = addr; i < end; i++)
	{
		if (*(BYTE*)i == 0xE8)
		{
			if (skips == 0)
				return i;
			skips--;
		}
	}
	return 0;
}

uintptr_t Dumper::Memory::get_calling(uintptr_t call_add)
{
	// check if passed add is a call op
	if (*(BYTE*)call_add != 0xE8)
		return 0;
	// get the relative address
	intptr_t rel_addr = *(intptr_t*)(call_add + 1);
	// return the calling address
	return call_add + rel_addr + 5;
}

uintptr_t Dumper::Memory::get_func_top(uintptr_t addr)
{
	const BYTE prol[3] = { 0x55, 0x8B, 0xEC }; // prologue
	
	// loop back from addr to region base & memcmp with prol header
	for (size_t i = addr; i >= r_module; i--)
	{
		if (memcmp((void*)i, prol, sizeof(prol)) == 0)
			return i;
	}
	return 0;
}

uintptr_t Dumper::Memory::get_func_end(uintptr_t addr)
{
	const BYTE epil[] = { 0x5B, 0x8B, 0xE5, 0x5D, 0x8B, 0xE3 }; // epilogue
	
	// get current region
	auto region = get_region(addr);
	if (!region.has_value())
		return 0;
	
	uintptr_t end = reinterpret_cast<uintptr_t>(region.value().BaseAddress) + region.value().RegionSize - sizeof(epil);
	// loop from addr to region top & memcmp with epil header
	for (size_t i = addr; i < end; i++)
	{
		if (memcmp((void*)i, epil, sizeof(epil)) == 0)
			return i;
	}
	return 0;
}

std::vector<uintptr_t> Dumper::Memory::scan(std::string sign, SearchSettings settings)
{
	return scan(Signature(sign), settings);
}

std::vector<uintptr_t> Dumper::Memory::scan(Signature sign, SearchSettings settings)
{
	auto res = std::vector<uintptr_t>();
	auto regions = get_regions(settings.start, settings.protect, settings.end);

	for (auto& region : regions)
	{
		DWORD OldProtect = 0;
		VirtualProtect(region.BaseAddress, region.RegionSize, PAGE_EXECUTE_READWRITE, &OldProtect);
		const uintptr_t base = reinterpret_cast<uintptr_t>(region.BaseAddress);

		for (size_t i = 0; i < region.RegionSize; i++)
		{
			if (sign.compare((void*)(base + i)) )
			{
				res.push_back(base + i);
				if (settings.stop_first)
				{
					VirtualProtect(region.BaseAddress, region.RegionSize, OldProtect, &OldProtect);
					return res;
				}
			}
		}
		VirtualProtect(region.BaseAddress, region.RegionSize, OldProtect, &OldProtect);
	}
	return res;
}

std::vector<int64_t> Dumper::Memory::get_immediates(SearchSettings settings)
{
	std::vector<int64_t> res = {};

	ZyanU8* data = (ZyanU8*)settings.start;
	ZyanUSize lenght = settings.end - settings.start;
	ZyanUSize offset = 0;

	ZydisDisassembledInstruction instruction;
	while (ZYAN_SUCCESS(ZydisDisassembleIntel(
		ZYDIS_MACHINE_MODE_LONG_COMPAT_32,
		settings.start,
		data + offset,
		lenght - offset,
		&instruction
	)))
	{
		// Check if the instruction has an immediate operand
		for (uint8_t i = 0; i < 10; ++i)
		{
			const ZydisDecodedOperand* operand = &instruction.operands[i];
			if (operand->type == ZYDIS_OPERAND_TYPE_IMMEDIATE)
			{
				res.push_back((uintptr_t)(operand->imm.value.s));
				if (settings.stop_first)
					return res;
			}
		}
		offset += instruction.info.length;
	}

	return res;
}

std::vector<int64_t> Dumper::Memory::get_immeoffsets(SearchSettings settings)
{
	std::vector<int64_t> res = {};

	ZyanU8* data = (ZyanU8*)settings.start;
	ZyanUSize lenght = settings.end - settings.start;
	ZyanUSize offset = 0;

	ZydisDisassembledInstruction instruction;
	while (ZYAN_SUCCESS(ZydisDisassembleIntel(
		ZYDIS_MACHINE_MODE_LONG_COMPAT_32,
		settings.start,
		data + offset,
		lenght - offset,
		&instruction
	)))
	{
		// Check if the instruction has an immediate operand
		for (uint8_t i = 0; i < 10; ++i)
		{
			const ZydisDecodedOperand* operand = &instruction.operands[i];
			if (operand->type == ZYDIS_OPERAND_TYPE_MEMORY)
			{
				if (!operand->mem.disp.has_displacement)
					continue;
				res.push_back(operand->mem.disp.value);
				if (settings.stop_first)
					return res;
			}
		}
		offset += instruction.info.length;
	}

	return res;
}
