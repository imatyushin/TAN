#include "StringUtility.h"

#include <cctype>
#include <algorithm>

bool compareIgnoreCase(const std::string & first, const std::string & second)
{
	if(first.size() != second.size())
	{
        return false;
	}

	for(auto firstChar = first.begin(), secondChar = second.begin(); firstChar != first.end(); ++firstChar, ++secondChar)
	{
		if(std::tolower(*firstChar) != std::tolower(*secondChar))
		{
			return false;
		}
	}

    return true;
}

std::string toString(const std::wstring & inputString)
{
	std::mbstate_t state = std::mbstate_t();

	const std::wstring::value_type *input(inputString.c_str());

    std::size_t length = std::wcsrtombs(nullptr, &input, 0, &state) + 1;
    std::vector<std::string::value_type> buffer(length);
    std::wcsrtombs(buffer.data(), &input, buffer.size(), &state);

	return buffer.data();
}

std::wstring toWideString(const std::string & inputString)
{
	std::mbstate_t state = std::mbstate_t();

	const std::string::value_type *input(inputString.c_str());

    std::size_t length = std::mbsrtowcs(nullptr, &input, 0, &state) + 1;
    std::vector<std::wstring::value_type> buffer(length);
    std::mbsrtowcs(buffer.data(), &input, buffer.size(), &state);

	return buffer.data();
}

std::string toUpper(const std::string & inputString)
{
	auto copy(inputString);

	std::transform(copy.begin(), copy.end(), copy.begin(), std::toupper);

	return copy;
}