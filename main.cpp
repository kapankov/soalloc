// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <iostream>
#include <windows.h>
#include <psapi.h>
#include "soalloc.h"

//#pragma pack(push,1)
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
//#pragma pack(pop)

int main()
{
	using namespace std;

	foo* x[10000] = { 0 };

//	cout << "sizeof(foo):\t" << sizeof(foo) << endl;

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

	cout << "Finish!" << endl;
	
	return 0;
}