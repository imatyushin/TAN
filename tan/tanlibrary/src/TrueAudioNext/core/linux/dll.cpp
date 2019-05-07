
#include "tanlibrary/include/TrueAudioNext.h"   //TAN
#include "public/include/core/Platform.h"       //AMF
#include "public/common/AMFFactory.h"           //AMF

__attribute__((constructor))
void InitializeLibrary()
{
}

__attribute__((destructor))
void DestroyLibrary()
{
    AMF_RESULT res = g_AMFFactory.Terminate();
    if (res != AMF_OK)
    {
        wprintf(L"AMF Factory Failed to terminate");
    }
}