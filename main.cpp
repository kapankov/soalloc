// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <iostream>
#include <windows.h>
#include <psapi.h>

//#pragma pack(push,1)
struct foo
{
	double x;
	int y;
	char z;
public:
	foo(): x(.0), y(0), z(0)
	{
		throw 0;
	}
	static void* operator new(size_t size) // throwing
	{
		std::cout << "throwing operator new" << std::endl;
		return ::operator new(size);
	}
	static void operator delete (void* ptr) noexcept // ordinary
	{
		std::cout << "ordinary operator delete: " << ptr << std::endl;
		::operator delete(ptr);
	}
	static void* operator new (std::size_t size, const std::nothrow_t& nothrow_value) noexcept // nothrow
	{
		std::cout << "nothrow operator new" << std::endl;
		return ::operator new(size, nothrow_value);
	}
	static void operator delete (void* ptr, const std::nothrow_t& nothrow_constant) noexcept
	{
		std::cout << "nothrow operator delete: " << ptr << std::endl;
		::operator delete(ptr, nothrow_constant);
	}
	static void* operator new (std::size_t size, void* ptr) noexcept // placement
	{
		std::cout << "placement operator new" << std::endl;
		return ::operator new(size, ptr);
	}
	static void operator delete (void* ptr, void* voidptr2) noexcept
	{
		std::cout << "placement operator delete: " << ptr << std::endl;
		::operator delete(ptr, voidptr2);
	}
	static void* operator new[](size_t size) // throwing
	{
		std::cout << "throwing operator new[]:\t" << size << std::endl;
		return ::operator new[](size);
	}
	static void operator delete[](void* ptr) noexcept // ordinary
	{
		std::cout << "ordinary operator delete[]: " << ptr << std::endl;
		::operator delete[](ptr);
	}
	static void* operator new[](std::size_t size, const std::nothrow_t& nothrow_value) noexcept // nothrow
	{
		std::cout << "nothrow operator new[]" << std::endl;
		return ::operator new[](size, nothrow_value);
	}
	static void operator delete[](void* ptr, const std::nothrow_t& nothrow_constant) noexcept
	{
		std::cout << "nothrow operator delete[]: " << ptr << std::endl;
		::operator delete[](ptr, nothrow_constant);
	}
	static void* operator new[](std::size_t size, void* ptr) noexcept // placement
	{
		std::cout << "placement operator new[]" << std::endl;
		return ::operator new[](size, ptr);
	}
	static void operator delete[](void* ptr, void* voidptr2) noexcept
	{
		std::cout << "placement operator delete[]: " << ptr << std::endl;
		::operator delete[](ptr, voidptr2);
	}
};
//#pragma pack(pop)

int main()
{
	using namespace std;

	foo* x[10] = { 0 };

//	cout << "sizeof(foo):\t" << sizeof(foo) << endl;

	for (size_t i = 0, n = sizeof(x) / sizeof(foo*); i < n; ++i)
	{
		//x[i] = new foo(); // throwing call
		try
		{
			x[i] = new (std::nothrow) foo(); // nothrow call
		}
		catch (...)
		{
			std::cout << "caught the exception" << std::endl;
		}
		cout << x[i] << endl;
	}

	foo* y = nullptr;
	try
	{
		y = new foo[10]();
	}
	catch(...)
	{
		std::cout << "caught the exception" << std::endl;
	}

	PROCESS_MEMORY_COUNTERS pmc;
	if (::GetProcessMemoryInfo(::GetCurrentProcess(), &pmc, sizeof(pmc)))
		cout << "WorkingSetSize:\t" << pmc.WorkingSetSize << endl;

	delete[] y;

	for (size_t i = 0, n = sizeof(x) / sizeof(foo*); i < n; ++i)
		delete x[i];

	cout << "Finish!" << endl;
	
	return 0;
}