#pragma once

#include <thread>
#include <chrono>
#include <iostream>
#include <cstring>

#ifdef _WIN32
#include <Windows.h>
#include <AclAPI.h>
#else
#include <pthread.h>
#endif

class PrioritizedThread:
    public std::thread
{
protected:
    bool mRealtime;
#ifdef _WIN32
#else
    int mPolicy;
    int mPriority;
    sched_param mSheduleParams;
#endif

public:
    PrioritizedThread(bool realTime = false):
        mRealtime(realTime)
#ifdef  _WIN32
#else
        , mPolicy(
            realTime
                ? SCHED_RR
                : SCHED_FIFO
          )
        , mPriority(
            realTime
                ? 2
                : 10
            )
#endif
    {
#ifdef _WIN32
#else
        mSheduleParams.sched_priority = mPriority;
#endif
    }

    virtual ~PrioritizedThread()
    {
    }

    PrioritizedThread& operator=(std::thread&& other) noexcept
    {
        swap(other);

#ifdef _WIN32
        if(mRealtime)
        {
            SetSecurityInfo(native_handle(), SE_WINDOW_OBJECT, PROCESS_SET_INFORMATION, 0, 0, 0, 0);
            SetPriorityClass(native_handle(), REALTIME_PRIORITY_CLASS);
            SetThreadPriority(native_handle(), THREAD_PRIORITY_TIME_CRITICAL);
        }
#else
        if(pthread_setschedparam(
            native_handle(),
            mPolicy,
            &mSheduleParams
            ))
        {
            std::cerr << "Error: failed to set thread priority (" << std::strerror(errno) << ") " << std::endl;
        }
#endif

        //??? SetSecurityInfo(GetCurrentProcess(), SE_WINDOW_OBJECT, PROCESS_SET_INFORMATION, 0, 0, 0, 0);
        //done - SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
        //done - SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

        return *this;
    }

    virtual void WaitCloseInfinite()
    {
        std::cout << "Wait thread finish..." << std::endl;
        
        while(joinable())
        {
           std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
};