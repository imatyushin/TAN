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
#include "fifo.h"
#include <cassert>
#include <memory.h>
#include <algorithm>

//#include <errno.h>

// #define BE_THREAD_SAFE

unsigned int FifoBuffer::fifoLength()
{
    if(m_Head >= m_Tail){
        return (m_Head - m_Tail);
    } else {
        return m_Head + m_BufferSize - m_Tail;
    }
}

unsigned int FifoBuffer::roomLeft()
{
    int len = m_BufferSize - fifoLength();
    return len;
}

FifoBuffer::FifoBuffer(int maxSize){
#ifdef BE_THREAD_SAFE
    CAutoLock autoLock(&m_Lock);
#endif

    m_Buffer = new char[maxSize];
    if(m_Buffer){
        m_BufferSize = maxSize;
    } else {
        m_BufferSize = 0;
    }

    m_Tail = 0;
    m_Head = 0;
    return;
}

FifoBuffer::~FifoBuffer()
{
 #ifdef BE_THREAD_SAFE
    CAutoLock autoLock(&m_Lock);
#endif

    if(m_Buffer) delete m_Buffer;
}

bool FifoBuffer::store(char *inputData, unsigned int length)
{
#ifdef BE_THREAD_SAFE
    CAutoLock autoLock(&m_Lock);
#endif

    if(length >= roomLeft()){
        return false;
    }

    unsigned int l2End = m_BufferSize - m_Head;
    if(length > l2End){
        memcpy(&m_Buffer[m_Head],inputData,l2End);
        inputData += l2End;
        length -= l2End;
        m_Head = 0;
    }
    memcpy(&m_Buffer[m_Head],inputData,length);
    m_Head += length;

    //m_Lock.Unlock();
    return true;
}

bool FifoBuffer::retrieve(char *outputData, unsigned int length)
{
#ifdef BE_THREAD_SAFE
    CAutoLock autoLock(&m_Lock);
#endif

    if(length > fifoLength()){
        return false;
    }

    unsigned int l2End = m_BufferSize - m_Tail;
    if(length > l2End){
        memcpy(outputData,&m_Buffer[m_Tail],l2End);
        outputData += l2End;
        length -= l2End;
        m_Tail = 0;
    }
    memcpy(outputData,&m_Buffer[m_Tail],length);
    m_Tail += length;

    return true;
}

// to be called only right after retrieve:
bool FifoBuffer::putBack(unsigned int n)
{
    if(n > m_BufferSize || n < 0)
        return false;

    if(m_Tail < n){
        m_Tail += m_BufferSize;
    }
    m_Tail -= n;
    return true;
}


int FifoBuffer::retrieveAll(char *outputData)
{
#ifdef BE_THREAD_SAFE
    CAutoLock autoLock(&m_Lock);
#endif

    int len = fifoLength();
    retrieve(outputData, len);
    return len;
}


// APIs for reduced copying:
int FifoBuffer::getNextEmptySeg(char **pSeg)
{
#ifdef BE_THREAD_SAFE
    CAutoLock autoLock(&m_Lock);
#endif

    unsigned int maxLen = roomLeft() - 1;
    unsigned int l2End = m_BufferSize - m_Head;
    if(l2End < maxLen)
        maxLen = l2End;
    *pSeg = &m_Buffer[m_Head];
    return(maxLen);
}

bool FifoBuffer::storeSeg(unsigned int length)
{
#ifdef BE_THREAD_SAFE
    CAutoLock autoLock(&m_Lock);
#endif

    unsigned int maxLen = roomLeft();
    unsigned int l2End = m_BufferSize - m_Head;

    if(l2End < maxLen){
        maxLen = l2End;
    }

    if(length > maxLen)
        return false;

    m_Head += length;

    if(m_Head >= m_BufferSize)
        m_Head = 0;

    return(true);
}


int FifoBuffer::getNextFullSeg(char **pSeg)
{
    unsigned int maxLen = fifoLength();
    unsigned int l2End = m_BufferSize - m_Tail;
    if(l2End < maxLen)
        maxLen = l2End;
    *pSeg = &m_Buffer[m_Tail];
    return(maxLen);
}

bool FifoBuffer::retrieveSeg(unsigned int length)
{
#ifdef BE_THREAD_SAFE
    CAutoLock autoLock(&m_Lock);
#endif

    unsigned int maxLen = fifoLength();
    unsigned int l2End = m_BufferSize - m_Tail;
    if(l2End < maxLen)
        maxLen = l2End;

    if(length > maxLen)
        return false;

    m_Tail += length;

    if(m_Tail >= m_BufferSize)
        m_Tail = 0;

    return true;
}

void FifoBuffer::flush(){
    m_Tail = 0;
    m_Head = 0;
}

/**/
std::mutex gLockMutex;

void Fifo::Reset(size_t newSize)
{
    std::lock_guard<std::mutex> lock(gLockMutex);
    
    mBuffer.resize(newSize);
    
    mQueueSize = 0;
    
    //mBufferInPosition.store(0);
    mBufferInPosition = 0;
    //mBufferOutPosition.store(0);
    mBufferOutPosition = 0;
}

