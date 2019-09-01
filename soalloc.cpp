// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <cstdlib>
#include <cassert>
#include "soalloc.h"

void FixedAllocator::Chunk::Init(size_t blockSize, unsigned char blocks)
{
	assert(blockSize > 0);
	assert(blocks > 0);
	// Overflow check
	assert((blockSize * blocks) / blockSize == blocks);

	m_pData = new unsigned char[blockSize * blocks];

	Reset(blockSize, blocks);
}

void* FixedAllocator::Chunk::Allocate(size_t blockSize)
{
	if (!m_blocksAvailable) return 0;

	assert((m_firstAvailableBlock * blockSize) / blockSize ==
		m_firstAvailableBlock);

	unsigned char* pResult =
		m_pData + (m_firstAvailableBlock * blockSize);
	m_firstAvailableBlock = *pResult;
	--m_blocksAvailable;

	return pResult;
}

void FixedAllocator::Chunk::Deallocate(void* p, size_t blockSize)
{
	assert(p >= m_pData);

	unsigned char* toRelease = static_cast<unsigned char*>(p);
	// Alignment check
	assert((toRelease - m_pData) % blockSize == 0);

	*toRelease = m_firstAvailableBlock;
	m_firstAvailableBlock = static_cast<unsigned char>(
		(toRelease - m_pData) / blockSize);
	// Truncation check
	assert(m_firstAvailableBlock == (toRelease - m_pData) / blockSize);

	++m_blocksAvailable;
}

void FixedAllocator::Chunk::Reset(size_t blockSize, unsigned char blocks)
{
	assert(blockSize > 0);
	assert(blocks > 0);
	// Overflow check
	assert((blockSize * blocks) / blockSize == blocks);

	m_firstAvailableBlock = 0;
	m_blocksAvailable = blocks;

	unsigned char i = 0;
	unsigned char* p = m_pData;
	for (; i != blocks; p += blockSize)
	{
		*p = ++i;
	}
}

void FixedAllocator::Chunk::Release()
{
	delete[] m_pData;
}

// Internal functions        
void FixedAllocator::DoDeallocate(void* p)
{
	assert(m_deallocChunk->m_pData <= p);
	assert(m_deallocChunk->m_pData + m_numBlocks * m_blockSize > p);

	// call into the chunk, will adjust the inner list but won't release memory
	m_deallocChunk->Deallocate(p, m_blockSize);

	if (m_deallocChunk->m_blocksAvailable == m_numBlocks)
	{
		// deallocChunk_ is completely free, should we release it? 

		Chunk& lastChunk = m_chunks.back();

		if (&lastChunk == m_deallocChunk)
		{
			// check if we have two last chunks empty

			if (m_chunks.size() > 1 &&
				m_deallocChunk[-1].m_blocksAvailable == m_numBlocks)
			{
				// Two free chunks, discard the last one
				lastChunk.Release();
				m_chunks.pop_back();
				m_allocChunk = m_deallocChunk = &m_chunks.front();
			}
			return;
		}

		if (lastChunk.m_blocksAvailable == m_numBlocks)
		{
			// Two free blocks, discard one
			lastChunk.Release();
			m_chunks.pop_back();
			m_allocChunk = m_deallocChunk;
		}
		else
		{
			// move the empty chunk to the end
			std::swap(*m_deallocChunk, lastChunk);
			m_allocChunk = &m_chunks.back();
		}
	}
}

#pragma warning(push)
#pragma warning(disable: 4702)	// подавить warning C4702: unreachable code
FixedAllocator::Chunk* FixedAllocator::VicinityFind(void* p)
{
	assert(!m_chunks.empty());
	assert(m_deallocChunk);

	const std::size_t chunkLength = m_numBlocks * m_blockSize;

	Chunk* lo = m_deallocChunk;
	Chunk* hi = m_deallocChunk + 1;
	Chunk* loBound = &m_chunks.front();
	Chunk* hiBound = &m_chunks.back() + 1;

	// Special case: deallocChunk_ is the last in the array
	if (hi == hiBound) hi = 0;

	for (;;)
	{
		if (lo)
		{
			if (p >= lo->m_pData && p < lo->m_pData + chunkLength)
			{
				return lo;
			}
			if (lo == loBound) lo = 0;
			else --lo;
		}

		if (hi)
		{
			if (p >= hi->m_pData && p < hi->m_pData + chunkLength)
			{
				return hi;
			}
			if (++hi == hiBound) hi = 0;
		}
	}
	assert(false);
	return 0;
#pragma warning(pop)
}

// Create a FixedAllocator able to manage blocks of 'blockSize' size
FixedAllocator::FixedAllocator(std::size_t blockSize)
	: m_blockSize(blockSize)
	, m_allocChunk(nullptr)
	, m_deallocChunk(nullptr)
	, m_cache_delguard(0)
{
	assert(m_blockSize > 0);

	m_prev = m_next = this;

	std::size_t numBlocks = DEFAULT_CHUNK_SIZE / blockSize;
	if (numBlocks > UCHAR_MAX) numBlocks = UCHAR_MAX;
	else if (numBlocks == 0) numBlocks = 8 * blockSize;

	m_numBlocks = static_cast<unsigned char>(numBlocks);
	assert(m_numBlocks == numBlocks);
	m_cache.reserve(POOL_CACHE_CAPACITY);
}

