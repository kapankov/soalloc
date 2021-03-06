/*******************************************************************************

	Allocator for small objects
	The idea and partially the code is borrowed from the book of Andrei
	Alexandrescu "Modern C++ Design: Generic Programming and Design Patterns 
	Applied". Copyright (c) 2001. Addison-Wesley.
	+ small performance improvements and bug fixes

	Multithreaded work has limitations: 
	1 - deleting an object should be performed from the same thread
	2 - you cannot delete the same object twice

*******************************************************************************/

#pragma once

#include <cstdlib>
#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <shared_mutex>


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
		void Init(std::size_t blockSize, unsigned char blocks);

		void* Allocate(std::size_t blockSize);

		void Deallocate(void* p, std::size_t blockSize);


		void Reset(std::size_t blockSize, unsigned char blocks);

		void Release();
		unsigned char* m_pData;
		unsigned char m_firstAvailableBlock;
		unsigned char m_blocksAvailable;
	};
	void DoDeallocate(void* p);
	Chunk* VicinityFind(void* p);

	std::size_t blockSize_;
	unsigned char numBlocks_;
	typedef std::vector<Chunk> Chunks;
	Chunks chunks_;
	Chunk* allocChunk_;
	Chunk* deallocChunk_;
	mutable const FixedAllocator* prev_;
	mutable const FixedAllocator* next_;

public:
	explicit FixedAllocator(std::size_t blockSize = 0);
	FixedAllocator(const FixedAllocator&);
	FixedAllocator& operator=(const FixedAllocator&);
	~FixedAllocator();

	void Swap(FixedAllocator& rhs);

	void* Allocate();
	void Deallocate(void* p);
	std::size_t BlockSize() const
	{
		return blockSize_;
	}
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

using SmallObjAllocatorMap = std::map<std::thread::id, SmallObjAllocator>;
using PoolAllocator = Singleton<std::pair<SmallObjAllocatorMap,std::shared_mutex>>;

template<typename T>
class soalloc
{
	static SmallObjAllocator* getSmallObjAllocator()
	{
		SmallObjAllocator* pSmallObjAllocator = nullptr;
		auto& map = PoolAllocator::GetInstance().first;
		std::shared_mutex& mutex = PoolAllocator::GetInstance().second;
		std::thread::id id = std::this_thread::get_id();
		{
			std::shared_lock<std::shared_mutex> lock(mutex);
			auto it = map.find(id);
			if (it != map.end())
				pSmallObjAllocator = &it->second;
		}
		if (!pSmallObjAllocator)
		{
			std::unique_lock<std::shared_mutex> lock(mutex);
			pSmallObjAllocator = &map[id];
		}
		return pSmallObjAllocator;
	}

	static void* alloc(size_t size, bool nothrow = false)
	{
		if (size == 0) size = 1;

		SmallObjAllocator* pSmallObjAllocator = getSmallObjAllocator();

		void* ptr = pSmallObjAllocator->Allocate(size); //pSmallObjAllocator ? pSmallObjAllocator->Allocate(size) : nullptr;
		if (ptr == nullptr && !nothrow)
		{
			std::bad_alloc exception;
			throw exception;
		}
		return ptr;
	}
	static void free(void* ptr) noexcept
	{
		if (ptr)
		{
			if (SmallObjAllocator * pSmallObjAllocator = getSmallObjAllocator())
				pSmallObjAllocator->Deallocate(ptr, sizeof(T));
		}
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