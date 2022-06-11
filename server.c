
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <arpa/inet.h>

#ifndef _WIN32
#include <sys/select.h>
#include <sys/socket.h>
#else
#include <winsock2.h>
#endif

#include <microhttpd.h>
#include "wordle.h"
#include "wstats.h"
#include "wdict.h"
#include "wsolve.h"

#define PORT        8888
#define MAIN_URL    "/solver"
#define API_URL     "/solver/solve"
#define JSON_DATA   "application/json"

static int on_client_connect( void *cls, const struct sockaddr *addr, socklen_t addrlen )
{
    (void)cls;
    if ( addr->sa_family == AF_INET ) {
/*
        printf("addr: sa_family=%d sa_data: %02x %02x %02x %02x %02x %02x\n",
        addr->sa_family, addr->sa_data[0], addr->sa_data[1], addr->sa_data[2],
         addr->sa_data[3], addr->sa_data[4], addr->sa_data[5]);
*/
        char ip4[INET_ADDRSTRLEN];  // space to hold the IPv4 string
        inet_ntop(AF_INET, &(addr->sa_data[2]), ip4, INET_ADDRSTRLEN);
        printf( "Address IPv4 length %d: %s port %x\n", addrlen, ip4,
                (addr->sa_data[0] << 8) + addr->sa_data[1] );
    } else if ( addr->sa_family == AF_INET6 ) {
        char ip6[INET6_ADDRSTRLEN]; // space to hold the IPv6 string
        inet_ntop(AF_INET6, &(addr->sa_data[2]), ip6, INET6_ADDRSTRLEN);
        printf( "Address IPv6 length %d: %s port %x\n", addrlen, ip6,
                (addr->sa_data[0] << 8) + addr->sa_data[1] );
    }
    return MHD_YES;
}

static int check_headers( void *cls, enum MHD_ValueKind kind,
                   const char *key, const char *value )
{
    (void)cls;
    (void)kind;
    printf( "%s: %s\n", key, value );
    return MHD_YES;
}

static int get_data_query( void *cls, enum MHD_ValueKind kind,
                        const char *key, const char *value )
{
    (void)kind;
    char **data_ptr = cls;
    printf( "%s: %s\n", key, value );
    if ( 0 == strcmp( key, "data" ) ) {
        int size = strlen( value ) + 1;
        *data_ptr = malloc( size );
        strcpy( *data_ptr, value );
    }   
    return MHD_YES;
}

static char * solve( char *data, solver_data *sd )
{
    static char buffer[ 4000 ];
//    printf( "data: %s\n", data );

    solver_data_status sds = set_solver_data( sd, data );
    if ( SOLVER_DATA_SET != sds ) { // invalid data: return no solution
        strcpy( buffer, "{ \"suggest\": \"\", \"list\": [] }" );
        return buffer;
    }
    word_node *res = get_solutions( sd );
    reset_solver_data( sd );

    if ( NULL == res ) {
        strcpy( buffer, "{ \"suggest\": \"\", \"list\": [] }" );
    } else {
        printf( "Found solution(s)\n" );
        char *best = select_most_likely_word( res );
        if ( NULL == best ) {
            best = "";
        }
        sprintf( buffer, "{ \"suggest\": \"%s\", \"list\": [ ", best );
        int offset = strlen( buffer );
        for ( word_node *wn = res ; wn; wn = wn->next ) {
            printf(" %s\n", wn->word );
            sprintf( &buffer[offset], "\"%s\"", wn->word );
            offset += WORD_SIZE + 2; // including ""
            if ( wn->next ) {
                strcpy( &buffer[offset], ", " );
                offset += 2;
            }
        }
        sprintf( &buffer[offset], " ] }" );
    }
    free_word_list( res );
    printf( "response:\n%s\n", buffer );
    return buffer;
}

static char * page = NULL;

