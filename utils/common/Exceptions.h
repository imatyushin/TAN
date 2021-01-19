//
// Copyright (c) 2016 Advanced Micro Devices, Inc. All rights reserved.
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
#pragma once

#include <exception>
#include <string>

#define TAN_EXCEPTION_NOT_IMPLEMENTED "Not implemented"

#define THROW_NOT_IMPLEMENTED throw TanException(TAN_EXCEPTION_NOT_IMPLEMENTED, __FILE__, __LINE__)

struct TanException:
    public std::exception
{
    //std::string     Message;
    //std::string     File;
    //uint32_t        Line;
    std::string     What;

    TanException(
        const std::string & message,
        const std::string & file,
        int line
        ):
        //Message(message),
        //File(file),
        //Line(line),
        What(message + " at " + file + " " + std::to_string(line))
    {
    }

    char const * what() const noexcept override
    {
        return What.c_str();
    }
};