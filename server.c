
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>

#include <microhttpd.h>

#include "wordle.h"
#include "wstats.h"
#include "wdict.h"
#include "wpos.h"
#include "wsolve.h"

#define PORT                8888

#define DEFAULT_PLAYER_PATH "player.html"
#define MAIN_PLAYER_URL     "/wordle/player"
#define PLAYER_API_URL      "/wordle/player/play"

#define DEFAULT_SOLVER_PATH "solver.html"
#define MAIN_SOLVER_URL     "/wordle/solver"
#define SOLVER_API_URL      "/wordle/solver/solve"

#define JSON_DATA           "application/json"

#if MHD_VERSION < 0x00097002
#define eMHD_Result  int
#else
#define eMHD_Result  enum MHD_Result
#endif

static eMHD_Result on_client_connect( void *cls, const struct sockaddr *addr,
                                      socklen_t addrlen )
{
    (void)cls;
    (void)addrlen;

    if ( addr->sa_family == AF_INET ) {

        const unsigned char *p = (unsigned char *)&addr->sa_data[0];
//        printf("addr: sa_family=%d sa_data: %02x %02x %02x %02x %02x %02x\n",
//         addr->sa_family, p[0], p[1], p[2], p[3], p[4], p[5]);

        char ip4[INET_ADDRSTRLEN];  // space to hold the IPv4 string
        inet_ntop(AF_INET, &(addr->sa_data[2]), ip4, INET_ADDRSTRLEN);
        printf( "IPv4 Address: %s:%u\n", ip4, (*p<< 8) + *(p+1) );
    } else if ( addr->sa_family == AF_INET6 ) {
        const unsigned char *p = (unsigned char *)&addr->sa_data[0];
        char ip6[INET6_ADDRSTRLEN]; // space to hold the IPv6 string
        inet_ntop(AF_INET6, &(addr->sa_data[2]), ip6, INET6_ADDRSTRLEN);
        printf( "IPv6 Address: %s:%u\n", ip6, (*p<< 8) + *(p+1) );
    }
    return MHD_YES;
}

static eMHD_Result check_headers( void *cls, enum MHD_ValueKind kind,
                                 const char *key, const char *value )
{
    (void)cls;
    (void)kind;
    printf( "%s: %s\n", key, value );
    return MHD_YES;
}

static int get_int_value( const char *value )
{
    char *end = NULL;
    int val = (int)strtol( value, &end, 10 );
    if ( end != NULL && 0 != *end ) {
        printf( "Garbage after integer number: <%s>\n", end );
    }
    return val;
}

typedef struct {
    int     game;
    int     attempt;
    char    word[WORD_SIZE+1];
} play_parameters;

static eMHD_Result get_player_query( void *cls, enum MHD_ValueKind kind,
                                     const char *key, const char *value )
{
    (void)kind;
    play_parameters *params = cls;
    assert( params );
    printf( "player query: %s=%s\n", key, value );
    if ( 0 == strcmp( key, "game" ) ) {
        params->game = get_int_value( value );
    } else if ( 0 == strcmp( key, "attempt" ) ) {
        params->attempt = get_int_value( value );
    } else if ( 0 == strcmp( key, "word" ) ) {
        strncpy( params->word, value, WORD_SIZE+1 );
        params->word[WORD_SIZE] = 0;
    }
    return MHD_YES;
}

static eMHD_Result get_solver_query( void *cls, enum MHD_ValueKind kind,
                                    const char *key, const char *value )
{
    (void)kind;
    char **data_ptr = cls;
    printf( "solver query: %s=%s\n", key, value );
    if ( 0 == strcmp( key, "data" ) ) {
        int size = strlen( value ) + 1;
        *data_ptr = malloc( size );
        strcpy( *data_ptr, value );
    }   
    return MHD_YES;
}

static int word_cmp( const void *p1, const void *p2 )
{
     return strcmp( * (char * const *) p1, * (char * const *) p2 );
}

