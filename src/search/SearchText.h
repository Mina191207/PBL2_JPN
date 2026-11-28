#pragma once
#include <string>
#include <string_view>

// Called only after TXT validation, on queries and the indexed resource fields.
std::string normalizeSearchText(std::string_view text);
