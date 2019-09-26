// This is an open source non-commercial project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <iostream>
#include <windows.h>
#include <psapi.h>
#include "soalloc.h"

/* standard: 100 000
WorkingSetSize: 6746112
23.6573 msec.
1 000 000
WorkingSetSize: 42962944
159.186 msec.

soalloc: 100 000
WorkingSetSize: 5414912
23.0391 msec.
1 000 000
WorkingSetSize: 27582464
381.049 msec.

Optimizations:
WorkingSetSize: 27590656
286.518 msec.

WorkingSetSize: 27561984
183.352 msec.
*/

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

int main()
{
	/* begin High Frequency Counter */
	LARGE_INTEGER li;
	double pcf = 0.;
	__int64 iCounter = 0;
	auto InitCounter = [&li, &pcf]()
	{
		if (QueryPerformanceFrequency(&li))
			pcf = static_cast<double>(li.QuadPart) / 1000.;
	};
	auto StartCounter = [&li, &iCounter]()
	{
		QueryPerformanceCounter(&li);
		iCounter = li.QuadPart;
	};
	auto FinishCounter = [&li, &iCounter, &pcf]() -> double
	{
		QueryPerformanceCounter(&li);
		return static_cast<double>(li.QuadPart - iCounter) / pcf;
	};
	/* end High Frequency Counter */

	using namespace std;

	foo* x[1000000] = { 0 };

//	cout << "sizeof(foo):\t" << sizeof(foo) << endl;
	InitCounter();
	StartCounter();

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

	std::cout << std::endl << FinishCounter() << " msec." << std::endl;

	cout << "Finish!" << endl;
	
	return 0;
}