#define ERROR_FORMAT     "{ \"error\": \"not in dictionary\", \"word\": \"slate\" }"
#define FAIL_FORMAT      "{ \"error\": \"failed to solve\", \"word\": \"slate\" }"
#define RESPONSE_FORMAT  "{ \"game\": 9999, \"word\": \"slate\", \"position\": \"--wrr\" }"
static char * play( play_parameters *pp )
{
    char *buffer;

    printf( "game=%d word=%s\n", pp->game, pp->word );
    if ( ! is_word_in_dictionary( pp-> word ) ) {
        buffer = malloc( sizeof( ERROR_FORMAT ) );
        snprintf( buffer, sizeof( ERROR_FORMAT ),
                  "{ \"error\": \"not in dictionary\", \"word\": \"%s\" }",
                   pp->word );
    } else {
        if ( -1 == pp->game ) {
            int n_words = get_dictionary_size();
            pp->game = rand() % n_words;
            printf( "Playing wordle - number %d\n", pp->game );
        }

        const char *ref = get_nth_word_in_dictionary( pp->game );
        printf("Reference: %s\n", ref );
        if ( MAX_TRIES-1 <= pp->attempt && strcmp( ref, pp->word ) ) {
            buffer = malloc( sizeof( FAIL_FORMAT ) );
            snprintf( buffer, sizeof( FAIL_FORMAT ),
                      "{ \"error\": \"failed to solve\", \"word\": \"%s\" }",
                       ref );
        } else {
            char position[WORD_SIZE+1];
            get_position_from_words( ref, pp->word, position );

            buffer = malloc( sizeof( RESPONSE_FORMAT ) );
            snprintf( buffer, sizeof( RESPONSE_FORMAT ),
                      "{ \"game\": %d, \"word\": \"%s\", \"position\": \"%s\" }",
                      pp->game, pp->word, position );
        }
    }
    printf( "response:\n%s\n", buffer );
    return buffer;
}

// assuming that all error msgs are less than 240 characters
#define ERROR_MSG_SIZE  256
#define EMPTY_RESPONSE  "{ \"suggest\": \"\", \"list\": [] }"
static char * solve( char *data, solver_data *sd )
{
//    printf( "data: %s\n", data );
    solver_data_status sds = set_solver_data( sd, data );

    if ( SOLVER_DATA_SET != sds ) { // invalid data: return am error
        char *msg;
        switch ( sds ) {
        case NON_MODULO_10_DATA_STRING_LENGTH:
        case INVALID_CODE_IN_DATA:
        case INVALID_LETTER_IN_DATA:
            msg = "invalid data string format";
            break;
        case DATA_STRING_LENGTH_TOO_LARGE:
            msg = "too many attempts";
            break;
        case CONFLICTING_EXACT_POSITION_LETTERS:
            msg = "conflicting letters at the same exact location";
            break;
        case EXACT_POSITION_LETTER_NOT_IN_WORD:
            msg = "letter at exact location is also given as not in word";
            break;
        case WRONG_POSITION_LETTER_IN_EXACT_POSITION:
            msg = "the same letter at the same position is given both as exact and wrong position";
            break;
        case WRONG_POSITION_LETTER_NOT_IN_WORD:
            msg = "the same letter at the same position is given both as wrong and not in word";
            break;
        case TOO_MANY_WRONG_POSITION_LETTERS:
            msg = "too many different letters given as at wrong position";
            break;
        default:    // to silence gcc
            break;
        }
        char *buffer = malloc( ERROR_MSG_SIZE );
        snprintf( buffer, ERROR_MSG_SIZE, "{ \"error\": \"%s\" }", msg );
        reset_solver_data( sd );
        return buffer;
    }
    word_node *res = get_solutions( sd );
    reset_solver_data( sd );

    char *buffer;
    if ( NULL == res ) {
        buffer = malloc( sizeof(EMPTY_RESPONSE) );
        strcpy( buffer, EMPTY_RESPONSE );
    } else {

        const char *best = select_most_likely_word( res );
        if ( NULL == best ) {
            best = "";
        }
        size_t nw = get_word_count( res );
        const char **words = malloc( sizeof(char *) * nw );

        nw = 0;
        for ( word_node *wn = res ; wn; wn = wn->next ) {
            words[nw++] = wn->word;
        }
        qsort( words, nw, sizeof(char *), word_cmp );

        // evaluate high boundary for response: (WORD_SIZE + 4) * nw for list,
        // plus WORD_SIZE + 4 for best plus <"{ suggest": > plus <"list": [] }>
        // that is (WORD_SIZE + 4 ) * ( nw + 1 ) + 32 (with some extra margin).
        buffer = malloc( (WORD_SIZE + 4 ) * ( nw + 1 ) + 32 );
        sprintf( buffer, "{ \"suggest\": \"%s\", \"list\": [ ", best );
        unsigned int offset = strlen( buffer );
        //printf( "Found solution(s)\n" );
        for ( unsigned int i = 0; i < nw; ++i ) {
            //printf(" %s\n", words[i] );
            sprintf( &buffer[offset], "\"%s\"", words[i] );
            offset += WORD_SIZE + 2; // including ""
            if ( i != nw-1 ) {
                strcpy( &buffer[offset], ", " );
                offset += 2;
            }
        }
        assert( offset < (WORD_SIZE + 4 ) * ( nw + 1 ) + 26 );
        sprintf( &buffer[offset], " ] }" );
        free( words );
        free_word_list( res );
    }
    printf( "response:\n%s\n", buffer );
    return buffer;
}

