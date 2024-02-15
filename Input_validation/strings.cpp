#include "strings.h"

namespace experis
{

//Todo template!!!
void GetDelimiterIdx(std::vector<unsigned short>& a_delimiterIdx, const char a_delimiterType, const std::string& a_string)
{
    short strIdx{0};
    for (char c : a_string)
    {
        if (c == a_delimiterType)
        {
            a_delimiterIdx.push_back(strIdx);
        } // else{} 
        ++strIdx;
    }
}

} //namespace experis