static char * main_static_page( char *path )
{
    if ( NULL == page ) {
        FILE *f = fopen( path, "r" );
        if ( NULL == f ) {
            printf( "server: could not open file %s - exiting\n", path );
            exit(1);
        }
        fseek( f, 0L, SEEK_END );
        long len = ftell( f );      // get file size
        rewind( f );

        printf( "file %s length %ld\n", path, len );
        page = malloc( (size_t)len + 1 );
        if ( NULL == page ) {
            printf( "server: could not allocate memory for static page - exiting\n" );
            exit(1);
        }

        size_t actual = fread( page, (size_t)1, (size_t)len, f );
        if ( actual != (size_t)len ) {
            printf( "server: error reading file %s - exiting\n", path );
            free( page );
            exit(1);
        }
        if ( 0 != fclose( f ) ) {
            printf( "server: error closing file %s - exiting\n", path );
            free( page );
            exit(1);
        }
    }
    return page;
}

static void free_static_page( void )
{
    if ( NULL != page ) {
        free( page );
    }
}

typedef struct {
    solver_data sd;
    char        *path;
} wordle_server;

static int answer_to_connection( void *cls, struct MHD_Connection *connection,
                                 const char *url, const char *method,
                                 const char *version, const char *upload_data,
                                 size_t *upload_data_size, void **con_cls )
{
    printf( "URL: %s METHOD %s version %s\n", url, method, version );
#if 0
    (void)cls;               /* Unused. Silent compiler warning. */
    (void)url;               /* Unused. Silent compiler warning. */
    (void)method;            /* Unused. Silent compiler warning. */
    (void)version;           /* Unused. Silent compiler warning. */
#endif
    (void)upload_data;       /* Unused. Silent compiler warning. */
    (void)upload_data_size;  /* Unused. Silent compiler warning. */
    (void)con_cls;           /* Unused. Silent compiler warning. */

    wordle_server *wsv = cls;
    MHD_get_connection_values( connection, MHD_HEADER_KIND, &check_headers, NULL );
    if ( 0 == strcmp( url, MAIN_URL ) ) {
        if ( 0 != strcmp( "GET", method ) ) {
            return MHD_NO;
        }
        char *page = main_static_page( wsv->path );
    //    printf( "Page:\n%s\n", page );
        struct MHD_Response *response = 
            MHD_create_response_from_buffer( strlen( page ), (void *) page, 
                                             MHD_RESPMEM_PERSISTENT);
        int ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
        MHD_destroy_response (response);
        return ret;
    }

    if ( 0 == strncmp( url, API_URL, sizeof(API_URL) - 1 ) ) {
        if ( 0 != strcmp( "GET", method ) ) {
            return MHD_NO;
        }
        printf( "GET called on %s\n", API_URL );
        char *data = NULL;
        MHD_get_connection_values( connection, MHD_GET_ARGUMENT_KIND,
                                   &get_data_query, &data );
        if ( data != NULL ) {

            char *page = solve( data, &wsv->sd );
            struct MHD_Response *response = 
                MHD_create_response_from_buffer( strlen( page ), (void *) page, 
                                                 MHD_RESPMEM_MUST_COPY);
            MHD_add_response_header(response, "Content-Type", JSON_DATA);
            int ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
            MHD_destroy_response (response);
            free( data );
            return ret;
        }
    }
    return MHD_NO;
}

int main( int argc, char **argv )
{
    (void)argc;

    wordle_server wsv;
    wsv.path = argv[1];
    init_solver_data( &wsv.sd );

    load_dictionary( WORDLE_DICTIONARY );

    struct MHD_Daemon *daemon;
    daemon = MHD_start_daemon( MHD_USE_AUTO | MHD_USE_INTERNAL_POLLING_THREAD, PORT,
                               &on_client_connect, NULL,
                               &answer_to_connection, &wsv, MHD_OPTION_END );
    if (NULL == daemon) {
        discard_dictionary( );
        discard_solver_data( &wsv.sd );
        return 1;
    }

    (void) getchar ();

    MHD_stop_daemon (daemon);
    free_static_page( );
    discard_dictionary( );
    discard_solver_data( &wsv.sd );
    return 0;
}
