#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"

#include "sysapi.h"
#include "sysapi_externs.h"

const char * sysapi_processor_flags_raw( void ) {
    sysapi_internal_reconfig();
    
    if( _sysapi_processor_flags_raw != NULL ) {
        return _sysapi_processor_flags_raw;
    }
    
    /* You can adapt this to ncpus.cpp's _SysapiProceCpuinfo for debugging. */
    FILE * fp = safe_fopen_wrapper_follow( "/proc/cpuinfo", "r", 0644 );
    dprintf( D_LOAD, "Reading from /proc/cpuinfo\n" );
    if( fp ) {
        /* See comment in npcus.cpp for an explanation of this constant. */
        char buffer[1024];
        while( fgets( buffer, sizeof( buffer ) - sizeof(char), fp ) ) {
            char * colon = strchr( buffer, ':' );
            
            char * value = NULL;
            char * attribute = NULL;
            if( colon != NULL ) {
                if( *(colon + 1) != '\0' ) {
                    value = colon + 2;
                }
                
                char * tmp = colon;
                while( isspace( *tmp ) || (*tmp == ':' ) ) {
                    *tmp = '\0';
                    --tmp;
                }
                attribute = buffer;
                
                if( strcmp( attribute, "flags" ) == 0 ) {
                    /* This is where we assume flags into buffer. */
                    _sysapi_processor_flags_raw = strdup( value );
                    if( _sysapi_processor_flags_raw == NULL ) {
                        EXCEPT( "Failed to allocate memory for the raw processor flags." );
                    }
                    break;
                }
            }
        }
        
        fclose( fp );
    } else {
        _sysapi_processor_flags_raw = "";
    }
    
    return _sysapi_processor_flags_raw;
}

const char * sysapi_processor_flags( void ) {
    sysapi_internal_reconfig();
    
    if( _sysapi_processor_flags != NULL ) {
        return _sysapi_processor_flags;
    }
    
    if( _sysapi_processor_flags_raw == NULL ) {
        sysapi_processor_flags_raw();
        ASSERT(_sysapi_processor_flags_raw != NULL);
    }

    /* Which flags do we care about?  You MUST terminate this list with NULL. */
    static const char * const flagNames[] = { "ssse3", "sse4_1", "sse4_2", NULL };

    /* Do some memory-allocation math. */
    int numFlags = 0;
    int maxFlagLength = 0;
    for( int i = 0; flagNames[i] != NULL; ++i ) {
        ++numFlags;
        int curFlagLength = strlen( flagNames[i] );
        if( curFlagLength > maxFlagLength ) { maxFlagLength = curFlagLength; }
    }

    char * currentFlag = (char *)malloc( maxFlagLength * sizeof( char ) );
    if( currentFlag == NULL ) {
        EXCEPT( "Failed to allocate memory for current processor flag." );
    }        
    currentFlag[0] = '\0';

    /* If we track which flags we have, we can make sure the order we
       print them is the same regardless of the raw flags order. */
    const char ** flags = (const char **)malloc( sizeof( const  char * ) * numFlags );
    if( flags == NULL ) {
        EXCEPT( "Failed to allocate memory for processor flags." );
    }
    for( int i = 0; i < numFlags; ++i ) { flags[i] = ""; }

    const char * flagStart = _sysapi_processor_flags_raw;
    const char * flagEnd = _sysapi_processor_flags_raw;
    while( * flagStart != '\0' ) {
        if( * flagStart == ' ' ) { ++flagStart; continue; }

        for( flagEnd = flagStart; (* flagEnd != '\0') && (* flagEnd != ' '); ++flagEnd ) { ; }

        int flagSize = (flagEnd - flagStart) / sizeof( char );
        if( flagSize > maxFlagLength ) {
            flagStart = flagEnd;
            continue;
        }
        
        /* We know that flagStart is neither ' ' nor '\0', so we must have
           at least one character.  Because we only care about flags of a
           certain length or smaller, we know we won't overflow our buffer. */
        strncpy( currentFlag, flagStart, flagSize );
        currentFlag[flagSize] = '\0';

        for( int i = 0; flagNames[i] != NULL; ++i ) {
            if( strcmp( currentFlag, flagNames[i] ) == 0 ) {
                /* Add to the flags. */
                flags[i] = flagNames[i];            
                break;
            }
        }

        flagStart = flagEnd;
    }
    free( currentFlag );

    /* How much space do we need? */
    int flagsLength = 1;
    for( int i = 0; i < numFlags; ++i ) {
        int length = strlen( flags[i] );
        if( length ) { flagsLength += length + 1; }
    }
    
    if( flagsLength == 1 ) {
        _sysapi_processor_flags = "none";
    } else {
        char * processor_flags = (char *)malloc( sizeof( char ) * flagsLength );
        if( processor_flags == NULL ) {
            EXCEPT( "Failed to allocate memory for processor flag list." );
        }
        processor_flags[0] = '\0';

        /* This way, the flags will always print out in the same order. */
        for( int i = 0; i < numFlags; ++i ) {
            if( strlen( flags[i] ) ) {
                strcat( processor_flags, flags[i] );
                strcat( processor_flags, " " );
            }                    
        }

        /* Remove the trailing space. */
        processor_flags[ flagsLength - 2 ] = '\0';
        _sysapi_processor_flags = processor_flags;
    }
    
    free( flags );
    return _sysapi_processor_flags;
}