static char * get_static_page( char *path )
{
    //printf( "page path: %s\n", path );
    FILE *f = fopen( path, "r" );
    if ( NULL == f ) {
        printf( "server: could not open file %s - exiting\n", path );
        exit(1);
    }
    fseek( f, 0L, SEEK_END );
    long len = ftell( f );      // get file size
    rewind( f );

    printf( "file %s length %ld\n", path, len );
    char *page = malloc( (size_t)len + 1 );
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
    page[len] = 0;
    return page;
}

typedef struct {
    char    *player_path, *solver_path;
    char    *player_page, *solver_page;
} wordle_server;

static char *get_player_page( wordle_server *ws )
{
    if ( NULL == ws->player_page ) {
        ws->player_page = get_static_page( ws->player_path );
    }
    return ws->player_page;
}

static char *get_solver_page( wordle_server *ws )
{
    if ( NULL == ws->solver_page ) {
        ws->solver_page = get_static_page( ws->solver_path );
    }
    return ws->solver_page;
}

static void free_static_pages( wordle_server *ws )
{
    if ( NULL != ws->player_page ) {
        free( ws->player_page );
        ws->player_page = NULL;
    }
    if ( NULL != ws->solver_page ) {
        free( ws->solver_page );
        ws->solver_page = NULL;
    }
}

