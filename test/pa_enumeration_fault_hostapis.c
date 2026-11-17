#include <string.h>

#include "pa_hostapi.h"
#include "pa_util.h"

static int failAll_;
static int hotplugPresent_ = 1;
static int successfulInitCount_;

static PaDeviceInfo stableDevices_[2];
static PaDeviceInfo hotplugDevice_;
static PaDeviceInfo *stableDeviceInfos_[2];
static PaDeviceInfo *hotplugDeviceInfos_[1];
static PaUtilHostApiRepresentation stableHostApi_;
static PaUtilHostApiRepresentation malformedHostApi_;
static PaUtilHostApiRepresentation hotplugHostApi_;

static PaError DummyOpenStream( struct PaUtilHostApiRepresentation *hostApi,
        PaStream **stream, const PaStreamParameters *inputParameters,
        const PaStreamParameters *outputParameters, double sampleRate,
        unsigned long framesPerBuffer, PaStreamFlags streamFlags,
        PaStreamCallback *streamCallback, void *userData )
{
    (void)hostApi;
    (void)stream;
    (void)inputParameters;
    (void)outputParameters;
    (void)sampleRate;
    (void)framesPerBuffer;
    (void)streamFlags;
    (void)streamCallback;
    (void)userData;
    return paDeviceUnavailable;
}

static PaError DummyIsFormatSupported(
        struct PaUtilHostApiRepresentation *hostApi,
        const PaStreamParameters *inputParameters,
        const PaStreamParameters *outputParameters, double sampleRate )
{
    (void)hostApi;
    (void)inputParameters;
    (void)outputParameters;
    (void)sampleRate;
    return paFormatIsSupported;
}

static void DummyTerminate( struct PaUtilHostApiRepresentation *hostApi )
{
    (void)hostApi;
}

static void InitializeCommonHostApi( PaUtilHostApiRepresentation *hostApi,
        const char *name, PaDeviceInfo **deviceInfos, int deviceCount,
        PaHostApiIndex hostApiIndex )
{
    memset( hostApi, 0, sizeof(*hostApi) );
    hostApi->info.structVersion = 1;
    hostApi->info.type = paInDevelopment;
    hostApi->info.name = name;
    hostApi->info.deviceCount = deviceCount;
    hostApi->info.defaultInputDevice =
            deviceCount > 0 ? 0 : paNoDevice;
    hostApi->info.defaultOutputDevice =
            deviceCount > 0 ? 0 : paNoDevice;
    hostApi->deviceInfos = deviceInfos;
    hostApi->Terminate = DummyTerminate;
    hostApi->OpenStream = DummyOpenStream;
    hostApi->IsFormatSupported = DummyIsFormatSupported;
    (void)hostApiIndex;
}

static void InitializeFakeDevice( PaDeviceInfo *deviceInfo, const char *name,
        PaHostApiIndex hostApiIndex )
{
    memset( deviceInfo, 0, sizeof(*deviceInfo) );
    deviceInfo->structVersion = 2;
    deviceInfo->name = name;
    deviceInfo->hostApi = hostApiIndex;
    deviceInfo->maxInputChannels = 1;
    deviceInfo->maxOutputChannels = 1;
    deviceInfo->defaultSampleRate = 48000.0;
}

static PaError InitializeStable( PaUtilHostApiRepresentation **hostApi,
        PaHostApiIndex hostApiIndex )
{
    if( failAll_ )
    {
        *hostApi = NULL;
        return paInternalError;
    }

    InitializeFakeDevice( &stableDevices_[0], "stable-a", hostApiIndex );
    InitializeFakeDevice( &stableDevices_[1], "stable-b", hostApiIndex );
    stableDeviceInfos_[0] = &stableDevices_[0];
    stableDeviceInfos_[1] = &stableDevices_[1];
    InitializeCommonHostApi( &stableHostApi_, "Stable API", stableDeviceInfos_,
            2, hostApiIndex );
    *hostApi = &stableHostApi_;
    ++successfulInitCount_;
    return paNoError;
}

static PaError InitializeFailing( PaUtilHostApiRepresentation **hostApi,
        PaHostApiIndex hostApiIndex )
{
    (void)hostApiIndex;
    *hostApi = NULL;
    return paUnanticipatedHostError;
}

static PaError InitializeMalformed( PaUtilHostApiRepresentation **hostApi,
        PaHostApiIndex hostApiIndex )
{
    if( failAll_ )
    {
        *hostApi = NULL;
        return paInternalError;
    }

    InitializeCommonHostApi( &malformedHostApi_, "Malformed API", NULL, 1,
            hostApiIndex );
    malformedHostApi_.Terminate = NULL;
    *hostApi = &malformedHostApi_;
    return paNoError;
}

static PaError InitializeHotplug( PaUtilHostApiRepresentation **hostApi,
        PaHostApiIndex hostApiIndex )
{
    int deviceCount = hotplugPresent_ ? 1 : 0;

    if( failAll_ )
    {
        *hostApi = NULL;
        return paInternalError;
    }

    InitializeFakeDevice( &hotplugDevice_, "hotplug-a", hostApiIndex );
    hotplugDeviceInfos_[0] = &hotplugDevice_;
    InitializeCommonHostApi( &hotplugHostApi_, "Hotplug API",
            hotplugDeviceInfos_, deviceCount, hostApiIndex );
    *hostApi = &hotplugHostApi_;
    ++successfulInitCount_;
    return paNoError;
}

PaUtilHostApiInitializer *paHostApiInitializers[] =
{
    InitializeStable,
    InitializeFailing,
    InitializeMalformed,
    InitializeHotplug,
    0
};

void FakeHostApis_SetHotplugPresent( int present )
{
    hotplugPresent_ = present;
}

void FakeHostApis_SetFailAll( int failAll )
{
    failAll_ = failAll;
}

void FakeHostApis_ResetSuccessfulInitCount( void )
{
    successfulInitCount_ = 0;
}

int FakeHostApis_GetSuccessfulInitCount( void )
{
    return successfulInitCount_;
}