FixedAllocator::FixedAllocator(const FixedAllocator& rhs)
	: m_blockSize(rhs.m_blockSize)
	, m_numBlocks(rhs.m_numBlocks)
	, m_chunks(rhs.m_chunks)
	, m_cache(rhs.m_cache)
	, m_cache_delguard(rhs.m_cache_delguard)
{
	m_prev = &rhs;
	m_next = rhs.m_next;
	rhs.m_next->m_prev = this;
	rhs.m_next = this;

	m_allocChunk = rhs.m_allocChunk
		? &m_chunks.front() + (rhs.m_allocChunk - &rhs.m_chunks.front())
		: 0;

	m_deallocChunk = rhs.m_deallocChunk
		? &m_chunks.front() + (rhs.m_deallocChunk - &rhs.m_chunks.front())
		: 0;

}

FixedAllocator& FixedAllocator::operator=(const FixedAllocator& rhs)
{
	FixedAllocator copy(rhs);
	copy.Swap(*this);
	return *this;
}

FixedAllocator::~FixedAllocator()
{
	if (m_prev != this)
	{
		m_prev->m_next = m_next;
		m_next->m_prev = m_prev;
		return;
	}

	assert(m_prev == m_next);
	Chunks::iterator i = m_chunks.begin();
	for (; i != m_chunks.end(); ++i)
	{
		assert(i->m_blocksAvailable == m_numBlocks);
		i->Release();
	}
}

void FixedAllocator::Swap(FixedAllocator& rhs)
{
	using namespace std;

	swap(m_blockSize, rhs.m_blockSize);
	swap(m_numBlocks, rhs.m_numBlocks);
	m_chunks.swap(rhs.m_chunks);
	swap(m_allocChunk, rhs.m_allocChunk);
	swap(m_deallocChunk, rhs.m_deallocChunk);
	m_cache.swap(rhs.m_cache);
}

// Allocate a memory block
void* FixedAllocator::Allocate()
{
	{
		m_cache_delguard = 0;
		if (m_cache.size() > 0)
		{
			void* p = m_cache.back();
			m_cache.pop_back();
			return p;
		}
	}

	if (m_allocChunk == 0 || m_allocChunk->m_blocksAvailable == 0)
	{
		Chunks::iterator i = m_chunks.begin();
		for (;; ++i)
		{
			if (i == m_chunks.end())
			{
				// Initialize
				m_chunks.reserve(m_chunks.size() + 1);
				m_chunks.emplace_back();
				m_allocChunk = &m_chunks.back();
				m_allocChunk->Init(m_blockSize, m_numBlocks);
				m_deallocChunk = &m_chunks.front();
				break;
			}
			if (i->m_blocksAvailable > 0)
			{
				m_allocChunk = &*i;
				break;
			}
		}
	}
	assert(m_allocChunk != 0);
	assert(m_allocChunk->m_blocksAvailable > 0);

	return m_allocChunk->Allocate(m_blockSize);
}

// Deallocate a memory block previously allocated with Allocate()
// (if that's not the case, the behavior is undefined)
void FixedAllocator::Deallocate(void* p)
{
	{
		if (m_cache_delguard < POOL_CACHE_CAPACITY)
		{
			m_cache_delguard++;
			if (m_cache.size() >= POOL_CACHE_CAPACITY)
			{
				void* pfront = m_cache.front();
				m_deallocChunk = VicinityFind(pfront);
				m_cache.erase(m_cache.begin());
				m_cache.push_back(p);
				p = pfront;
			}
			else
			{
				m_cache.push_back(p);
				return;
			}
		}
		else
		{
			for (auto i = m_cache.begin(); i != m_cache.end(); i = m_cache.erase(i))
			{
				m_deallocChunk = VicinityFind(*i);
				assert(m_deallocChunk);
				DoDeallocate(*i);
			}
		}
	}
	m_deallocChunk = VicinityFind(p);
	assert(m_deallocChunk);

	DoDeallocate(p);
}

PoolManager::PoolManager(
	std::size_t maxObjectSize)
	: m_pLastAlloc(0), m_pLastDealloc(0)
	, m_maxObjectSize(maxObjectSize)
{

}

namespace { // anoymous 

// See LWG DR #270
	struct CompareFixedAllocatorSize
		: std::binary_function<const FixedAllocator&, std::size_t, bool>
	{
		bool operator()(const FixedAllocator& x, std::size_t numBytes) const
		{
			return x.BlockSize() < numBytes;
		}
	};

} // anoymous namespace

void* PoolManager::Allocate(size_t n)
{
	if (n > m_maxObjectSize) return ::operator new(n);

	while (1)
	{
		if (m_pLastAlloc && m_pLastAlloc->BlockSize() == n)
		{
			return m_pLastAlloc->Allocate();
		}
		Pool::iterator it = std::lower_bound(m_pool.begin(), m_pool.end(), n,
			CompareFixedAllocatorSize());
		if (it == m_pool.end() || it->BlockSize() != n)
			break;
		m_pLastAlloc = &*it;
	}
	{
		m_pLastAlloc = &*m_pool.insert(m_pool.end(), FixedAllocator(n));
		m_pLastDealloc = &*m_pool.begin();
	}
	return m_pLastAlloc->Allocate();
}

void PoolManager::Deallocate(void* p, size_t n)
{
	if (n > m_maxObjectSize) return operator delete(p);

	FixedAllocator* pLastDealloc = nullptr;
	{
		if (m_pLastDealloc && m_pLastDealloc->BlockSize() == n)
		{
			m_pLastDealloc->Deallocate(p);
			return;
		}
		Pool::iterator i = std::lower_bound(m_pool.begin(), m_pool.end(), n,
			CompareFixedAllocatorSize());
		assert(i != m_pool.end());
		assert(i->BlockSize() == n);
		pLastDealloc = &*i;
	}
	pLastDealloc->Deallocate(p);
	{
		m_pLastDealloc = pLastDealloc;
	}
}
