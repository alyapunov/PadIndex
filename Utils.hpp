#pragma once

#include <stdlib.h>
#include <iostream>

#include "Timer.hpp"
#include "Win.hpp"

inline void fatal(const char* msg)
{
    std::cerr << msg << std::endl;
    exit(0);
}

inline void check(bool expr, const char* msg)
{
    if (!expr)
        fatal(msg);
}

class CTitle
{
public:
    CTitle(const std::string& message)
    {
        m_Timer.Start();
        std::cout << message.c_str() << "...";
    }
    ~CTitle()
    {
        m_Timer.Stop();
        std::cout << " done in " << m_Timer.ElapsedMilliSec() << " milliseconds." << std::endl;
    }
private:
    CTimer m_Timer;
};
