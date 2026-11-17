#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "portaudio.h"

void FakeHostApis_SetHotplugPresent( int present );
void FakeHostApis_SetFailAll( int failAll );
void FakeHostApis_ResetSuccessfulInitCount( void );
int FakeHostApis_GetSuccessfulInitCount( void );

static int failures_;

static void Check( int condition, const char *message )
{
    if( !condition )
    {
        fprintf( stderr, "FAIL: %s\n", message );
        ++failures_;
    }
}

static void CheckDeviceName( int index, const char *expected )
{
    const PaDeviceInfo *info = Pa_GetDeviceInfo( index );

    Check( info != NULL, "device info must be non-null" );
    if( info )
        Check( strcmp( info->name, expected ) == 0, expected );
}

int main( void )
{
    PaError result;
    int initCount;

    setenv( "PA_DEVICE_ENUMERATION_LOG", "0", 1 );
    setenv( "PA_DEVICE_CACHE_TTL_MS", "60000", 1 );

    result = Pa_Initialize();
    Check( result == paNoError, "Pa_Initialize must isolate a failing backend" );
    Check( Pa_RunDeviceEnumerationSelfCheck() == paNoError,
            "startup self-check must pass" );
    Check( Pa_GetLastDeviceEnumerationError() < 0,
            "partial backend failure must be reported" );
    Check( Pa_GetHostApiCount() == 2,
            "only valid host APIs must be published" );
    Check( Pa_GetDeviceCount() == 3,
            "valid devices from healthy backends must be retained" );

    CheckDeviceName( 0, "stable-a" );
    CheckDeviceName( 1, "stable-b" );
    CheckDeviceName( 2, "hotplug-a" );
    Check( Pa_GetDeviceInfo( -1 ) == NULL, "negative index must return NULL" );
    Check( Pa_GetDeviceInfo( 3 ) == NULL, "past-end index must return NULL" );

    FakeHostApis_ResetSuccessfulInitCount();
    Check( Pa_GetDeviceCount() == 3, "cached count must remain stable" );
    Check( Pa_GetDeviceCount() == 3, "cached count must remain stable" );
    Check( FakeHostApis_GetSuccessfulInitCount() == 0,
            "repeated enumeration must use the cache" );

    FakeHostApis_SetHotplugPresent( 0 );
    Check( Pa_InvalidateDeviceCache() == paNoError,
            "cache invalidation must succeed" );
    Check( Pa_GetDeviceCount() == 2,
            "invalidated cache must observe device removal" );
    CheckDeviceName( 0, "stable-a" );
    CheckDeviceName( 1, "stable-b" );

    FakeHostApis_SetHotplugPresent( 1 );
    Check( Pa_RefreshDeviceList() == paNoError,
            "explicit refresh must observe device insertion" );
    Check( Pa_GetDeviceCount() == 3,
            "refresh must restore the hotplugged device" );
    CheckDeviceName( 2, "hotplug-a" );
    Check( Pa_RunDeviceEnumerationSelfCheck() == paNoError,
            "post-refresh self-check must pass" );

    initCount = FakeHostApis_GetSuccessfulInitCount();
    FakeHostApis_SetFailAll( 1 );
    Check( Pa_RefreshDeviceList() == paDeviceEnumerationFailed,
            "all-backend failure must return an explicit error" );
    FakeHostApis_SetFailAll( 0 );
    Check( Pa_InvalidateDeviceCache() == paNoError,
            "recovery invalidation must succeed" );
    Check( Pa_GetDeviceCount() == 3,
            "recovery after failed refresh must preserve the device list" );
    Check( FakeHostApis_GetSuccessfulInitCount() > initCount,
            "recovery must re-run healthy backend enumeration" );

    Check( Pa_Terminate() == paNoError, "Pa_Terminate must succeed" );
    Check( Pa_GetDeviceCount() == paNotInitialized,
            "enumeration after terminate must return paNotInitialized" );

    if( failures_ == 0 )
        printf( "PASS: device enumeration isolation, ordering, cache and hotplug\n" );
    return failures_ == 0 ? 0 : 1;
}
