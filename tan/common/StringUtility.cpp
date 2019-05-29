#include "StringUtility.h"

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

std::wstring toWideString(const std::string & inputString)
{
	std::mbstate_t state = std::mbstate_t();

	const char *input(inputString.c_str());

    std::size_t length = std::mbsrtowcs(nullptr, &input, 0, &state) + 1;
    std::vector<wchar_t> buffer(length);
    std::mbsrtowcs(buffer.data(), &input, buffer.size(), &state);

	return buffer.data();
}