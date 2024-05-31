#ifdef _DEBUG

#include <crtdbg.h>

//
// Show CRT errors in console instead of messagebox,
// which is invisible during CI (GitHub Actions).
//
// Example: ATLASSERT(false && "Too many categories defined")
// in atltrace.h message after recent Visual Studio upgrade
//

class CRTDbg
{
public:
    CRTDbg()
    {
        _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
        _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
        _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    }
};

namespace {
CRTDbg sCRTDbg;
}   // namespace

#endif  // _DEBUG