size_t Fifo::GetQueueSize() const 
{
    std::lock_guard<std::mutex> lock(gLockMutex);
    
    return mQueueSize;
}
/**/

uint32_t Fifo::Write(const uint8_t *data, size_t size)
{
    //auto bufferDataSize(mQueueSize.load());
    //auto bufferInPosition(mBufferInPosition.load());
    //auto bufferOutPosition(mBufferOutPosition.load());

    std::lock_guard<std::mutex> lock(gLockMutex);
    auto bufferDataSize(mQueueSize/*.load()*/);
    auto bufferInPosition(mBufferInPosition/*.load()*/);
    auto bufferOutPosition(mBufferOutPosition/*.load()*/);

    //free size from bufferInPosition and higher
    auto tailBlock(
        bufferInPosition == bufferOutPosition
            ? (bufferDataSize == mBuffer.size() ? 0 : mBuffer.size() - bufferInPosition)
            :
            (
                bufferInPosition > bufferOutPosition
                    ? (mBuffer.size() - bufferInPosition)
                    : bufferOutPosition - bufferInPosition
            )
        );
    auto headBlock(
        bufferInPosition > bufferOutPosition
            ? bufferOutPosition
            : 0
        );

    auto sizeWritten(0);

    if(tailBlock)
    {
        auto size2Write(std::min(size_t(size), tailBlock));

        std::memcpy(&mBuffer.front() + bufferInPosition, data, size2Write);

        size -= size2Write;
        sizeWritten += size2Write;
        bufferInPosition += size2Write;
    }

    //some free space exists at front
    if(bufferInPosition == mBuffer.size() && headBlock)
    {
        bufferInPosition = 0;
    }

    if(size && headBlock)
    {
        auto size2Write(std::min(size_t(size), headBlock));

        if(bufferDataSize + sizeWritten + size2Write > mBuffer.size())
        {
            //assert(false);
        }

        std::memcpy(&mBuffer.front(), data, size2Write);

        size -= size2Write;
        sizeWritten += size2Write;
        bufferInPosition += size2Write;
    }

    if(bufferInPosition == mBuffer.size())
    {
        bufferInPosition = 0;
    }

    //mBufferInPosition.store(bufferInPosition);
    mBufferInPosition = bufferInPosition;
    
    //if(mQueueSize.load() + sizeWritten > mBuffer.size())
    //if(mQueueSize + sizeWritten > mBuffer.size())
    //{
        //assert(false);
    //}

    mQueueSize += sizeWritten;

    return sizeWritten;
}

uint32_t Fifo::Read(uint8_t *outputBuffer, size_t size2Fill)
{
    //auto bufferDataSize(mQueueSize.load());
    //auto bufferInPosition(mBufferInPosition.load());
    //auto bufferOutPosition(mBufferOutPosition.load());

    std::lock_guard<std::mutex> lock(gLockMutex);
    auto bufferDataSize(mQueueSize);
    auto bufferInPosition(mBufferInPosition);
    auto bufferOutPosition(mBufferOutPosition);

    auto tailSize(
        bufferInPosition == bufferOutPosition
            ? (bufferDataSize ? mBuffer.size() - bufferInPosition : 0)
            :
            (
                bufferInPosition > bufferOutPosition
                    ? bufferInPosition - bufferOutPosition
                    //: (bufferOutPosition > 0 ? mBuffer.size() - bufferOutPosition : 0)
                    : (bufferInPosition < bufferOutPosition ? mBuffer.size() - bufferOutPosition : 0)
            )
        );
    auto headSize(
        bufferInPosition < bufferOutPosition
            ? bufferInPosition
            : 0
        );

    auto sizeFilled(0);

    if(tailSize)
    {
        auto fillTailSize(std::min(tailSize, size2Fill));

        if(fillTailSize > mQueueSize)
        {
            assert(false);
        }

        std::memcpy(outputBuffer, &mBuffer.front() + bufferOutPosition, fillTailSize);

        sizeFilled += fillTailSize;
        size2Fill -= fillTailSize;
        bufferOutPosition += fillTailSize;
    }

    //tail eaten, data exists at a head
    if(bufferOutPosition == mBuffer.size() && (bufferInPosition < bufferOutPosition))
    {
        bufferOutPosition = 0;
    }

    if(size2Fill && headSize)
    {
        auto fillHeadSize(std::min(headSize, size2Fill));

        if(sizeFilled + fillHeadSize > mQueueSize)
        {
            assert(false);
        }

        std::memcpy(outputBuffer + sizeFilled, &mBuffer.front(), fillHeadSize);

        sizeFilled += fillHeadSize;
        size2Fill -= fillHeadSize;
        bufferOutPosition += fillHeadSize;

        /*if(bufferOutPosition == mBuffer.size())
        {
            bufferOutPosition = 0;
        }*/
    }

    //returns 2 head
    /*if(bufferOutPosition == mBuffer.size() && (bufferInPosition < bufferOutPosition))
    {
        bufferOutPosition = 0;
    }*/

    //mBufferOutPosition.store(bufferOutPosition);
    mBufferOutPosition = bufferOutPosition;

    mQueueSize -= sizeFilled;

    return sizeFilled;
}