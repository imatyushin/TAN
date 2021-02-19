#pragma once
#include <string>
#include <vector>

bool                    compareIgnoreCase(const std::string & first, const std::string & second);

std::string             toString(const std::wstring & inputString);
std::wstring            toWideString(const std::string & inputString);

std::string             toUpper(const std::string & inputString);