#include "SHRMemoryAllocationManager.h"

SHRMemoryAllocationManager::SHRMemoryAllocationManager(size_t totalSize)
{
	AddNewFreeBlock(0, totalSize);

	m_freeSize = totalSize;
}

size_t SHRMemoryAllocationManager::Allocate(size_t size)
{
	if (!CanAllocate(size)) return -1;

	SHRFreeBlockBySizeMap::iterator itBySize = m_freeBlocksBySize.lower_bound(size);
	SHRFreeBlocksByOffsetMap::iterator itByOffset = itBySize->second;

	size_t newOffset = itByOffset->first;
	size_t newSize = itByOffset->second.size - size;

	m_freeBlocksByOffset.erase(itByOffset);
	m_freeBlocksBySize.erase(itBySize);
	if (newSize > 0)
	{
		AddNewFreeBlock(newOffset, newSize);
	}
	m_freeSize -= size;
	return newOffset;
}

void SHRMemoryAllocationManager::Free(size_t offset, size_t size)
{
	auto itNextByOffset = m_freeBlocksByOffset.upper_bound(offset);
	auto itPrevByOffset = itNextByOffset == m_freeBlocksByOffset.begin() ? m_freeBlocksByOffset.end() : itNextByOffset--;

	size_t newOffset, newSize;
	if (itPrevByOffset != m_freeBlocksByOffset.end() && itPrevByOffset->first + itPrevByOffset->second.size == offset)
	{
		newOffset = itPrevByOffset->first;
		newSize = itPrevByOffset->second.size + size;
		m_freeBlocksBySize.erase(itPrevByOffset->second.bySizeIt);
		m_freeBlocksByOffset.erase(itPrevByOffset);
		if (itNextByOffset != m_freeBlocksByOffset.end() && offset + size == itNextByOffset->first)
		{
			newSize += itNextByOffset->second.size;
			m_freeBlocksBySize.erase(itNextByOffset->second.bySizeIt);
			m_freeBlocksByOffset.erase(itNextByOffset);
		}
	}
	else if (itNextByOffset != m_freeBlocksByOffset.end() && offset + size == itNextByOffset->first)
	{
		newOffset = offset;
		newSize = size + itNextByOffset->second.size;
		m_freeBlocksBySize.erase(itNextByOffset->second.bySizeIt);
		m_freeBlocksByOffset.erase(itNextByOffset);
	}
	else
	{
		newOffset = offset;
		newSize = size;
	}

	AddNewFreeBlock(newOffset, newSize);
	m_freeSize += size;
}

bool SHRMemoryAllocationManager::CanAllocate(size_t requiredSize)
{
	return m_freeSize >= requiredSize;
}

void SHRMemoryAllocationManager::AddNewFreeBlock(size_t offset, size_t size)
{
	auto itByOffset = m_freeBlocksByOffset.emplace(offset, size);
	auto itBySize = m_freeBlocksBySize.emplace(size, itByOffset.first);
	itByOffset.first->second.bySizeIt = itBySize;
}