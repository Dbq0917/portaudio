#include <stdio.h>
#include <sys/resource.h>
#include <time.h>

#include "portaudio.h"

static double NowSeconds( void )
{
    struct timespec ts;
    clock_gettime( CLOCK_MONOTONIC, &ts );
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

int main( void )
{
    const int iterations = 100000;
    struct rusage usage;
    PaError result;
    volatile unsigned long checksum = 0;
    double start;
    double afterInit;
    double afterWarm;
    double afterRefresh;
    int deviceCount;
    int i;

    start = NowSeconds();
    result = Pa_Initialize();
    afterInit = NowSeconds();
    if( result != paNoError )
    {
        fprintf( stderr, "Pa_Initialize failed: %d (%s)\n",
                result, Pa_GetErrorText( result ) );
        return 1;
    }

    deviceCount = Pa_GetDeviceCount();
    if( deviceCount < 0 )
    {
        fprintf( stderr, "Pa_GetDeviceCount failed: %d (%s)\n",
                deviceCount, Pa_GetErrorText( deviceCount ) );
        Pa_Terminate();
        return 1;
    }

    for( i = 0; i < iterations; ++i )
    {
        const PaDeviceInfo *info =
                deviceCount > 0 ? Pa_GetDeviceInfo( i % deviceCount ) : NULL;
        checksum += (unsigned long)Pa_GetDeviceCount();
        if( info )
            checksum += (unsigned long)info->maxInputChannels;
    }
    afterWarm = NowSeconds();

    result = Pa_RefreshDeviceList();
    afterRefresh = NowSeconds();
    if( result != paNoError )
    {
        fprintf( stderr, "Pa_RefreshDeviceList failed: %d (%s)\n",
                result, Pa_GetErrorText( result ) );
        Pa_Terminate();
        return 1;
    }

    getrusage( RUSAGE_SELF, &usage );
    printf( "devices=%d last_enum_error=%d self_check=%d init_ms=%.3f "
            "warm_pair_us=%.3f refresh_ms=%.3f maxrss_kb=%ld checksum=%lu\n",
            deviceCount,
            Pa_GetLastDeviceEnumerationError(),
            Pa_RunDeviceEnumerationSelfCheck(),
            ( afterInit - start ) * 1000.0,
            ( afterWarm - afterInit ) * 1000000.0 / iterations,
            ( afterRefresh - afterWarm ) * 1000.0,
            (long)usage.ru_maxrss,
            checksum );

    Pa_Terminate();
    return 0;
}
