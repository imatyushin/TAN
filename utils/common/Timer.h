/* ************************************************************************
 * Copyright 2013 Advanced Micro Devices, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * ************************************************************************/
#pragma once

struct BasicTimer
{
    bool Started;

    BasicTimer():
        Started(false)
    {
    }

    void Start()
    {
        Started = true;
    }

    void Stop()
    {
        Started = false;
    }

    inline bool IsStarted() const
    {
        return Started;
    }
};

#ifdef WIN32

struct Timer:
    public BasicTimer
{
    LARGE_INTEGER start, stop, freq;

public:
    Timer() { QueryPerformanceFrequency( &freq ); }

    void Start()
    {
        BasicTimer::Start();

        QueryPerformanceCounter(&start);
    }

    double Sample()
    {
        QueryPerformanceCounter  ( &stop );
        double time = (double)(stop.QuadPart-start.QuadPart) / (double)(freq.QuadPart);
        return time;
    }
};

#elif defined(__APPLE__) || defined(__MACOSX)

#include <mach/clock.h>
#include <mach/mach.h>

struct Timer:
    public BasicTimer
{
    clock_serv_t clock;
    mach_timespec_t start, end;

public:
    Timer() { host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &clock); }
    ~Timer() { mach_port_deallocate(mach_task_self(), clock); }

    void Start()
    {
        BasicTimer::Start();

        clock_get_time(clock, &start);
    }

    double Sample()
    {
        clock_get_time(clock, &end);
        double time = 1000000000L * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
        return time * 1E-9;
    }
};

#else

#include <time.h>
#include <math.h>

struct Timer:
    public BasicTimer
{
    struct timespec start, end;

public:
    void Start()
    {
        BasicTimer::Start();

        clock_gettime(CLOCK_MONOTONIC, &start);
    }

    double Sample()
    {
        clock_gettime(CLOCK_MONOTONIC, &end);
        double time = 1000000000L * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
        return time * 1E-9;
    }
};

#endif