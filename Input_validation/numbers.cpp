#include "numbers.h"

namespace experis
{

bool OnlyDigits(const std::string& untrust_num)
{
	for (char c : untrust_num)
    {
        if (!std::isdigit(c))
        {
            return false;
        }
    }
    return true;
}

bool OneToFive(const std::string& untrust_num)
{
    int oneToFive{};
    try
    {
        oneToFive = std::stoi(untrust_num);
        if (oneToFive >= 1 && oneToFive <= 5)
        {
            return true;
        }
    }
    catch (const std::exception&)
    {
        return false;
    }
    return false;
}

} // namespace experis