#pragma once

#include <vector>

#ifndef DEFAULT_CHUNK_SIZE
#define DEFAULT_CHUNK_SIZE 65536	//4096
#endif

#ifndef POOL_CACHE_CAPACITY
#define POOL_CACHE_CAPACITY 16
#endif

#ifndef MAX_SMALL_OBJECT_SIZE
#define MAX_SMALL_OBJECT_SIZE 128
#endif

class FixedAllocator
{
private:
	struct Chunk
	{
		void Init(size_t blockSize, unsigned char blocks);
		void* Allocate(size_t blockSize);
		void Deallocate(void* p, size_t blockSize);
		void Reset(size_t blockSize, unsigned char blocks);
		void Release();

		unsigned char* m_pData;
		unsigned char m_firstAvailableBlock;
		unsigned char m_blocksAvailable;
	};

	// Internal functions        
	void DoDeallocate(void* p);

	Chunk* VicinityFind(void* p);

	// Data
	std::size_t m_blockSize;
	unsigned char m_numBlocks;
	typedef std::vector<Chunk> Chunks;
	Chunks m_chunks;
	Chunk* m_allocChunk;
	Chunk* m_deallocChunk;
	// For ensuring proper copy semantics
	mutable const FixedAllocator* m_prev;
	mutable const FixedAllocator* m_next;
	// cache
	std::vector<void*> m_cache;
	int m_cache_delguard;

public:
	// Create a FixedAllocator able to manage blocks of 'blockSize' size
	explicit FixedAllocator(std::size_t blockSize);

	FixedAllocator(const FixedAllocator& rhs);

	FixedAllocator& operator=(const FixedAllocator& rhs);

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
		return m_blockSize;
	}
};

class PoolManager
{
public:
	PoolManager(
		std::size_t maxObjectSize = MAX_SMALL_OBJECT_SIZE);

	PoolManager(const PoolManager&) = delete;
	PoolManager& operator=(const PoolManager&) = delete;

	void* Allocate(std::size_t n);
	void Deallocate(void* p, std::size_t n);
private:
	typedef std::vector<FixedAllocator> Pool;
	Pool m_pool;
	FixedAllocator* m_pLastAlloc;
	FixedAllocator* m_pLastDealloc;
	std::size_t m_maxObjectSize;
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

using PoolAllocator = Singleton<PoolManager>;

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
		return alloc(size, true);
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