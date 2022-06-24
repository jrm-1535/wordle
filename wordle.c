
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

static char key_attrs[ ALPHABET_SIZE ];
static inline void init_key_attrs( void )
{
    memset( key_attrs, UNKNOWN, sizeof(key_attrs) );
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
        char key_attr = key_attrs[ try[i]-'a' ];
        switch( position[i] ) {
        case NOT_IN:
            if ( key_attr != CORRECT &&  key_attr != IN_WRONG ) {
                key_attrs[ try[i]-'a' ] = NOT_IN;
            }
            fputs( RED_BG, stdout );
            break;
        case IN_WRONG:
            if ( key_attr != CORRECT ) {
                key_attrs[ try[i]-'a' ] = IN_WRONG;
            }
            fputs( YELLOW_BG, stdout );
            break;
        case CORRECT:
            key_attrs[ try[i]-'a' ] = CORRECT;
            fputs( GREEN_BG, stdout );
            break;
        }
        putchar( try[i] );
    }
    fputs( DEFAULT, stdout );

    for ( int i = 0; i < ALPHABET_SIZE; ++ i ) {
        switch( key_attrs[i] ) {
        case NOT_IN: fputs( RED_BG, stdout ); break;
        case IN_WRONG: fputs( YELLOW_BG, stdout ); break;
        case CORRECT: fputs( GREEN_BG, stdout ); break;
        case UNKNOWN: fputs( BLUE_BG, stdout ); break;
        }
        putchar( 'a' + i );
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
#else // for debugging specific word
    printf( "Playing wordle:\n" );
    const char *word = "afoul";
    if ( ! is_word_in_dictionary( word ) ) {
        printf("Word %s is not in dictionary\n", word );
        exit(0);
    }
#endif
    init_key_attrs();
    int stdin_fd = fileno( stdin );
    char buffer[WORD_SIZE+1];
    do {
        read( stdin_fd, (void *)buffer, WORD_SIZE+1 );
        buffer[ WORD_SIZE ] = 0;
        fputs( UP, stdout );
    } while ( ! check_match( word, buffer ) );
}

static void help( void )
{
    printf( "wordle -h -f -d=<sets>\n" );
    printf( "    Wordle prints a list of letter statistics in words or a list of\n" );
    printf( "    possible words, given the constraints expressed by option -d,\n" );
    printf( "    or starts a random wordle game if no argument is given.\n\n" );
    printf( "Options:\n" );
    printf( "    -h  print this help message and exits.\n" );
    printf( "    -f  print frequencies of letter appearance at all positions\n" );
    printf( "        and exits\n" );
    printf( "    -d  print the list of words that match the given constraints\n" );
    printf( "        and exits. The constraints are expressed as a string made\n" );
    printf( "        of a series of sets, each set describing the results of an\n" );
    printf( "        attempt to guess a word of 5 letters. The maximum number\n" );
    printf( "        of attempts is 6 (as in wordle). The status of each letter\n" );
    printf( "        in the word is given as a couple or characters: the first\n" );
    printf( "        one is a code indicating whether the following letter is at\n" );
    printf( "        the right position (r), a wrong position (w) or not in the\n" );
    printf( "        word(n), and the second one is the letter in question.\n\n" );
    printf( "Options -d and -f are exclusive.\n\n");
    printf( "Examples:\n" );
    printf( "    wordle -d=rsnlwawtne\n" );
    printf( "        means that a first attempt was made with 'slate' and the\n" );
    printf( "        result is: s is at the right position, l and e and not in\n" );
    printf( "        the word to guess and both a and t are in the word but at\n" );
    printf( "        a wrong position.\n" );
    printf( "    wordle -d=rsnlwawtnersntwanrrt\n" );
    printf( "        this is the same first attemp as above followed by a second\n" );
    printf( "        one with 'start': s is still at the right position, 't' and\n" );
    printf( "        'r' are not (anymore) in the word, 'a' is still at a wrong\n" );
    printf( "        position and 't' is now at the right position in the word.\n" );
    printf( "        notice that 't' is both at the right position as the last\n" );
    printf( "        letter in the word and not in the word anymore as second\n" );
    printf( "        letter - an indication that the word contains only one 't'.\n");
    printf( "    This might be \"saint\", for example\n\n" );
    printf( "Note:\n" );
    printf( "    Without options, wordle selects a random word and starts an\n" );
    printf( "    interactive game asking you to guess the word.\n" );
}

typedef struct {
    char *data;
    bool frequencies;
} args_t;

static void get_args( int argc, char **argv, args_t *args )
{
    assert( NULL != args );
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
                    args->data = s;
                } else {
                    printf("wordle: option -d must be followed by '='\n");
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

enum operations {
 STATS, PLAY, SOLVE
};

static enum operations process_args( args_t *args, solver_data *given )
{
    if ( args->frequencies ) {
        return STATS;
    }

    if ( NULL == args->data ) {
        return PLAY;
    }

    init_solver_data( given );
    if ( SOLVER_DATA_SET != set_solver_data( given, args->data ) ) {
        exit(1);
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
//        print_solver_data( &given );
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
        break;
    }
    discard_dictionary( );
    return 0;
}
