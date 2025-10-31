#pragma once

#include <map>

class SHRMemoryAllocationManager
{
public:
	struct FreeBlockInfo;

	using SHRFreeBlocksByOffsetMap = std::map<size_t, FreeBlockInfo>;
	using SHRFreeBlockBySizeMap = std::multimap<size_t, SHRFreeBlocksByOffsetMap::iterator>;

	struct FreeBlockInfo
	{
		size_t size;
		SHRFreeBlockBySizeMap::iterator bySizeIt;

		FreeBlockInfo(size_t s) : size(s) {};
	};

public:
	SHRMemoryAllocationManager(size_t totalSize);
	~SHRMemoryAllocationManager() = default;

	size_t Allocate(size_t size);
	void Free(size_t offset, size_t size);

	bool CanAllocate(size_t requiredSize);

private:
	void AddNewFreeBlock(size_t offset, size_t size);

public:
	size_t m_freeSize;
	SHRFreeBlocksByOffsetMap m_freeBlocksByOffset;
	SHRFreeBlockBySizeMap m_freeBlocksBySize;
};

