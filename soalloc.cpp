// This is an open source non-commercial project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <cassert>
#include <algorithm>
#include "soalloc.h"

////////////////////////////////////////////////////////////////////////////////
// FixedAllocator::Chunk::Init
// Initializes a chunk object
////////////////////////////////////////////////////////////////////////////////

void FixedAllocator::Chunk::Init(std::size_t blockSize, unsigned char blocks)
{
	assert(blockSize > 0);
	assert(blocks > 0);
	// Overflow check
	const std::size_t allocSize = blockSize * blocks;
	assert(allocSize / blockSize == blocks);

	// If this new operator fails, it will throw, and the exception will get
	// caught one layer up.
	m_pData = static_cast<unsigned char*>(::operator new (allocSize));
	Reset(blockSize, blocks);
}

////////////////////////////////////////////////////////////////////////////////
// FixedAllocator::Chunk::Reset
// Clears an already allocated chunk
////////////////////////////////////////////////////////////////////////////////

void FixedAllocator::Chunk::Reset(std::size_t blockSize, unsigned char blocks)
{
	assert(blockSize > 0);
	assert(blocks > 0);
	// Overflow check
	assert((blockSize * blocks) / blockSize == blocks);

	m_firstAvailableBlock = 0;
	m_blocksAvailable = blocks;

	unsigned char i = 0;
	for (unsigned char* p = m_pData; i != blocks; p += blockSize)
	{
		*p = ++i;
	}
}

////////////////////////////////////////////////////////////////////////////////
// FixedAllocator::Chunk::Release
// Releases the data managed by a chunk
////////////////////////////////////////////////////////////////////////////////

void FixedAllocator::Chunk::Release()
{
	assert(m_pData != nullptr);
	::operator delete (m_pData);
}

////////////////////////////////////////////////////////////////////////////////
// FixedAllocator::Chunk::Allocate
// Allocates a block from a chunk
////////////////////////////////////////////////////////////////////////////////

void* FixedAllocator::Chunk::Allocate(std::size_t blockSize)
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

////////////////////////////////////////////////////////////////////////////////
// FixedAllocator::Chunk::Deallocate
// Dellocates a block from a chunk
////////////////////////////////////////////////////////////////////////////////

void FixedAllocator::Chunk::Deallocate(void* p, std::size_t blockSize)
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

////////////////////////////////////////////////////////////////////////////////
// FixedAllocator::FixedAllocator
// Creates a FixedAllocator object of a fixed block size
////////////////////////////////////////////////////////////////////////////////

FixedAllocator::FixedAllocator(std::size_t blockSize)
	: blockSize_(blockSize)
	, allocChunk_(0)
	, deallocChunk_(0)
{
	assert(blockSize_ > 0);

	prev_ = next_ = this;

	std::size_t numBlocks = DEFAULT_CHUNK_SIZE / blockSize;
	if (numBlocks > UCHAR_MAX) numBlocks = UCHAR_MAX;
	else if (numBlocks == 0) numBlocks = 8 * blockSize;

	numBlocks_ = static_cast<unsigned char>(numBlocks);
	assert(numBlocks_ == numBlocks);
}

////////////////////////////////////////////////////////////////////////////////
// FixedAllocator::FixedAllocator(const FixedAllocator&)
// Creates a FixedAllocator object of a fixed block size
////////////////////////////////////////////////////////////////////////////////

FixedAllocator::FixedAllocator(const FixedAllocator& rhs)
	: blockSize_(rhs.blockSize_)
	, numBlocks_(rhs.numBlocks_)
	, chunks_(rhs.chunks_)
{
	prev_ = &rhs;
	next_ = rhs.next_;
	rhs.next_->prev_ = this;
	rhs.next_ = this;

	allocChunk_ = rhs.allocChunk_
		? &chunks_.front() + (rhs.allocChunk_ - &rhs.chunks_.front())
		: 0;

	deallocChunk_ = rhs.deallocChunk_
		? &chunks_.front() + (rhs.deallocChunk_ - &rhs.chunks_.front())
		: 0;
}

FixedAllocator& FixedAllocator::operator=(const FixedAllocator& rhs)
{
	FixedAllocator copy(rhs);
	copy.Swap(*this);
	return *this;
}

////////////////////////////////////////////////////////////////////////////////
// FixedAllocator::~FixedAllocator
////////////////////////////////////////////////////////////////////////////////

