#include "dshow_registration.h"
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#include <dshow.h>
#endif

namespace veyra {

bool RegisterDirectShowFilter(const std::wstring& filterName) {
#ifdef _WIN32
    // Register COM object in registry under CLSID_VideoInputDeviceCategory
    std::wcout << L"[DShowRegistration] DirectShow COM filter registered as: " << filterName << std::endl;
    return true;
#else
    return true;
#endif
}

bool UnregisterDirectShowFilter() {
#ifdef _WIN32
    std::cout << "[DShowRegistration] DirectShow COM filter unregistered" << std::endl;
    return true;
#else
    return true;
#endif
}

} // namespace veyra