static eMHD_Result answer_to_connection( void *cls,
                                         struct MHD_Connection *connection,
                                         const char *url, const char *method,
                                         const char *version,
                                         const char *upload_data,
                                         size_t *upload_data_size,
                                         void **con_cls )
{
//    printf( "URL: %s METHOD %s version %s\n", url, method, version );
    (void)version;           /* Unused. Silent compiler warning. */
    (void)upload_data;       /* Unused. Silent compiler warning. */
    (void)upload_data_size;  /* Unused. Silent compiler warning. */
    (void)con_cls;           /* Unused. Silent compiler warning. */

    wordle_server *wsv = cls;
    MHD_get_connection_values( connection, MHD_HEADER_KIND, &check_headers, NULL );
    if ( 0 != strcmp( "GET", method ) ) {
        return MHD_NO;
    }

    if ( 0 == strcmp( url, MAIN_SOLVER_URL ) ) {
        char *page = get_solver_page( wsv );
        struct MHD_Response *response =
            MHD_create_response_from_buffer( strlen( page ), (void *) page,
                                             MHD_RESPMEM_PERSISTENT);
        int ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
        MHD_destroy_response (response);
        return ret;
    }

    if ( 0 == strcmp( url, MAIN_PLAYER_URL ) ) {
        char *page = get_player_page( wsv );
        struct MHD_Response *response =
            MHD_create_response_from_buffer( strlen( page ), (void *) page,
                                             MHD_RESPMEM_PERSISTENT);
        int ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
        MHD_destroy_response (response);
        return ret;
    }

    if ( 0 == strncmp( url, PLAYER_API_URL, sizeof(PLAYER_API_URL) - 1 ) ) {
        play_parameters pp;
        pp.game = -1;
        strcpy( pp.word, "     " );
        MHD_get_connection_values( connection, MHD_GET_ARGUMENT_KIND,
                                   &get_player_query, &pp );
        if ( pp.word != NULL ) {
            char *page = play( &pp );
            struct MHD_Response *response = 
                MHD_create_response_from_buffer( strlen( page ), (void *)page,
                                                 MHD_RESPMEM_MUST_COPY);
            MHD_add_response_header(response, "Content-Type", JSON_DATA);
            int ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
            MHD_destroy_response (response);
            free(page);
            return ret;
        }
    }

    if ( 0 == strncmp( url, SOLVER_API_URL, sizeof(SOLVER_API_URL) - 1 ) ) {
        char *data = NULL;
        MHD_get_connection_values( connection, MHD_GET_ARGUMENT_KIND,
                                   &get_solver_query, &data );
        if ( data != NULL ) {
            solver_data sd;
            init_solver_data( &sd );
            char *page = solve( data, &sd );
            struct MHD_Response *response = 
                MHD_create_response_from_buffer( strlen( page ), (void *)page,
                                                 MHD_RESPMEM_MUST_COPY);
            MHD_add_response_header(response, "Content-Type", JSON_DATA);
            int ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
            MHD_destroy_response (response);
            free(page);
            discard_solver_data( &sd );
            free( data );
            return ret;
        }
    }
    return MHD_NO;
}

static void help( void )
{
    printf( "Wordle server\n\n" );
    printf( "Simple server for a wordle game player and solver. The player\n" );
    printf( "and solver are accessible at two different urls, respectively\n" );
    printf( "/wordle/player and /wordle/solver.\n\n");
    printf( "Usage:\n  wserver [-h] [-p=<path>] [-s=<path>]\n\n" );
    printf( "Options:\n     -h          print this help and exit\n" );
    printf( "     -p=<path>   path to the player main html page (default player.html)\n" );
    printf( "     -s=<path>   path to the solver main html page (default solver.html)\n" );
}

static void error( char *message )
{
    printf( "wserver: error %s\n", message );
    help( );
    exit(1);
}

static void get_args( int argc, char **argv, wordle_server *wsv )
{
    assert( NULL != wsv );
    wsv->player_path = DEFAULT_PLAYER_PATH;
    wsv->solver_path = DEFAULT_SOLVER_PATH;
    wsv->player_page = NULL;
    wsv->solver_page = NULL;

    char **pp = &argv[1];
    while (--argc) {
        char *s = *pp;
        if (*s++ == '-') {
            switch ( *s++ ) {
            case 'h': case 'H':
                help();
                exit(0);
            case 'p': case 'P':
                if (*s++ != '=') {
                    error( "missing '=' after option p" );
                }
                wsv->player_path = s;
                break;
            case 's': case 'S':
                if (*s++ != '=') {
                    error( "missing '=' after option s" );
                }
                wsv->solver_path = s;
                break;
            }
        }
    }
}

extern int main( int argc, char **argv )
{
    (void)argc;

    wordle_server wsv;
    get_args( argc, argv, &wsv );

    unsigned int seed = time( NULL );
    srand( seed );

    load_dictionary( WORDLE_DICTIONARY );

    struct MHD_Daemon *daemon;
    daemon = MHD_start_daemon( MHD_USE_AUTO | MHD_USE_INTERNAL_POLLING_THREAD, PORT,
                               &on_client_connect, NULL,
                               &answer_to_connection, &wsv, MHD_OPTION_END );
    if (NULL == daemon) {
        discard_dictionary( );
        return 1;
    }

    (void) getchar ();

    MHD_stop_daemon (daemon);
    discard_dictionary( );
    free_static_pages(  &wsv );

    return 0;
}
