//
// MIT license
//
// Copyright (c) 2019 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//


#ifndef GRAALUTIL_HPP_
#define GRAALUTIL_HPP_

/******************************************************************************
* Included header files                                                       *
******************************************************************************/
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <ctime>
#include <cmath>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#if !defined(__APPLE__) && !defined(__MACOSX)
#include <malloc.h>
#endif
#include <math.h>
#include <numeric>
#include <stdint.h>


#if defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR)
#define _aligned_malloc __mingw_aligned_malloc
#define _aligned_free  __mingw_aligned_free
#endif // __MINGW32__  and __MINGW64_VERSION_MAJOR


#ifndef _WIN32
#if defined(__INTEL_COMPILER)
#pragma warning(disable : 1125)
#endif
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#if !defined(__APPLE__) && !defined(__MACOSX)
#include <linux/limits.h>
#else
#include <limits.h>
#endif
#include <unistd.h>
#endif

#include "public/common/TraceAdapter.h"

/******************************************************************************
* Defined macros                                                              *
******************************************************************************/
#define GRAAL_SUCCESS 0
#define GRAAL_FAILURE 1
#define GRAAL_EXPECTED_FAILURE 2
#define GRAAL_NOT_IMPLEMENTED 3
#define GRAAL_OPENCL_FAILURE 4
#define GRAAL_NOT_ENOUGH_GPU_MEM 5

#define GRAAL_VERSION_MAJOR 2
#define GRAAL_VERSION_MINOR 9
#define GRAAL_VERSION_SUB_MINOR 1
#define GRAAL_VERSION_BUILD 1
#define GRAAL_VERSION_REVISION 1

#ifndef _WIN32
#define sscanf_s sscanf
#define sprintf_s sprintf
#endif

