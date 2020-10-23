#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <map>

class ScopeLogger
{
protected:
    typedef std::map<std::thread::id, uint32_t>
                        levels;

    std::string         mMessage;
    levels::iterator    mLevelIterator;

    levels & GetStorage()
    {
        static levels Levels;

        return Levels;
    }

public:
    ScopeLogger(const std::string & message1, const std::string & message2):
        mMessage(message2.length() ? message1 + ": " + message2 : message1)
    {
        auto &Levels = GetStorage();
        auto threadId(std::this_thread::get_id());
        mLevelIterator = Levels.find(threadId);

        if(mLevelIterator == Levels.end())
        {
            mLevelIterator = Levels.insert({threadId, 0}).first;
        }
        else
        {
            ++((*mLevelIterator).second);
        }

        std::cout << std::string((*mLevelIterator).second, ' ') << ">>" << mMessage << std::endl;
    }

    ScopeLogger(const std::string & message):
        ScopeLogger(message, "")
    {
    }

    ~ScopeLogger()
    {
        std::cout << std::string((*mLevelIterator).second, ' ') << "<<" << mMessage << std::endl;
        --((*mLevelIterator).second);
    }
};

#ifndef SL(x)
#define SL(x) ScopeLogger scopeLogger ## __LINE__(__FUNCTION__, x)
#endif

#ifndef FL
#define FL SL(__FUNCTION__)
#endif
