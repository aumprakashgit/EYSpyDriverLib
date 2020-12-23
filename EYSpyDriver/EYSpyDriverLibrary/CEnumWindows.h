#include <wtypes.h>


namespace EY
{
	namespace SpyDriver
	{
		public class CEnumWindows
		{
		public:
			CEnumWindows(HWND hWndParent);
			virtual ~CEnumWindows();

			HWND Window(int nIndex) const { return (HWND)m_apWindowHandles.GetAt(nIndex); }
			int Count() const { return m_apWindowHandles.GetCount(); }

		private:
			BOOL CALLBACK TheEnumProc(HWND hWnd, LPARAM lParam);

			CPtrArray m_apWindowHandles;
		};
	}
}