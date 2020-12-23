#include <windows.h>

//using namespace std;

namespace EY {
	namespace SpyDriver {


		BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lParam);

		int FindCountofWindow(LPCWSTR childElementName);

	}
}