// This is an open source non-commercial project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <iostream>
#include <chrono>
#include <thread>
#include <windows.h>
#include <psapi.h>
#include "soalloc.h"

class Stopwatch final
{
public:

	using elapsed_resolution = std::chrono::milliseconds;

	Stopwatch()
	{
		Reset();
	}

	void Reset()
	{
		reset_time = clock.now();
	}

	elapsed_resolution Elapsed()
	{
		return std::chrono::duration_cast<elapsed_resolution>(clock.now() - reset_time);
	}

private:

	std::chrono::high_resolution_clock clock;
	std::chrono::high_resolution_clock::time_point reset_time;
};

/*#pragma pack(push,1)*/
struct foo: public soalloc<foo>
{
	double x;
	int y;
	char z;

	foo(): x(.0), y(0), z(0)
	{
//		throw 0;
	}
};
/*#pragma pack(pop)*/

void testCycle()
{
	using namespace std;
	foo* x[1000000] = { 0 };
	//	cout << "sizeof(foo):\t" << sizeof(foo) << endl;
	Stopwatch sw;
	for (size_t i = 0, n = sizeof(x) / sizeof(foo*); i < n; ++i)
	{
		x[i] = new foo(); // throwing call
/*		try
		{
			x[i] = new (std::nothrow) foo(); // nothrow call
		}
		catch (...)
		{
			std::cout << "caught the exception" << std::endl;
		}*/
		//		cout << x[i] << endl;
	}
	/*	foo* y = nullptr;
		try
		{
			y = new foo[10]();
		}
		catch(...)
		{
			std::cout << "caught the exception" << std::endl;
		}*/

	PROCESS_MEMORY_COUNTERS pmc;
	if (::GetProcessMemoryInfo(::GetCurrentProcess(), &pmc, sizeof(pmc)))
		cout << "WorkingSetSize:\t" << pmc.WorkingSetSize << endl;

	//	delete[] y;

	for (size_t i = 0, n = sizeof(x) / sizeof(foo*); i < n; ++i)
		delete x[i];

	std::cout << std::endl << sw.Elapsed().count() << " msec." << std::endl;

	cout << "Finish!" << endl;
}

template <typename T>
void testSingleThread()
{
	using namespace std;
	unsigned int count = 100000000; // (std::numeric_limits<unsigned int>::max)();
	unsigned int newCalls = 0;
	unsigned int delCalls = 0;
	T* arr[RAND_MAX + 1] = { nullptr };
	cout << "Second test!" << endl;
	Stopwatch sw;

	while (count--)
	{
		int iRand = rand();
		if (arr[iRand] == nullptr)
		{
			arr[iRand] = new T();
			newCalls++;
		}
		else
		{
			delete arr[iRand];
			arr[iRand] = nullptr;
			delCalls++;
		}
		//		if (count%100000==0) std::cout << "+";
	}
	std::cout << std::endl << sw.Elapsed().count() << " msec." << std::endl;
	std::cout << "new calls: " << newCalls << std::endl;
	std::cout << "delete calls: " << delCalls << std::endl;

	for (int i = 0; i < (RAND_MAX + 1); ++i)
	{
		//if (arr[i])  <- not necessary
		delete arr[i];
	}

	std::cout << std::endl;
}

template <typename T>
void testMultiThread()
{
	const unsigned int maxthreads = 32;
	unsigned int n = std::thread::hardware_concurrency();
	std::cout << n << " concurrent threads are supported.\n";
	std::thread t[maxthreads];
	for (unsigned int i = 0; i < n && i < maxthreads; ++i)
		t[i] = std::thread([]() mutable { testSingleThread<T>(); });
	for (unsigned int i = 0; i < n && i < maxthreads; ++i)
		t[i].join();
	std::cout << "Ready!" << "[" << std::this_thread::get_id() << "]" << std::endl;

}

int main()
{
//	testCycle();
//	testSingleThread<foo>();
	testMultiThread<foo>();
	return 0;
}