// ----------------------------------------------------------------------------

#if defined( PROCESSOR_FLAGS_TESTING )

/* 
 * To compile:

g++ -DGLIBC=GLIBC -DHAVE_CONFIG_H -DLINUX -DPROCESSOR_FLAGS_TESTING \
    -I../condor_includes -I../condor_utils -I../safefile \
    -I<configured build directory>/src/condor_includes \
    processor_flags.cpp

 *
 */

// Required to link.
char * _sysapi_processor_flags = NULL;
char * _sysapi_processor_flags_raw = NULL;
void sysapi_internal_reconfig( void ) { ; }
int _EXCEPT_Line;
int _EXCEPT_Errno;
const char * _EXCEPT_File;
void _EXCEPT_ ( const char * fmt, ... ) { exit( 1 ); }

int main( int argc, char ** argv ) {
    const char * expected;
    
    expected = "sse4_1 ssse3";
    _sysapi_processor_flags = NULL;
    _sysapi_processor_flags_raw = (char *)"test1 sse4_1 sse3 ssse3 test2 testFour";
    fprintf( stdout, "Expected '%s', got '%s'.\n", expected, sysapi_processor_flags() );

    expected = "none";
    _sysapi_processor_flags = NULL;
    _sysapi_processor_flags_raw = (char *)"test1 test2 testFour";
    fprintf( stdout, "Expected '%s', got '%s'.\n", expected, sysapi_processor_flags() );

    expected = "none";
    _sysapi_processor_flags = NULL;
    _sysapi_processor_flags_raw = (char *)"";
    fprintf( stdout, "Expected '%s', got '%s'.\n", expected, sysapi_processor_flags() );

    expected = "sse4_1 sse4_2 ssse3";
    _sysapi_processor_flags = NULL;
    _sysapi_processor_flags_raw = (char *)"sse4_1 sse4_2 ssse3";
    fprintf( stdout, "Expected '%s', got '%s'.\n", expected, sysapi_processor_flags() );

    expected = "sse4_1 sse4_2 ssse3";
    _sysapi_processor_flags = NULL;
    _sysapi_processor_flags_raw = (char *)"sse4_1 sse4_2 ssse3 ssse3";
    fprintf( stdout, "Expected '%s', got '%s'.\n", expected, sysapi_processor_flags() );

    expected = "sse4_1 sse4_2 ssse3";
    _sysapi_processor_flags = NULL;
    _sysapi_processor_flags_raw = (char *)"sse4_1 sse4_2 ssse3 ssse3";
    fprintf( stdout, "Expected '%s', got '%s'.\n", expected, sysapi_processor_flags() );

    expected = "sse4_1 sse4_2 ssse3";
    _sysapi_processor_flags = NULL;
    _sysapi_processor_flags_raw = (char *)"ssse3 sse4_1 sse4_2 ssse3 ssse3";
    fprintf( stdout, "Expected '%s', got '%s'.\n", expected, sysapi_processor_flags() );

    expected = "sse4_2 ssse3";
    _sysapi_processor_flags = NULL;
    _sysapi_processor_flags_raw = (char *)"test1 sse4_2 sse3 ssse3 test2 testFour";
    fprintf( stdout, "Expected '%s', got '%s'.\n", expected, sysapi_processor_flags() );
    
    return 0;
}

#endif