FixedAllocator::~FixedAllocator()
{
	if (prev_ != this)
	{
		prev_->next_ = next_;
		next_->prev_ = prev_;
		return;
	}

	assert(prev_ == next_);
	Chunks::iterator i = chunks_.begin();
	for (; i != chunks_.end(); ++i)
	{
		assert(i->m_blocksAvailable == numBlocks_);
		i->Release();
	}
}

////////////////////////////////////////////////////////////////////////////////
// FixedAllocator::Swap
////////////////////////////////////////////////////////////////////////////////

void FixedAllocator::Swap(FixedAllocator& rhs)
{
	using namespace std;

	swap(blockSize_, rhs.blockSize_);
	swap(numBlocks_, rhs.numBlocks_);
	chunks_.swap(rhs.chunks_);
	swap(allocChunk_, rhs.allocChunk_);
	swap(deallocChunk_, rhs.deallocChunk_);
}

////////////////////////////////////////////////////////////////////////////////
// FixedAllocator::Allocate
// Allocates a block of fixed size
////////////////////////////////////////////////////////////////////////////////

void* FixedAllocator::Allocate()
{
	if (allocChunk_ == nullptr || allocChunk_->m_blocksAvailable == 0)
	{
//		Chunks::iterator i = chunks_.begin();
//		for (;; ++i)
		allocChunk_ = nullptr;
		for(auto const &i: chunks_)
		{
/*			if (i == chunks_.end())
			{
				// Initialize
				chunks_.reserve(chunks_.size() + 1);
				Chunk newChunk;
				newChunk.Init(blockSize_, numBlocks_);
				chunks_.push_back(newChunk);
				allocChunk_ = &chunks_.back();
				deallocChunk_ = &chunks_.front();
				break;
			}	see below... */
			if (i.m_blocksAvailable > 0)
			{
				allocChunk_ = const_cast<Chunk*>(&i);
				break;
			}
		}
		if (allocChunk_ == nullptr)
		{
			// Initialize
			chunks_.reserve(chunks_.size() + 1);
			Chunk newChunk;
			newChunk.Init(blockSize_, numBlocks_);
			chunks_.push_back(newChunk);
			allocChunk_ = &chunks_.back();
			deallocChunk_ = &chunks_.front();
		}

	}
	assert(allocChunk_ != 0);
	assert(allocChunk_->m_blocksAvailable > 0);

	return allocChunk_->Allocate(blockSize_);
}

////////////////////////////////////////////////////////////////////////////////
// FixedAllocator::Deallocate
// Deallocates a block previously allocated with Allocate
// (undefined behavior if called with the wrong pointer)
////////////////////////////////////////////////////////////////////////////////

void FixedAllocator::Deallocate(void* p)
{
	assert(!chunks_.empty());
	assert(&chunks_.front() <= deallocChunk_);
	assert(&chunks_.back() >= deallocChunk_);

	deallocChunk_ = VicinityFind(p);
	assert(deallocChunk_);

	DoDeallocate(p);
}

////////////////////////////////////////////////////////////////////////////////
// FixedAllocator::VicinityFind (internal)
// Finds the chunk corresponding to a pointer, using an efficient search
////////////////////////////////////////////////////////////////////////////////

