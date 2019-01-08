#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>

#include "Timer.hpp"
#include "Utils.hpp"

#include "Win.hpp"

struct CDbFileReader
{
    CDbFileReader(const std::string& filename, const std::initializer_list<const char*>& fields)
    {
        Open(filename, fields);
    }

    void Open(const std::string& filename, const std::initializer_list<const char*>& fields)
    {
        m_Timer.Start();

        if (m_File.is_open())
            m_File.close();
        std::cout << "Loading file " << filename << "... ";
        m_File.open(filename, std::fstream::in);
        if (!m_File.is_open())
            m_File.open("../" + filename, std::fstream::in);
        check(m_File.is_open(), "file not found!");

        m_Fields.resize(fields.size());
        check(readLineInteral(), "wrong file format!");
        check(std::equal(m_Fields.begin(), m_Fields.end(), fields.begin()),
              "wrong file header!");
    }

    bool ReadLine()
    {
        bool res = readLineInteral();
        if (!res)
        {
            m_Timer.Stop();
            std::cout << "done in " << m_Timer.ElapsedMilliSec() << " milliseconds." << std::endl;
        }
        return res;
    }

    const std::vector<std::string> Fields() const
    {
        return m_Fields;
    }

private:
    bool readLineInteral()
    {
        if (m_File.eof())
            return false;
        for (size_t i = 0; i < m_Fields.size(); i++)
            m_File >> m_Fields[i];
        return true;
    }

    std::vector<std::string> m_Fields;
    std::fstream m_File;
    CTimer m_Timer;
};