#ifdef _DEBUG_PRINT
    AMF_RETURN_IF_FALSE(actual != NULL, GRAAL_FAILURE, L#msg);

#  define CHECK_ERROR(actual, reference, msg) \
    AMF_RETURN_IF_FALSE(actual == reference, GRAAL_FAILURE, L#msg);
#else
#  define CHECK_ALLOCATION(actual, msg)

#  define CHECK_ERROR(actual, reference, msg)
#endif
#define FREE(ptr) \
    { \
        if(ptr != NULL) \
        { \
            free(ptr); \
            ptr = NULL; \
        } \
    }

#ifdef _WIN32
#define ALIGNED_FREE(ptr) \
    { \
        if(ptr != NULL) \
        { \
            _aligned_free(ptr); \
            ptr = NULL; \
        } \
    }
#endif


/******************************************************************************
* namespace boltsdk                                                           *
******************************************************************************/
namespace graal
{

/**
 * graalVersionStr
 * struct to form
 */
static struct graalVersionStr
{
    int major;      /**< GRAAL major release number */
    int minor;      /**< GRAAL minor release number */
    int subminor;   /**< GRAAL sub-minor release number */
    int build;      /**< GRAAL build number */
    int revision;   /**< GRAAL revision number */

    /**
     * Constructor
     */
     inline graalVersionStr()
    {
        major = GRAAL_VERSION_MAJOR;
        minor = GRAAL_VERSION_MINOR;
        subminor = GRAAL_VERSION_SUB_MINOR;
        build = GRAAL_VERSION_BUILD;
        revision = GRAAL_VERSION_REVISION;
    }
} graalVerStr;


/**************************************************************************
* CmdArgsEnum                                                             *
* Enum for datatype of CmdArgs                                            *
**************************************************************************/
enum CmdArgsEnum
{
    CA_ARG_INT,
    CA_ARG_FLOAT,
    CA_ARG_DOUBLE,
    CA_ARG_LONG,
    CA_ARG_STRING,
    CA_NO_ARGUMENT
};

/**************************************************************************
* Option                                                                  *
* struct implements the fields required to map cmd line args to option    *
**************************************************************************/
struct Option
{
    std::string  _sVersion;
    std::string  _lVersion;
    std::string  _description;
    std::string  _usage;
    CmdArgsEnum  _type;
    void *       _value;
};

/**
 * error
 * constant function, Prints error messages
 * @param errorMsg std::string message
 */
static void error(std::string errorMsg)
{
#ifdef _DEBUG_PRINTF
    std::cout<<"Error: "<<errorMsg<<std::endl;
#endif
    AMFTraceError(AMF_FACILITY, L"Error: %s", errorMsg.c_str());
}

/**
 * expectedError
 * constant function, Prints error messages
 * @param errorMsg char* message
 */
static void expectedError(const char* errorMsg)
{
#ifdef _DEBUG_PRINTF
    std::cout<<"Expected Error: "<<errorMsg<<std::endl;
#endif
    AMFTraceError(AMF_FACILITY, L"Expected Error: %s", errorMsg);
}

/**
 * expectedError
 * constant function, Prints error messages
 * @param errorMsg string message
 */
static void expectedError(std::string errorMsg)
{
#ifdef _DEBUG_PRINTF
    std::cout<<"Expected Error: "<<errorMsg<<std::endl;
#endif
    AMFTraceError(AMF_FACILITY, L"Expected Error: %s", errorMsg.c_str());
}


/**
* compare template version
* compare data to check error
* @param refData templated input
* @param data templated input
* @param length number of values to compare
* @param epsilon errorWindow
*/
static bool compare(const float *refData, const float *data,
             const int length, const float epsilon = 1e-6f)
{
    float error = 0.0f;
    float ref = 0.0f;
    for(int i = 1; i < length; ++i)
    {
        float diff = refData[i] - data[i];
        error += diff * diff;
        ref += refData[i] * refData[i];
    }
    float normRef =::sqrtf((float) ref);
    if (::fabs((float) ref) < 1e-7f)
    {
        return false;
    }
    float normError = ::sqrtf((float) error);
    error = normError / normRef;
    return error < epsilon;
}
static bool compare(const double *refData, const double *data,
             const int length, const double epsilon = 1e-6)
{
    double error = 0.0;
    double ref = 0.0;
    for(int i = 1; i < length; ++i)
    {
        double diff = refData[i] - data[i];
        error += diff * diff;
        ref += refData[i] * refData[i];
    }
    double normRef =::sqrt((double) ref);
    if (::fabs((double) ref) < 1e-7)
    {
        return false;
    }
    double normError = ::sqrt((double) error);
    error = normError / normRef;
    return error < epsilon;
}

static bool compare(int *refData, int *data, int length) {

    int i;
    for (i=0;i<length;i++)
    if (refData[i] != data[i])
        return 0;
    return 1;
}

/**
 * strComparei
 * Case insensitive compare of 2 strings
 * returns true when strings match(case insensitive), false otherwise
 */
static bool strComparei(std::string a, std::string b)
{
    int sizeA = (int)a.size();
    if (b.size() != sizeA)
    {
        return false;
    }
    for (int i = 0; i < sizeA; i++)
    {
        if (tolower(a[i]) != tolower(b[i]))
        {
            return false;
        }
    }
    return true;
}

/**
 * toString
 * convert a T type to string
 */
template<typename T>
std::string toString(T t, std::ios_base & (*r)(std::ios_base&) = std::dec)
{
    std::ostringstream output;
    output << r << t;
    return output.str();
}

/**
 * filetoString
 * converts any file into a string
 * @param file string message
 * @param str string message
 * @return 0 on success Positive if expected and Non-zero on failure
 */
static int fileToString(std::string &fileName, std::string &str)
{
    size_t      size;
    char*       buf;
    // Open file stream
    std::fstream f(fileName.c_str(), (std::fstream::in | std::fstream::binary));
    // Check if we have opened file stream
    if (f.is_open())
    {
        size_t  sizeFile;
        // Find the stream size
        f.seekg(0, std::fstream::end);
        size = sizeFile = (size_t)f.tellg();
        f.seekg(0, std::fstream::beg);
        buf = new char[size + 1];
        if (!buf)
        {
            f.close();
            return  GRAAL_FAILURE;
        }
        // Read file
        f.read(buf, sizeFile);
        f.close();
        str[size] = '\0';
        str = buf;
        return GRAAL_SUCCESS;
    }
    else
    {
        error("Converting file to string. Cannot open file.");
        str = "";
        return GRAAL_FAILURE;
    }
}

/**
*******************************************************************
* @fn printArray
* @brief displays a array on std::out
******************************************************************/
template<typename T>
void printArray(
    const std::string header,
    const T * data,
    const int width,
    const int height)
{
#ifdef _DEBUG_PRINTF
    std::cout<<"\n"<<header<<"\n";
    for(int i = 0; i < height; i++)
    {
        for(int j = 0; j < width; j++)
        {
            std::cout<<data[i*width+j]<<" ";
        }
        std::cout<<"\n";
    }
    std::cout<<"\n";
#endif
}

/**
*******************************************************************
* @fn printArray
* @brief displays a array of opencl vector types on std::out
******************************************************************/
template<typename T>
void printArray(
    const std::string header,
    const T * data,
    const int width,
    const int height,
    int veclen)
{
#ifdef _DEBUG_PRINTF
    std::cout<<"\n"<<header<<"\n";
    for(int i = 0; i < height; i++)
    {
        for(int j = 0; j < width; j++)
        {
            std::cout << "(";
            for(int k=0; k<veclen; k++)
            {
                std::cout<<data[i*width+j].s[k]<<", ";
            }
            std::cout << ") ";
        }
        std::cout<<"\n";
    }
    std::cout<<"\n";
#endif
}


/**
*******************************************************************
* @fn printArray overload function
* @brief displays a array on std::out
******************************************************************/
template<typename T>
void printArray(
    const std::string header,
    const std::vector<T>& data,
    const int width,
    const int height)
{
#ifdef _DEBUG_PRINTF
    std::cout<<"\n"<<header<<"\n";
    for(int i = 0; i < height; i++)
    {
        for(int j = 0; j < width; j++)
        {
            std::cout<<data[i*width+j]<<" ";
        }
        std::cout<<"\n";
    }
    std::cout<<"\n";
#endif
}

/**
 * printstats
 * Print the results from the test
 * @param stdStr Parameter
 * @param stats Statistic value of parameter
 * @param n number
 */
static void printStatistics(std::string *statsStr, std::string * stats, int n)
{
#ifdef _DEBUG_PRINTF
    int *columnWidth = new int[n];
    if(columnWidth == NULL)
    {
        return;
    }
    std::cout << std::endl << "|";
    for(int i=0; i<n; i++)
    {
        columnWidth[i] = (int) ((statsStr[i].length() > stats[i].length())?
                                statsStr[i].length() : stats[i].length());
        std::cout << " " << std::setw(columnWidth[i]+1) << std::left << statsStr[i] <<
                  "|";
    }
    std::cout << std::endl << "|";
    for(int i=0; i<n; i++)
    {
        for(int j=0; j<(columnWidth[i]+2); j++)
        {
            std::cout << "-";
        }
        std::cout << "|";
    }
    std::cout << std::endl << "|";
    for(int i=0; i<n; i++)
    {
        std::cout << " " << std::setw(columnWidth[i]+1) << std::left << stats[i] << "|";
    }
    std::cout << std::endl;
    if(columnWidth)
    {
        delete[] columnWidth;
    }
#endif
}

/**
 * fillRandom
 * fill array with random values
 */
template<typename T>
int fillRandom(
    T * arrayPtr,
    const int width,
    const int height,
    const T rangeMin,
    const T rangeMax,
    unsigned int seed=123)
{
    if(!arrayPtr)
    {
        error("Cannot fill array. NULL pointer.");
        return GRAAL_FAILURE;
    }
    if(!seed)
    {
        seed = (unsigned int)time(NULL);
    }
    srand(seed);
    double range = double(rangeMax - rangeMin) + 1.0;
    /* random initialisation of input */
    for(int i = 0; i < height; i++)
        for(int j = 0; j < width; j++)
        {
            int index = i*width + j;
            arrayPtr[index] = rangeMin + T(range*rand()/(RAND_MAX + 1.0));
        }
    return GRAAL_SUCCESS;
}

/**
 * fillPos
 * fill the specified positions
 */
template<typename T>
int fillPos(
    T * arrayPtr,
    const int width,
    const int height)
{
    if(!arrayPtr)
    {
        error("Cannot fill array. NULL pointer.");
        return GRAAL_FAILURE;
    }
    /* initialisation of input with positions*/
    for(T i = 0; i < height; i++)
        for(T j = 0; j < width; j++)
        {
            T index = i*width + j;
            arrayPtr[index] = index;
        }
    return GRAAL_SUCCESS;
}

/**
 * fillConstant
 * fill the array with constant value
 */
template<typename T>
int fillConstant(
    T * arrayPtr,
    const int width,
    const int height,
    const T val)
{
    if(!arrayPtr)
    {
        error("Cannot fill array. NULL pointer.");
        return GRAAL_FAILURE;
    }
    /* initialisation of input with constant value*/
    for(int i = 0; i < height; i++)
        for(int j = 0; j < width; j++)
        {
            int index = i*width + j;
            arrayPtr[index] = val;
        }
    return GRAAL_SUCCESS;
}


/**
 * roundToPowerOf2
 * rounds to a power of 2
 */
template<typename T>
T roundToPowerOf2(T val)
{
    int bytes = sizeof(T);
    val--;
    for(int i = 0; i < bytes; i++)
    {
        val |= val >> (1<<i);
    }
    val++;
    return val;
}

/**
 * isPowerOf2
 * checks if input is a power of 2
 */
template<typename T>
int isPowerOf2(T val)
{
    long long _val = val;
    if((_val & (-_val))-_val == 0 && _val != 0)
    {
        return GRAAL_SUCCESS;
    }
    else
    {
        return GRAAL_FAILURE;
    }
}

/**
        * getPath
        * @return path of the current directory
        */
static std::string getPath()
{
    printf("todo: use utils!\n");

#ifdef _WIN32
    char buffer[MAX_PATH];
#ifdef UNICODE
    if(!GetModuleFileNameA(NULL, (LPSTR)buffer, sizeof(buffer)))
    {
        throw std::string("GetModuleFileName() failed!");
    }
#else
    if(!GetModuleFileName(NULL, buffer, sizeof(buffer)))
    {
        throw std::string("GetModuleFileName() failed!");
    }
#endif
    std::string str(buffer);
    /* '\' == 92 */
    int last = (int)str.find_last_of((char)92);
#else
    char buffer[PATH_MAX + 1];
    ssize_t len;
    if((len = readlink("/proc/self/exe",buffer, sizeof(buffer) - 1)) == -1)
    {
        throw std::string("readlink() failed!");
    }
    buffer[len] = '\0';
    std::string str(buffer);
    /* '/' == 47 */
    int last = (int)str.find_last_of((char)47);
#endif
    return str.substr(0, last + 1);
}


/**
***********************************************************************
* @brief Returns SDK Version string
* @return std::string
**********************************************************************/
static std::string getSdkVerStr()
{
    char str[1024];
    std::string dbgStr("");
    std::string internal("");
#ifdef _WIN32
#ifdef _DEBUG
    dbgStr.append("-dbg");
#endif
#else
#ifdef NDEBUG
    dbgStr.append("-dbg");
#endif
#endif
#if defined (_WIN32) && !defined(__MINGW32__)
    sprintf_s(str, 256, "Graal-v%d.%d.%d%s%s (%d.%d)",
              graalVerStr.major,
              graalVerStr.minor,
              graalVerStr.subminor,
              internal.c_str(),
              dbgStr.c_str(),
              graalVerStr.build,
              graalVerStr.revision);
#else

    //todo: where is sdkVerStr defined?
    #define sdkVerStr graalVerStr

    sprintf(str, "AMD-APP-SDK-v%d.%d.%d%s%s (%d.%d)",
            sdkVerStr.major,
            sdkVerStr.minor,
            sdkVerStr.subminor,
            internal.c_str(),
            dbgStr.c_str(),
            sdkVerStr.build,
            sdkVerStr.revision);
#endif
    return std::string(str);
}

/**
***********************************************************************
* @brief Check if application is 64bit
* @return bool
**********************************************************************/
static bool is64Bit()
{
    return (sizeof(int*) == 8);
}

/**
 * Timer
 * struct to handle time measuring functionality
 */
class GraalTimer
{
    private:

        struct Timer
        {
            std::string name;   /**< name name of time object*/
            long long _freq;    /**< _freq frequency*/
            double _clocks;  /**< _clocks number of ticks at end*/
            double _start;   /**< _start start point ticks*/
        };

        std::vector<Timer*> _timers;      /**< _timers vector to Timer objects */


    public :
        double totalTime;                 /** total time taken */
        /**
         * Constructor
         */
        GraalTimer()
        {
        }

        /**
         * Destructor
         */
        ~GraalTimer()
        {
            while(!_timers.empty())
            {
                Timer *temp = _timers.back();
                _timers.pop_back();
                delete temp;
            }
        }

        /**
        * CreateTimer
        */
        int createTimer()
        {
            Timer* newTimer = new Timer;
            newTimer->_start = 0;
            newTimer->_clocks = 0;
#ifdef _WIN32
            QueryPerformanceFrequency((LARGE_INTEGER*)&newTimer->_freq);
#else
            newTimer->_freq = (long long)1.0E3;
#endif
            /* Push back the address of new Timer instance created */
            _timers.push_back(newTimer);
            return (int)(_timers.size() - 1);
        }

        /**
        * resetTimer
        */
        int resetTimer(int handle)
        {
            if(handle >= (int)_timers.size())
            {
                error("Cannot reset timer. Invalid handle.");
                return -1;
            }
            (_timers[handle]->_start) = 0;
            (_timers[handle]->_clocks) = 0;
            return GRAAL_SUCCESS;
        }
        /**
        * startTimer
        */
        int startTimer(int handle)
        {
            if(handle >= (int)_timers.size())
            {
                error("Cannot reset timer. Invalid handle.");
                return GRAAL_FAILURE;
            }
#ifdef _WIN32
            long long tmpStart;
            QueryPerformanceCounter((LARGE_INTEGER*)&(tmpStart));
            _timers[handle]->_start = (double)tmpStart;
#else
            struct timeval s;
            gettimeofday(&s, 0);
            _timers[handle]->_start = s.tv_sec * 1.0E3 + ((double)(s.tv_usec / 1.0E3));
#endif
            return GRAAL_SUCCESS;
        }

        /**
        * stopTimer
        */
        int stopTimer(int handle)
        {
            double n=0;
            if(handle >= (int)_timers.size())
            {
                error("Cannot reset timer. Invalid handle.");
                return GRAAL_FAILURE;
            }
#ifdef _WIN32
            long long n1;
            QueryPerformanceCounter((LARGE_INTEGER*)&(n1));
            n = (double) n1;
#else
            struct timeval s;
            gettimeofday(&s, 0);
            n = s.tv_sec * 1.0E3+ (double)(s.tv_usec/1.0E3);
#endif
            n -= _timers[handle]->_start;
            _timers[handle]->_start = 0;
            _timers[handle]->_clocks += n;
            return GRAAL_SUCCESS;
        }

        /**
        * readTimer
        */
        double readTimer(int handle)
        {
            if(handle >= (int)_timers.size())
            {
                error("Cannot read timer. Invalid handle.");
                return GRAAL_FAILURE;
            }
            double reading = double(_timers[handle]->_clocks);
            reading = double(reading / _timers[handle]->_freq);
            return reading;
        }


};


#if 0
/**************************************************************************
* SDKCmdArgsParser                                                          *
* class implements Support for Command Line Options for any sample        *
**************************************************************************/
class SDKCmdArgsParser
{
    protected:
        int _numArgs;               /**< number of arguments */
        int _argc;                  /**< number of arguments */
        int _seed;                  /**< seed value */
        char ** _argv;              /**< array of char* holding CmdLine Options */
        Option * _options;          /**< struct option object */
        bool version;                           /**< Cmd Line Option- if version */
        std::string name;                       /**< Name of the Sample */
        sdkVersionStr sdkVerStr;     /**< SDK version string */
    public:
        bool quiet;                 /**< Cmd Line Option- if Quiet */
        bool verify;                /**< Cmd Line Option- if verify */
        bool timing;                /**< Cmd Line Option- if Timing */
        std::string sampleVerStr;   /**< Sample version string */

    protected:

        /**
         * usage
         * Displays the various options available for any sample
         */
        void usage()
        {
            std::cout<<"Usage\n";
            help();
        }

        /**
        *******************************************************************
        * @brief Constructor private and cannot be called directly
        ******************************************************************/
        SDKCmdArgsParser (void)
        {
            _options = NULL;
            _numArgs = 0;
            _argc = 0;
            _argv = NULL;
            _seed = 123;
            quiet = 0;
            verify = 0;
            timing = 0;
        }
    private:

        /**
        *******************************************************************
        * @fn match
        * @param argv array of CMDLine Options
        * @param argc number of CMdLine Options
        * @param totalArgc total number of Original command line Options
        * @param inv_arg pointer to an integer
        * @return GRAAL_SUCCESS on success, GRAAL_FAILURE otherwise
        ******************************************************************/
        int match(char ** argv, int argc, int totalArgc, int *inv_arg)
        {
            int matched = 0;
            int shortVer = true;
            char * arg = *argv;
            if(argc > 1)
            {
                if (*arg == '-' && *(arg + 1) == '-')
                {
                    shortVer = false;
                    arg++;
                }
                else if (*arg != '-')
                {
                    if(argc == totalArgc)
                        *inv_arg = 0;
                    else
                        *inv_arg = 1;
                    return matched;
                }
            }
            else
            {
                if (*arg == '-' && *(arg + 1) == '-')
                {
                    shortVer = false;
                    arg++;
                }
                else if(*arg != '-')
                {
                    *inv_arg = 1;
                    return matched;
                }
            }
            arg++;
            for (int count = 0; count < _numArgs; count++)
            {
                if (shortVer)
                {
                    matched = _options[count]._sVersion.compare(arg) == 0 ? 1 : 0;
                }
                else
                {
                    matched = _options[count]._lVersion.compare(arg) == 0 ? 1 : 0;
                }
                if (matched == 1)
                {
                    int extra_arg = 0;
                    if (_options[count]._type == CA_NO_ARGUMENT)
                    {
                        *((bool *)_options[count]._value) = true;
                        if (argc > 1 && *(*(argv + 1)) != '-')
                        {
#ifdef _DEBUG_PRINTF
                            printf("Invalid Argument for %s\n", argv[0]);
#endif
                            *inv_arg = 1;
                            return GRAAL_FAILURE;
                        }
                    }
                    else if (_options[count]._type == CA_ARG_FLOAT)
                    {

                        if (argc > 1)
                        {
                            if (sscanf_s(*(argv + 1), "%f", (float *)_options[count]._value) != 1)
                            {
#ifdef _DEBUG_PRINTF
                                printf("Error :: argument value for %s is not float\n", argv[0]);
#endif
                                *inv_arg = 1;
                                return GRAAL_FAILURE;
                            }
                            if (argc > 2) {
                                if (*(*(argv + 2)) != '-')
                                {
#ifdef _DEBUG_PRINTF
                                    printf("Invalid argument for %s\n", argv[0]);
#endif
                                    *inv_arg = 1;
                                    return GRAAL_FAILURE;
                                }
                            }
                            matched++;
                        }
                        else
                        {
                            std::cout << "Error. Missing argument for \"" << (*argv) << "\"\n";
                            *inv_arg = 1;
                            return GRAAL_FAILURE;
                        }
                    }
                    else if (_options[count]._type == CA_ARG_DOUBLE)
                    {
                        if (argc > 1)
                        {
                            if (sscanf_s(*(argv + 1), "%lf", (double *)_options[count]._value) != 1)
                            {
#ifdef _DEBUG_PRINTF
                                printf("Error :: argument value for %s is not double\n", argv[0]);
#endif
                                *inv_arg = 1;
                                return GRAAL_FAILURE;
                            }
                            if (argc > 2) {
                                if (*(*(argv + 2)) != '-')
                                {
#ifdef _DEBUG_PRINTF
                                    printf("Invalid argument for %s\n", argv[0]);
#endif
                                    *inv_arg = 1;
                                    return GRAAL_FAILURE;
                                }
                            }
                            matched++;
                        }
                        else
                        {
                            std::cout << "Error. Missing argument for \"" << (*argv) << "\"\n";
                            *inv_arg = 1;
                            return GRAAL_FAILURE;
                        }
                    }
                    else if (_options[count]._type == CA_ARG_INT)
                    {
                        if (argc > 1)
                        {
                            if ((sscanf_s(*(argv + 1), "%d", (int *)_options[count]._value) != 1))
                            {
#ifdef _DEBUG_PRINTF
                                printf("Error :: argument value for %s is not an integer\n", argv[0]);
#endif
                                *inv_arg = 1;
                                return GRAAL_FAILURE;
                            }
                            if (argc > 2) {
                                if (*(*(argv + 2)) != '-')
                                {
#ifdef _DEBUG_PRINTF
                                    printf("Invalid argument for %s\n", argv[0]);
#endif
                                    *inv_arg = 1;
                                    return GRAAL_FAILURE;
                                }
                            }
                            matched++;
                        }
                        else
                        {
                            std::cout << "Error. Missing argument for \"" << (*argv) << "\"\n";
                            *inv_arg = 1;
                            return GRAAL_FAILURE;
                        }
                    }
                    else if (_options[count]._type == CA_ARG_LONG)
                    {
                        if (argc > 1)
                        {
                            if ((sscanf_s(*(argv + 1), "%lu", (size_t *)_options[count]._value) != 1))
                            {
#ifdef _DEBUG_PRINTF
                                printf("Error :: argument value for %s is not of long datatype\n", argv[0]);
#endif
                                *inv_arg = 1;
                                return GRAAL_FAILURE;
                            }
                            if (argc > 2) {
                                if (*(*(argv + 2)) != '-')
                                {
#ifdef _DEBUG_PRINTF
                                    printf("Invalid argument for %s\n", argv[0]);
#endif
                                    *inv_arg = 1;
                                    return GRAAL_FAILURE;
                                }
                            }
                            matched++;
                        }
                        else
                        {
                            std::cout << "Error. Missing argument for \"" << (*argv) << "\"\n";
                            *inv_arg = 1;
                            return GRAAL_FAILURE;
                        }
                    }
                    else
                    {
                        if (argc > 1)
                        {
                            std::string * str = (std::string *)_options[count]._value;
                            str->clear();
                            str->append(*(argv + 1));
                            matched++;
                        }
                        else
                        {
                            std::cout << "Error. Missing argument for \"" << (*argv) << "\"\n";
                            *inv_arg = 1;
                            return GRAAL_FAILURE;
                        }
                    }
                    *inv_arg = 0;
                    break;
                }
            }
            *inv_arg = 0;
            return matched;
        }

    public:

        /**
        *******************************************************************
        * @brief Destructor of SDKCmdArgsParser
        ******************************************************************/
        ~SDKCmdArgsParser ()
        {
            if(_options)
            {
                delete[] _options;
            }
        }
        /**
        *******************************************************************
        * @fn AddOption
        * @brief Function to add a new CmdLine Option
        * @param op Option object
        * @return GRAAL_SUCCESS on success, GRAAL_FAILURE otherwise
        ******************************************************************/
        int AddOption(Option* op)
        {
            if(!op)
            {
                std::cout<<"Error. Cannot add option, NULL input";
                return -1;
            }
            else
            {
                Option* save = NULL;
                if(_options != NULL)
                {
                    save = _options;
                }
                _options = new Option[_numArgs + 1];
                if(!_options)
                {
                    std::cout<<"Error. Cannot add option ";
                    std::cout<<op->_sVersion<<". Memory allocation error\n";
                    return GRAAL_FAILURE;
                }
                if(_options != NULL)
                {
                    for(int i=0; i< _numArgs; ++i)
                    {
                        _options[i] = save[i];
                    }
                }
                _options[_numArgs] = *op;
                _numArgs++;
                delete []save;
            }
            return GRAAL_SUCCESS;
        }

        /**
        *******************************************************************
        * @fn DeleteOption
        * @brief Function to a delete CmdLine Option
        * @param op Option object
        * @return GRAAL_SUCCESS on success, GRAAL_FAILURE otherwise
        ******************************************************************/
        int DeleteOption(Option* op)
        {
            if(!op || _numArgs <= 0)
            {
                std::cout<<"Error. Cannot delete option."
                         <<"Null pointer or empty option list\n";
                return GRAAL_FAILURE;
            }
            for(int i = 0; i < _numArgs; i++)
            {
                if(op->_sVersion == _options[i]._sVersion ||
                        op->_lVersion == _options[i]._lVersion )
                {
                    for(int j = i; j < _numArgs-1; j++)
                    {
                        _options[j] = _options[j+1];
                    }
                    _numArgs--;
                }
            }
            return GRAAL_SUCCESS;
        }

        /**
        *******************************************************************
        * @fn parse
        * @brief Function to parse the Cmdline Options
        * @param argv array of strings of CmdLine Options
        * @param argc Number of CmdLine Options
        * @return 0 on success Positive if expected and Non-zero on failure
        ******************************************************************/
        int parse(char ** p_argv, int p_argc)
        {
            int matched = 0;
            int argc, totalArgc;
            char **argv;
            int ret = 0;
            _argc = p_argc;
            _argv = p_argv;
            totalArgc = argc = p_argc;
            argv = p_argv;
            if (argc == 1)
            {
                return GRAAL_FAILURE;
            }
            while (argc > 0)
            {
                int inv_arg = 0;
                matched = match(argv, argc, totalArgc, &inv_arg);
                argc -= (matched > 0 ? matched : 1);
                argv += (matched > 0 ? matched : 1);
                if (inv_arg)
                {
                    return 0;
                }

            }
            return matched;
        }

        /**
        *******************************************************************
        * @fn isArgSet
        * @brief Function to check if a argument is set
        * @param arg option string
        * @param shortVer if the option is short version. (Opt param default=false)
        * @return true if success else false
        ******************************************************************/
        bool isArgSet(std::string str, bool shortVer = false)
        {
            bool enabled = false;
            for (int count = 0; count < _argc; count++)
            {
                char * arg = _argv[count];
                if (*arg != '-')
                {
                    continue;
                }
                else if (*arg == '-' && *(arg+1) == '-' && !shortVer)
                {
                    arg++;
                }
                arg++;
                if (str.compare(arg) == 0)
                {
                    enabled = true;
                    break;
                }
            }
            return enabled;
        }

        /**
        *******************************************************************
        * @fn help
        * @brief Function to write help text
        ******************************************************************/
        void help(void)
        {
            std::cout <<  std::left << std::setw(6) << "-h, " << std::left << std::setw(16)
                      << "" "--help" << std::left << std::setw(34) <<" "
                      << "Display this information\n";
            for (int count = 0; count < _numArgs; count++)
            {
                if (_options[count]._sVersion.length() > 0)
                {
                    std::cout<< std::left << std::setw(6) <<  "-" + _options[count]._sVersion +
                             ", ";
                }
                else
                {
                    std::cout << "      ";
                }
                std::cout<< std::left<< std::setw(16) <<  "--" + _options[count]._lVersion;
                if(!_options[count]._usage.empty())
                {
                    std::cout <<std::left <<std::setw(34) << _options[count]._usage ;
                }
                else
                {
                    std::cout <<std::left <<std::setw(34) << "";
                }
                std::cout<< _options[count]._description + "\n";
            }
        }
        /**
        * parseCommandLine
        * parses the command line options given by user
        * @param argc Number of elements in cmd line input
        * @param argv array of char* storing the CmdLine Options
        * @return 0 on success Positive if expected and Non-zero on failure
        */
        virtual int parseCommandLine(int argc, char **argv) = 0;
};


#endif


}





#endif
