#pragma once

#include <cstdlib>
#include <vector>
//#include <bitset>

#ifndef DEFAULT_CHUNK_SIZE
#define DEFAULT_CHUNK_SIZE 4096
#endif

#ifndef MAX_SMALL_OBJECT_SIZE
#define MAX_SMALL_OBJECT_SIZE 256
#endif

////////////////////////////////////////////////////////////////////////////////
// class FixedAllocator
// Offers services for allocating fixed-sized objects
////////////////////////////////////////////////////////////////////////////////

class FixedAllocator
{
	struct Chunk
	{
		/** Initializes a just-constructed Chunk.
		 @param blockSize Number of bytes per block.
		 @param blocks Number of blocks per Chunk.
		 @return True for success, false for failure.
		 */

		void Init(std::size_t blockSize, unsigned char blocks);
		/** Allocate a block within the Chunk.  Complexity is always O(1), and
		 this will never throw.  Does not actually "allocate" by calling
		 malloc, new, or any other function, but merely adjusts some internal
		 indexes to indicate an already allocated block is no longer available.
		 @return Pointer to block within Chunk.
		 */

		void* Allocate(std::size_t blockSize);
		/** Deallocate a block within the Chunk. Complexity is always O(1), and
		 this will never throw.  For efficiency, this assumes the address is
		 within the block and aligned along the correct byte boundary.  An
		 assertion checks the alignment, and a call to HasBlock is done from
		 within VicinityFind.  Does not actually "deallocate" by calling free,
		 delete, or other function, but merely adjusts some internal indexes to
		 indicate a block is now available.
		 */
		void Deallocate(void* p, std::size_t blockSize);

		/** Resets the Chunk back to pristine values. The available count is
		 set back to zero, and the first available index is set to the zeroth
		 block.  The stealth indexes inside each block are set to point to the
		 next block. This assumes the Chunk's data was already using Init.
		 */
		void Reset(std::size_t blockSize, unsigned char blocks);

		/// Releases the allocated block of memory.
		void Release();
		/// Pointer to array of allocated blocks.
		unsigned char* pData_;
		/// Index of first empty block.
		unsigned char firstAvailableBlock_;
		/// Count of empty blocks.
		unsigned char blocksAvailable_;
	};
	// Internal functions        
	void DoDeallocate(void* p);
	Chunk* VicinityFind(void* p);

	// Data 
	std::size_t blockSize_;
	unsigned char numBlocks_;
	typedef std::vector<Chunk> Chunks;
	Chunks chunks_;
	Chunk* allocChunk_;
	Chunk* deallocChunk_;
	// For ensuring proper copy semantics
	mutable const FixedAllocator* prev_;
	mutable const FixedAllocator* next_;

public:
	// Create a FixedAllocator able to manage blocks of 'blockSize' size
	explicit FixedAllocator(std::size_t blockSize = 0);
	FixedAllocator(const FixedAllocator&);
	FixedAllocator& operator=(const FixedAllocator&);
	~FixedAllocator();

	void Swap(FixedAllocator& rhs);

	// Allocate a memory block
	void* Allocate();
	// Deallocate a memory block previously allocated with Allocate()
	// (if that's not the case, the behavior is undefined)
	void Deallocate(void* p);
	// Returns the block size with which the FixedAllocator was initialized
	std::size_t BlockSize() const
	{
		return blockSize_;
	}
	// Comparison operator for sorting 
	bool operator<(std::size_t rhs) const
	{
		return BlockSize() < rhs;
	}
};

////////////////////////////////////////////////////////////////////////////////
// class SmallObjAllocator
// Offers services for allocating small-sized objects
////////////////////////////////////////////////////////////////////////////////

class SmallObjAllocator
{
public:
	SmallObjAllocator(
		std::size_t chunkSize = DEFAULT_CHUNK_SIZE,
		std::size_t maxObjectSize = MAX_SMALL_OBJECT_SIZE);

	void* Allocate(std::size_t numBytes);
	void Deallocate(void* p, std::size_t size);

private:
	SmallObjAllocator(const SmallObjAllocator&);
	SmallObjAllocator& operator=(const SmallObjAllocator&);

