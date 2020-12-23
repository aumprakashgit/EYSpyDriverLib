#include "EYWPFElements.h"
#include "Stdafx.h"
//
////-----------------------------------------------------------------------------
////Get all child elements by parent window handle
////-----------------------------------------------------------------------------
//
BOOL EY::SpyDriver::Handles::add_window(HWND hwnd)
{
	TCHAR String[200] = { 0 };
	if (!hwnd)
		return TRUE;                                // Not a window, return TRUE to Enumwindows in order to get the next handle
	if (!::IsWindowVisible(hwnd))
		return TRUE;                                // Not visible, return TRUE to Enumwindows in order to get the next handle 
	LRESULT result = SendMessageW(hwnd, WM_GETTEXT, sizeof(String), (LPARAM)String);        //result stores the number of characters copied from the window
	if (!result)
		return TRUE;                                // No window title, return TRUE to Enumwindows in order to get the next handle
	child_data data;                                // define an object of type child_data called data
	data.handle = hwnd;                             //copy the handle to the data.handle member

	for (int i = 0; i < result; i++)                 //copy each character to data.caption by using push_back
		data.caption.push_back(String[i]);

	stuff.push_back(data);                          //Put the whole data (with its members data.caption and data.handel) struct in the vector "stuff" using pushback
	return TRUE;
}

BOOL EY::SpyDriver::Handles::EnumChildWindows(HWND hwnd, LPARAM lParam)
{
	Handles* ptr = reinterpret_cast<Handles*>(lParam);
	return ptr->add_window(hwnd);
}

EY::SpyDriver::Handles& EY::SpyDriver::Handles::enum_windows(HWND hParentWnd)
{
	stuff.clear();                                  //clean up
	EnumChildWindows(hParentWnd, reinterpret_cast<LPARAM>(this));
	return *this;
}

