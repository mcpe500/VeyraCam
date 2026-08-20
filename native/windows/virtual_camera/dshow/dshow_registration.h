#pragma once

#include <string>

namespace veyra {

bool RegisterDirectShowFilter(const std::wstring& filterName = L"Veyra Camera");
bool UnregisterDirectShowFilter();

} // namespace veyra