	typedef std::vector<FixedAllocator> Pool;
	Pool pool_;
	FixedAllocator* pLastAlloc_;
	FixedAllocator* pLastDealloc_;
	std::size_t chunkSize_;
	std::size_t maxObjectSize_;
};

// Singleton
template <typename T>
class Singleton
{
private:
	Singleton() = default;
	Singleton(const Singleton&) = delete;
	Singleton& operator=(Singleton&) = delete;
	~Singleton() = default;

public:
	static T& GetInstance() noexcept
	{
		static T s;
		return s;
	}
};

using PoolAllocator = Singleton<SmallObjAllocator>;	//Singleton<PoolManager>;

template<typename T>
class soalloc
{
private:
	static void* alloc(size_t size, bool nothrow = false)
	{
		if (size == 0) size = 1;
		void* ptr = PoolAllocator::GetInstance().Allocate(size);
		if (ptr == nullptr && !nothrow)
		{
			std::bad_alloc exception;
			throw exception;
		}
		return ptr;
	}
	static void free(void* ptr) noexcept
	{
		if (ptr) PoolAllocator::GetInstance().Deallocate(ptr, sizeof(T));
	}
public:
	static void* operator new(size_t size) // throwing
	{
//		std::cout << "throwing operator new (" << typeid(T).name() << "), size: " << size << std::endl;
		return alloc(size);
	}
	static void operator delete (void* ptr) noexcept // ordinary
	{
//		std::cout << "ordinary operator delete (" << typeid(T).name() << "): " << ptr << std::endl;
		free(ptr);
	}
	static void* operator new (std::size_t size, const std::nothrow_t& nothrow_value) noexcept // nothrow
	{
//		std::cout << "nothrow operator new (" << typeid(T).name() << "), size: " << size << std::endl;
		try
		{
			return alloc(size, true);
		}
		catch (...)
		{
			return nullptr;
		}
	}
	static void operator delete (void* ptr, const std::nothrow_t& nothrow_constant) noexcept
	{
//		std::cout << "nothrow operator delete (" << typeid(T).name() << "): " << ptr << std::endl;
		free(ptr);
	}
	static void* operator new (std::size_t size, void* ptr) noexcept // placement
	{
//		std::cout << "placement operator new (" << typeid(T).name() << "), size: " << size << std::endl;
		return ptr;
	}
	static void operator delete (void* ptr, void* voidptr2) noexcept
	{
//		std::cout << "placement operator delete (" << typeid(T).name() << "): " << ptr << std::endl;
		return;
	}
	static void* operator new[](size_t size) // throwing
	{
//		std::cout << "throwing operator new[] (" << typeid(T).name() << "): " << size << std::endl;
		return ::operator new[](size);
	}
	static void operator delete[](void* ptr) noexcept // ordinary
	{
//		std::cout << "ordinary operator delete[] (" << typeid(T).name() << "): " << ptr << std::endl;
		::operator delete[](ptr);
	}
	static void* operator new[](std::size_t size, const std::nothrow_t& nothrow_value) noexcept // nothrow
	{
//		std::cout << "nothrow operator new[] (" << typeid(T).name() << "), size: " << size << std::endl;
		return ::operator new[](size, nothrow_value);
	}
	static void operator delete[](void* ptr, const std::nothrow_t& nothrow_constant) noexcept
	{
//		std::cout << "nothrow operator delete[] (" << typeid(T).name() << "): " << ptr << std::endl;
		::operator delete[](ptr, nothrow_constant);
	}
	static void* operator new[](std::size_t size, void* ptr) noexcept // placement
	{
//		std::cout << "placement operator new[] (" << typeid(T).name() << "), size: " << size << std::endl;
		return ::operator new[](size, ptr);
	}
	static void operator delete[](void* ptr, void* voidptr2) noexcept
	{
//		std::cout << "placement operator delete[] (" << typeid(T).name() << "): " << ptr << std::endl;
		return;
	}
};