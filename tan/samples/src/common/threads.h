#pragma once

#include <thread>
#include <chrono>
#include <iostream>
#include <cstring>
//#include <mutex>
//#include <condition_variable>

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
    //std::mutex mLockMutex;
    //std::condition_variable mQuitCondition;

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
            std::cerr << "Error: failed to set thread priority (" << std::strerror(errno) << ")" << std::endl;
        }
#endif

        //??? SetSecurityInfo(GetCurrentProcess(), SE_WINDOW_OBJECT, PROCESS_SET_INFORMATION, 0, 0, 0, 0);
        //done - SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
        //done - SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

        return *this;
    }

    virtual void WaitCloseInfinite()
    {
        join();

        /*
        // wait for the detached thread
        std::unique_lock<std::mutex> lock(mLockMutex);
        mQuitCondition.wait(lock, []{ return ready; });
        */
    }
};