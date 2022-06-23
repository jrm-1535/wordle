
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>

#include "wdict.h"
#include "wstats.h"
#include "wpos.h"
#include "wsolve.h"

#define NOT_IN     '-'
#define IN_WRONG   'w'
#define CORRECT    'r'
#define UNKNOWN    ' '

typedef struct {
  char  attr, letter;
} chattr;

chattr all[ ALPHABET_SIZE ];
static void init_all( void )
{
    for ( int i = 0; i < ALPHABET_SIZE; ++i ) {
        all[i].attr = UNKNOWN;
        all[i].letter = 'a' + i;
    }
}

static bool check_match( const char *ref, char *try )
{
    char position[WORD_SIZE+1];
    if ( -1 == get_position_from_words( ref, try, position ) ) {
        printf( "%c%c%c%c%c is not in dictionary, try again\n",
                try[0], try[1], try[2], try[3], try[4] );
        return false;
    }

    for ( int i = 0; i < WORD_SIZE; ++ i ) {
        char key_attr = all[ try[i]-'a' ].attr;
        switch( position[i] ) {
        case NOT_IN:
            if ( key_attr != CORRECT &&  key_attr != IN_WRONG ) {
                all[ try[i]-'a' ].attr = NOT_IN;
            }
            fputs( RED_BG, stdout );
            break;
        case IN_WRONG:
            if ( key_attr != CORRECT ) {
                all[ try[i]-'a' ].attr = IN_WRONG;
            }
            fputs( YELLOW_BG, stdout );
            break;
        case CORRECT:
            all[ try[i]-'a' ].attr = CORRECT;
            fputs( GREEN_BG, stdout );
            break;
        }
        putchar( try[i] );
    }
    fputs( DEFAULT, stdout );

    for ( int i = 0; i < ALPHABET_SIZE; ++ i ) {
        switch( all[i].attr ) {
        case NOT_IN: fputs( RED_BG, stdout ); break;
        case IN_WRONG: fputs( YELLOW_BG, stdout ); break;
        case CORRECT: fputs( GREEN_BG, stdout ); break;
        case UNKNOWN: fputs( BLUE_BG, stdout ); break;
        }
        putchar( all[i].letter );
    }
    fputs( NORMAL, stdout );
    if ( 0 == strncmp( ref, try, WORD_SIZE ) ) {
        return true;
    } else {
        return false;
    }
}

static void play( void )
{
#if 1
    unsigned int seed = time( NULL );
    int n_words = get_dictionary_size();
    srand( seed );
    int word_number = rand() % n_words;
    const char *word = get_nth_word_in_dictionary( word_number );
    printf( "Playing wordle - number %d:\n", word_number );
#else
    printf( "Playing wordle:\n" );
    const char *word = "patsy";
    if ( ! is_word_in_dictionary( word ) ) {
        printf("Word %s is not in dictionary\n", word );
        exit(0);
    }
#endif
    init_all();
    int stdin_fd = fileno( stdin );
    char buffer[WORD_SIZE+1];
    do {
        read( stdin_fd, (void *)buffer, WORD_SIZE+1 );
        buffer[ WORD_SIZE ] = 0;
        fputs( UP, stdout );
    } while ( ! check_match( word, buffer ) );
}

enum operations {
 STATS, PLAY, SOLVE
};

