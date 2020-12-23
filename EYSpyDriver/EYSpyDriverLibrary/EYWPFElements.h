#include "TChar.h"
#include "string.h"
#include "windows.h"
#include "Winuser.h"
#include <string>
#include <vector>

//using namespace std;

typedef std::basic_string<TCHAR> tstring;                   //define   basic_string<TCHAR> as a member of the std namespace 
														//and at the same time use typedef to define the synonym tstring for it


namespace EY {
	namespace SpyDriver {

		public class Handles {

		public: struct child_data { tstring caption;	HWND handle; };

		private:
			std::vector<child_data> stuff;                      //define a vector of objecttype "child_data" (the struct defined above) named stuff

			BOOL add_window(HWND hwnd);

			static BOOL CALLBACK EnumChildWindows(HWND hwnd, LPARAM lParam);

		public:
			Handles& enum_windows(HWND hParentWnd);

			std::vector<child_data>& get_results()
			{
				return stuff;
			}
		};
	}
}