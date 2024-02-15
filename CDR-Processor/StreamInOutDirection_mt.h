#pragma once

#include <mutex>
#include <ostream>
#include <iostream>
#include "SimpleNetMT.h"

namespace experis
{

class StreamInOutDirection_mt
{
public:
    explicit StreamInOutDirection_mt() = default;
    StreamInOutDirection_mt(const StreamInOutDirection_mt& a_other) = delete;
    StreamInOutDirection_mt& operator=(const StreamInOutDirection_mt& a_other) = delete;
    virtual ~StreamInOutDirection_mt() = default;
        
    virtual StreamInOutDirection_mt& operator<<(const std::string& a_string) = 0;
    virtual StreamInOutDirection_mt& operator<<(const unsigned short a_value) = 0;
    virtual StreamInOutDirection_mt& operator<<(const unsigned int a_value) = 0;
    virtual StreamInOutDirection_mt& operator<<(const unsigned long long a_value) = 0;
    
    virtual void GetLine(std::string& a_untrust_userInput) = 0;
};

class StreamInOutOstream_mt : public StreamInOutDirection_mt
{
public:
    explicit StreamInOutOstream_mt() = delete;
    explicit StreamInOutOstream_mt(std::mutex& a_mutex, std::ostream& a_stream);
    StreamInOutOstream_mt(const StreamInOutOstream_mt& a_other) = delete;
    StreamInOutOstream_mt& operator=(const StreamInOutOstream_mt& a_other) = delete;
    virtual ~StreamInOutOstream_mt() = default;
        
    virtual StreamInOutOstream_mt& operator<<(const std::string& a_string) override;
    virtual StreamInOutOstream_mt& operator<<(const unsigned short a_value) override;
    virtual StreamInOutOstream_mt& operator<<(const unsigned int a_value) override;
    virtual StreamInOutOstream_mt& operator<<(const unsigned long long a_value) override;

    virtual void GetLine(std::string& a_untrust_userInput) override;

private:
    std::ostream& m_OstreamInOut_mt;
    std::mutex& m_outMutex;
};

class StreamInOutTelnet : public StreamInOutDirection_mt
{
public:
    explicit StreamInOutTelnet() = delete;
    explicit StreamInOutTelnet(simplenet::SimpleNetMT::Connection& a_stream, std::mutex& a_mutex);
    StreamInOutTelnet(const StreamInOutTelnet& a_other) = delete;
    StreamInOutDirection_mt& operator=(const StreamInOutTelnet& a_other) = delete;
    virtual ~StreamInOutTelnet() = default;

    virtual StreamInOutTelnet& operator<<(const std::string& a_string) override;
    virtual StreamInOutTelnet& operator<<(const unsigned short a_value) override;
    virtual StreamInOutTelnet& operator<<(unsigned int a_value) override;
    virtual StreamInOutTelnet& operator<<(unsigned long long a_value) override;

    virtual void GetLine(std::string& a_untrust_userInput) override;

private:
    std::mutex& m_outMutex;
    simplenet::SimpleNetMT::Connection& m_streamTelnet;
};

} // experis namespace