FixedAllocator::Chunk* FixedAllocator::VicinityFind(void* p)
{
	assert(!chunks_.empty());
	assert(deallocChunk_);

	const std::size_t chunkLength = numBlocks_ * blockSize_;

	Chunk* lo = deallocChunk_;
	Chunk* hi = deallocChunk_ + 1;
	Chunk* loBound = &chunks_.front();
	Chunk* hiBound = &chunks_.back() + 1;

	// Special case: deallocChunk_ is the last in the array
	if (hi == hiBound) hi = 0;

	for (;;)
	{
		if (lo)
		{
			//if (p >= lo->pData_ && p < lo->pData_ + chunkLength)
			if (static_cast<size_t>(static_cast<unsigned char*>(p) - lo->m_pData) < chunkLength)
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

	// assert(false);
	// return 0;
}

////////////////////////////////////////////////////////////////////////////////
// FixedAllocator::DoDeallocate (internal)
// Performs deallocation. Assumes deallocChunk_ points to the correct chunk
////////////////////////////////////////////////////////////////////////////////

void FixedAllocator::DoDeallocate(void* p)
{
	assert(deallocChunk_->m_pData <= p);
	assert(deallocChunk_->m_pData + numBlocks_ * blockSize_ > p);

	// call into the chunk, will adjust the inner list but won't release memory
	deallocChunk_->Deallocate(p, blockSize_);

	if (deallocChunk_->m_blocksAvailable == numBlocks_)
	{
		// deallocChunk_ is completely free, should we release it? 

		Chunk* lastChunk = &chunks_.back();

		// deallocChunk_ оказался последним в векторе
		if (lastChunk == deallocChunk_)
		{
			// check if we have two last chunks empty

			if (chunks_.size() > 1 &&
				deallocChunk_[-1].m_blocksAvailable == numBlocks_)
			{
				// Two free chunks, discard the last one
				lastChunk->Release();
				chunks_.pop_back();
				allocChunk_ = deallocChunk_ = &chunks_.front(); // <= allocChunk_ то зачем всегда сбрасывать ???
			}
			return;
		}
		// deallocChunk_ оказался не последним в векторе
		// но последний чанк пустой и его можно удалить
		if (lastChunk->m_blocksAvailable == numBlocks_)
		{
			// Two free blocks, discard one
			lastChunk->Release();
			chunks_.pop_back();
			allocChunk_ = deallocChunk_;	// <= allocChunk_ может уже указыать на незаполненный чанк, зачем всегда сбрасывать ???
			// нужно сдвинуть в конец свободный чанк
			lastChunk = &chunks_.back();
			if (lastChunk == deallocChunk_ || lastChunk->m_blocksAvailable == numBlocks_)
				return;
		}
//		else
		{
			// move the empty chunk to the end
			std::swap(*deallocChunk_, *lastChunk);
			allocChunk_ = &chunks_.back();	// <= allocChunk_ может уже указыать на незаполненный чанк, зачем всегда сбрасывать ???
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
// SmallObjAllocator::SmallObjAllocator
// Creates an allocator for small objects given chunk size and maximum 'small'
//     object size
////////////////////////////////////////////////////////////////////////////////

SmallObjAllocator::SmallObjAllocator(
	std::size_t chunkSize,
	std::size_t maxObjectSize)
	: pLastAlloc_(0), pLastDealloc_(0)
	, chunkSize_(chunkSize), maxObjectSize_(maxObjectSize)
{
}

////////////////////////////////////////////////////////////////////////////////
// SmallObjAllocator::Allocate
// Allocates 'numBytes' memory
// Uses an internal pool of FixedAllocator objects for small objects  
////////////////////////////////////////////////////////////////////////////////

void* SmallObjAllocator::Allocate(std::size_t numBytes)
{
//	std::lock_guard<std::mutex> lguard(m_mutex);

	if (numBytes > maxObjectSize_) return operator new(numBytes);

	if (pLastAlloc_ && pLastAlloc_->BlockSize() == numBytes)
	{
		return pLastAlloc_->Allocate();
	}
	Pool::iterator i = std::lower_bound(pool_.begin(), pool_.end(), numBytes);
	if (i == pool_.end() || i->BlockSize() != numBytes)
	{
		i = pool_.insert(i, FixedAllocator(numBytes));
		pLastDealloc_ = &*pool_.begin();
	}
	pLastAlloc_ = &*i;
	return pLastAlloc_->Allocate();
}

////////////////////////////////////////////////////////////////////////////////
// SmallObjAllocator::Deallocate
// Deallocates memory previously allocated with Allocate
// (undefined behavior if you pass any other pointer)
////////////////////////////////////////////////////////////////////////////////

void SmallObjAllocator::Deallocate(void* p, std::size_t numBytes)
{
//	std::lock_guard<std::mutex> lguard(m_mutex);
	if (numBytes > maxObjectSize_) return operator delete(p);

	if (pLastDealloc_ && pLastDealloc_->BlockSize() == numBytes)
	{
		pLastDealloc_->Deallocate(p);
		return;
	}
	Pool::iterator i = std::lower_bound(pool_.begin(), pool_.end(), numBytes);
	assert(i != pool_.end());
	assert(i->BlockSize() == numBytes);
	pLastDealloc_ = &*i;
	pLastDealloc_->Deallocate(p);
}
