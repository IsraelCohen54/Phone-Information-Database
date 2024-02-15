#include "StreamInOutDirection_mt.h"

namespace experis
{

// ~~~~ derived StreamInOutOstream_mt Class ~~~
StreamInOutOstream_mt::StreamInOutOstream_mt(std::mutex& a_mutex, std::ostream& a_stream)
: StreamInOutDirection_mt{}
, m_outMutex{a_mutex}
, m_OstreamInOut_mt{a_stream}
{

}

StreamInOutOstream_mt& StreamInOutOstream_mt::operator<<(const std::string& a_string)
{
    std::unique_lock<std::mutex> lock(m_outMutex);

    m_OstreamInOut_mt << a_string;

    return *this;
}

StreamInOutOstream_mt& StreamInOutOstream_mt::operator<<(const unsigned short a_value)
{
    std::unique_lock<std::mutex> lock(m_outMutex);

    m_OstreamInOut_mt << a_value;

    return *this;
}

StreamInOutOstream_mt& StreamInOutOstream_mt::operator<<(const unsigned int a_value)
{
    std::unique_lock<std::mutex> lock(m_outMutex);

    m_OstreamInOut_mt << a_value;

    return *this;
}

StreamInOutOstream_mt& StreamInOutOstream_mt::operator<<(const unsigned long long a_value)
{
    std::unique_lock<std::mutex> lock(m_outMutex);

    m_OstreamInOut_mt << a_value;

    return *this;
}

void StreamInOutOstream_mt::GetLine(std::string& untrust_userInput)
{
    std::getline(std::cin, untrust_userInput);
}

// ~~~~ derived StreamInOutTelnet Class ~~~

StreamInOutTelnet::StreamInOutTelnet(simplenet::SimpleNetMT::Connection& a_stream, std::mutex& a_mutex)
: StreamInOutDirection_mt{}
, m_outMutex{a_mutex}
, m_streamTelnet{a_stream}
{

}

StreamInOutTelnet& StreamInOutTelnet::operator<<(const std::string& a_string)
{
    std::unique_lock<std::mutex> lock(m_outMutex);
    m_streamTelnet.Write(a_string);
    return *this;
}

StreamInOutTelnet& StreamInOutTelnet::operator<<(const unsigned short a_value)
{
    std::unique_lock<std::mutex> lock(m_outMutex);
    m_streamTelnet.Write(std::to_string(a_value));
    return *this;
}

StreamInOutTelnet& StreamInOutTelnet::operator<<(unsigned int a_value)
{
    std::unique_lock<std::mutex> lock(m_outMutex);
    m_streamTelnet.Write(std::to_string(a_value));
    return *this;
}

StreamInOutTelnet& StreamInOutTelnet::operator<<(unsigned long long a_value)
{
    std::unique_lock<std::mutex> lock(m_outMutex);
    m_streamTelnet.Write(std::to_string(a_value));
    return *this;
}

void StreamInOutTelnet::GetLine(std::string& a_untrust_userInput)
{        
    a_untrust_userInput = m_streamTelnet.ReadLine(); //Todo move ctor? meybe implemented at Yariv lib
}

} // namespace experis