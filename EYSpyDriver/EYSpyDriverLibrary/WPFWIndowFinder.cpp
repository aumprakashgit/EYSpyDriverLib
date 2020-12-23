#include "WPFWIndowFinder.h"
#include <iostream>
#include <windows.h>
#include "Stdafx.h"
using namespace System;

BOOL EY::SpyDriver::EnumChildProc(HWND hwnd, LPARAM lParam)
{
	std::cout << "hwnd_Child = " << hwnd << std::endl;
	return TRUE; // must return TRUE; If return is FALSE it stops the recursion
}

int EY::SpyDriver::FindCountofWindow(LPCWSTR childElementName)
{
		HWND hwnd = FindWindow(0, childElementName);
	EnumChildWindows(hwnd, EnumChildProc, 0);

	system("PAUSE");
	return 0;
}