void help( void )
{
    printf( "wordle -h -f -k=<set> -n=<letters> -w=<sets>\n" );
    printf( "    wordle prints a list of possible words, given the constraints\n" );
    printf( "    expressed by options -k, -n and -w.\n\n" );
    printf( "Options:\n" );
    printf( "    -h  print this help message and exits.\n" );
    printf( "    -f  print frequencies of letter appearance at all positions\n" );
    printf( "        and exits\n" );
    printf( "    -k  letters known to be at their location. This option must\n" );
    printf( "        be given as exactly 5 characters, with a dash replacing\n" );
    printf( "        unknown letters.\n" );
    printf( "    -n  list of letters known not to be in the solution.\n" );
    printf( "    -w  letters known to be in the solution, but at a wrong\n" );
    printf( "        location. This option must be given as a series of sets\n" );
    printf( "        of exactly 5 characters, each set separated by commas\n" );
    printf( "        and consisting of one or more occurences of the same\n" );
    printf( "        letter at wrong location(s) and as many occurences of\n" );
    printf( "        a dash replacing unknown location(s) as needed.\n\n" );
    printf( "    -d  alternatively, option -d replaces a combination of -k, -n\n" );
    printf( "        and -w with a string made of a series of 5 sets, each set\n" );
    printf( "        having 2 letters. The first one is a code indicating whether\n" );
    printf( "        the following letter is at an exact position (e), a wrong\n" );
    printf( "        position (w) or not in the word (n), and the second one\n" );
    printf( "        is the letter in question. Option d and options -k, -w or\n" );
    printf( "        -n are exclusive.\n\n");
    printf( "Example:\n" );
    printf( "    wordle -w=a----,---i- -n=lgrhoy -k=s---t\n\n" );
    printf( "    This might be \"saint\"\n\n" );
    printf( "Note:\n" );
    printf( "    By default, without option -n the list of letters known not\n" );
    printf( "    to be in is assumed to be an empty list, without option -w\n" );
    printf( "    the list of letters known to be in at a wrong location is\n" );
    printf( "    assumed to be an empty list and without -k, no letter known\n" );
    printf( "    to be at their location is assumed.\n\n");
    printf( "    Without options, wordle selects a random word and starts an\n" );
    printf( "    interactive game asking you to guess the word.\n" );
}

typedef struct {
    char *out, *wrong, *known, *data;
    bool frequencies;
} args_t;

void get_args( int argc, char **argv, args_t *args )
{
    assert( NULL != args );
    args->out = NULL;
    args->wrong = NULL;
    args->known = NULL;
    args->data = NULL;
    args->frequencies = false;

    char **pp = &argv[1];
    while (--argc) {
        char *s = *pp;
        if (*s++ == '-') {
            switch ( *s++ ) {
            case 'h': case 'H':
                help();
                exit(0);
            case 'f': case 'F':
                args->frequencies = true;
                break;
            case 'd': case 'D':
                if (*s++ == '=') {
                    if ( NULL != args->data ) {
                        printf("Wordle: error multiple options -d\n");
                        exit(1);
                    }
                    if ( NULL != args->out || NULL != args->wrong ||
                         NULL != args->known ) {
                        printf("Wordle: error option -d and options -k -w -k are exclusive\n");
                        exit(1);
                    }
                    args->data = s;
                } else {
                    printf("wordle: option -d must be followed by '='\n");
                    exit(1);
                }
                break;

            case 'n': case 'N':
                if (*s++ == '=') {
                    if ( NULL != args->out ) {
                        printf("Wordle: error multiple options -n\n");
                        exit(1);
                    }
                    args->out = s;
                } else {
                    printf("wordle: option -n must be followed by '='\n");
                    exit(1);
                }
                break;
            case 'w': case 'W':
                if (*s++ == '=') {
                    if ( NULL != args->wrong ) {
                        printf("Wordle: error multiple options -w\n");
                        exit(1);
                    }
                    args->wrong = s;
                } else {
                    printf("wordle: option -w must be followed by '='\n");
                    exit(1);
                }
                break;
            case 'k': case 'K':
                if (*s++ == '=') {
                    if ( NULL != args->known ) {
                        printf("Wordle: error multiple options -k\n");
                        exit(1);
                    }
                    args->known = s;
                } else {
                    printf("wordle: option -k must be followed by '='\n");
                    exit(1);
                }
                break;
            default:
                printf("wordle: error option -%c not recognized\n", *(s-1));
                help();
                exit(1);
            }
        } else {
            printf("wordle: unrecognized parameter %s\n", s-1);
            help();
            exit(1);
        }
        pp++;
    }
}

