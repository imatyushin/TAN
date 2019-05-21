#pragma once

#include <thread>
#include <chrono>
#include <iostream>
#include <cstring>

#ifdef _WIN32
#else
#include <pthread.h>
#endif

class PrioritizedThread:
    public std::thread
{
protected:
    int mPolicy;
    int mPriority;
    sched_param mSheduleParams;

public:
    PrioritizedThread(bool realTime = false):
        mPolicy(
            realTime
                ? SCHED_RR
                : SCHED_FIFO
          ),
        mPriority(
            realTime
                ? 2
                : 10
            )
    {
        mSheduleParams.sched_priority = mPriority;
    }

    virtual ~PrioritizedThread()
    {
    }

    PrioritizedThread& operator=(std::thread&& other) noexcept
    {
        swap(other);

        if(pthread_setschedparam(
            native_handle(),
            mPolicy,
            &mSheduleParams
            ))
        {
            std::cerr << "Error: failed to set thread priority (" << std::strerror(errno) << ") " << std::endl;
        }

        //??? SetSecurityInfo(GetCurrentProcess(), SE_WINDOW_OBJECT, PROCESS_SET_INFORMATION, 0, 0, 0, 0);
        //done - SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
        //done - SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

        return *this;
    }

    virtual void WaitCloseInfinite()
    {
        while(joinable())
        {
           std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
};