void set_solver_data_from_args( solver_data *given, args_t *args )
{
    if ( args->out ) {
        int n = strlen( args->out );
        if ( n >= MAX_TRIES * WORD_SIZE ) {
            printf("wordle: too many letters known to be out (%d)\n", n );
            exit(1);
        }
        for ( int i = 0; i < n; ++i ) {
            if ( args->out[i] < 'a' || args->out[i] > 'z' ) {
                printf( "wordle: error invalid entry (%c) in letters known to be out\n",
                            args->out[i] );
                exit(1);
            }
        }
        given->out = malloc( n + 1 );
        strcpy( given->out, args->out );
//        given->out = args->out;
    } else {
        given->out = malloc( 1 );
        given->out[0] = '\0';
    }

    if ( args->known ) {
        int n = strlen( args->known );
        if ( n != 5 ) {
            printf("wordle: 5 letters or '-' specify the known letters et location\n");
            exit(1);
        }

        for ( int i = 0; i < WORD_SIZE; ++i ) {
            if ( args->known[i] != '-' ) {
                if ( args->known[i] < 'a' || args->known[i] > 'z' ) {
                    printf( "wordle: error invalid letter (%c) in known set\n",
                            args->known[i] );
                    exit(1);
                }
                if ( NULL != strchr( given->out, args->known[i] ) ) {
                    printf( "wordle: known letter %c is also not in solution\n",
                            args->known[i] );
                    exit(1);
                }
            }
        }
        memcpy( given->known, args->known, WORD_SIZE + 1 );
    }

    if ( args->wrong ) {

        char *set = args->wrong;
        int index = 0;                          // in required
        int index_at_pos[WORD_SIZE] = { 0 };    // in wrong[pos]

        while ( *set != 0 ) {
            if ( ',' == *set ) { ++set; }

            char *p = set;  // first pass to check letters in each set
            char c = ' ';   // single letter in a set

            for ( int i = 0; i < WORD_SIZE; ++i ) {
                if ( *p == 0 ) {
                    printf( "wordle: error inconsistent set for -w (%s)\n",
                            set );
                    exit(1);
                }
                if ( *p != '-' ) {
                    if ( *p < 'a' || *p > 'z' ) {
                        printf( "wordle: error invalid letter (%c) in wrong sets\n",
                                *p );
                        exit(1);
                    }
                    if ( NULL != strchr( given->out, *p ) ) {
                        printf( "wordle: wrong letter %c is also not in solution\n",
                                *p );
                        exit(1);
                    }
                    if ( ' ' == c ) {
                        c = *p;
                    } else if ( c != *p ) {
                        printf( "wordle: error inconsistent set for -w (%s)\n",
                                set );
                        exit(1);
                    }
                }
                ++p;
            }

            if ( ' ' != c ) {  // second pass to get constraints
                p = set;
                if ( NULL == strchr( given->known, c ) ) {
                    given->required[index] = c;     // already known, so not required
                    given->required[++index] = 0;   // anymore, but still wrong @pos
                }
                for ( int i = 0; i < WORD_SIZE; ++i ) {
                    if ( '-' != *p ) {
                        given->wrong[i][index_at_pos[i]] = c;
                        ++index_at_pos[i];
                    }
                    ++p;
                }
            }
            set = p;
        }
    }
}

enum operations process_args( args_t *args, solver_data *given )
{
    if ( args->frequencies ) {
        return STATS;
    }

    if ( NULL == args->out && NULL == args->known && NULL == args->wrong &&
         NULL == args->data ) {
        return PLAY;
    }

    init_solver_data( given );
    if ( args->data ) {
        if ( SOLVER_DATA_SET != set_solver_data( given, args->data ) )
            exit(1);
    } else {
        set_solver_data_from_args( given, args );
    }

    return SOLVE;
}

int main ( int argc, char **argv )
{
    args_t args;
    get_args( argc, argv, &args );

    load_dictionary( WORDLE_DICTIONARY );

    solver_data given;
    enum operations op = process_args( &args, &given );

    word_node *result;
    switch ( op ) {
    case STATS:
        print_letter_stats( );
        break;
    case SOLVE:
        print_solver_data( &given );
        result = get_solutions( &given );
        if ( NULL == result ) {
            printf( "No solution\n" );
        } else {
            const char *best = select_most_likely_word( result );
            printf("Possiblities:\n");
            for ( word_node *wn = result ; wn; wn = wn->next ) {
                printf(" %s\n", wn->word );
            }
            if ( NULL != best ) {
                printf( "Suggesting to try %s\n", best );
            }
            free_word_list( result );
        }
        discard_solver_data( &given );
        break;
     case PLAY:
        play( );
    }
    discard_dictionary( );
    return 0;
}
