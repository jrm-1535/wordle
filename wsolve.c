
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "wdict.h"
#include "wsolve.h"

typedef struct {
    word_node       *subset;            // current subset
    char            *out;               // definitely not in
    char            *known;             // must be @ position
    char            *required;          // required but
    char            *wrong[WORD_SIZE];  // must not be @ position
    int             n_required;
} subset_constraints_alt;

void match( void *ctxt, char *word )
{
    subset_constraints_alt *sc = ctxt;
    for ( int i = 0; i < sc->n_required; ++i ) {
        if ( NULL == strchr( word, sc->required[i] ) ) {
            return;
        }
    }
    for ( int i = 0; i < WORD_SIZE; ++i ) {
        if ( NULL != strchr( sc->out, word[i] ) ) {
            return;
        }
        if ( sc->known[i] != '-' && sc->known[i] != word[i] ) {
            return;
        }
        if ( sc->wrong[i] && strchr( sc->wrong[i], word[i] ) ) {
            return;
        }
    }
    word_node *wn = malloc( sizeof(word_node) );
    assert( wn );
    wn->word = word;
    wn->next = sc->subset;
    sc->subset = wn;
}

extern word_node *get_solutions( solver_data *given )
{
    subset_constraints_alt constraints;
    constraints.subset = NULL;
    constraints.out = given->out;
    constraints.known = given->known;
    constraints.required = given->required;
    constraints.n_required = strlen( constraints.required );
    memcpy( constraints.wrong, given->wrong, sizeof(char *) * WORD_SIZE );

    for_each_word_in_dictionary( match, &constraints );
    return constraints.subset;
}


extern void init_solver_data( solver_data *data )
{
    strcpy( data->known, "-----" );
    data->out = NULL;
    memset( data->required, 0, WORD_SIZE+1 );
    // up to WORD_SIZE wrong characters at up to WORD_SIZE positions
    char *buffer = malloc( WORD_SIZE * WORD_SIZE );
    memset( buffer, 0, WORD_SIZE * WORD_SIZE );
    for ( int i = 0; i < WORD_SIZE; ++i ) {
        data->wrong[i] = &buffer[WORD_SIZE * i];
    }
}

extern void reset_solver_data( solver_data *data )
{
    strcpy( data->known, "-----" );
    if ( NULL != data->out ) {
        free( data->out );
        data->out = NULL;
    }
    memset( data->required, 0, WORD_SIZE+1 );
    memset( data->wrong[0], 0, WORD_SIZE * WORD_SIZE );
}

extern void discard_solver_data( solver_data *data )
{
    if ( NULL != data->out ) {
        free( data->out );
        data->out = NULL;
    }
    if ( NULL != data->wrong ) {
        free( data->wrong[0] );
    }
    for ( int i = 0; i < WORD_SIZE; ++i ) {
        data->wrong[i] = NULL;
    }
}

// no more than 6 sets of 5 letters
#define MAX_LETTER_COUNT    (MAX_TRIES*WORD_SIZE)

extern solver_data_status set_solver_data( solver_data *given, char *data )
{
    int n = strlen(data);
    if ( n % 10 != 0 ) {
        printf("wordle: data string must have a number of letters multiple of 10 (%d)\n", n );
        return NON_MODULO_10_DATA_STRING_LENGTH;
    }
    int n_letters = n >> 1;
    if ( n_letters >= MAX_LETTER_COUNT ) {
        printf("wordle: too many attempts (%d)\n", n_letters / 5 );
        return DATA_STRING_LENGTH_TOO_LARGE;
    }

    // first pass find the number of required, known and not in word letters
    char req_t[ MAX_LETTER_COUNT ] = { 0 }; // temp storage for required
    char knw_t[ MAX_LETTER_COUNT ] = { 0 }; // temp storage for known
    int n_required = 0, n_out = 0, n_known = 0 ;
    for ( int i = 0; i < n; i += 2 ) {
        switch ( data[i] ) {
        case 'w' :
            if ( NULL == strchr( req_t, data[i+1] ) ) {
                req_t[n_required] = data[i+1];
                ++n_required;
            }
            break;
        case 'n' :
            ++n_out;
            break;
        case 'k' :
            if ( NULL == strchr( knw_t, data[i+1] ) ) {
                knw_t[n_known] = data[i+1];
                ++n_known;
            }
            break;
        default:
            printf( "wordle: invalid code (%c) in data\n", data[i] );
            return INVALID_CODE_IN_DATA;
        }
        if ( data[i+1] < 'a' || data[i+1] > 'z' ) {
            printf( "wordle: invalid letter (%c) in data\n", data[i+1] );
            return INVALID_LETTER_IN_DATA;
        }
    }
    if ( n_known > WORD_SIZE ) {
        printf( "wordle: too many known letters (%d)\n", n_known );
        return TOO_MANY_EXACT_POSITION_LETTERS;
    }
    if ( n_out > MAX_TRIES * WORD_SIZE ) {
        printf("wordle: too many letters known to be out (%d)\n", n );
        return TOO_MANY_NOT_IN_LETTERS;
    }
    if ( n_required >= WORD_SIZE ) {
        printf( "wordle: too many wrong letters (%d)\n", n_required );
        return TOO_MANY_WRONG_POSITION_LETTERs;
    }

    printf( "n_required = %d, n_out = %d, n_known = %d\n",
            n_required, n_out, n_known );

    // malloc room for out
    given->out = malloc( n_out + 1 );
    assert( given->out );
    memset( given->out, 0, n_out + 1 );

    // second pass, store out and known data
    int i_out = 0;
    for ( int i = 0; i < n; i += 2 ) {
        switch ( data[i] ) {
        case 'n' :
            if ( NULL != strchr( knw_t, data[i+1] ) ) {
                printf( "wordle: known letter (%c) is given as not in\n",
                        data[i+1] );
                free( given->out );
                return EXACT_POSITION_NOT_IN_LETTERS;
            }
            if ( NULL != strchr( req_t, data[i+1] ) ) {
                printf( "wordle: letter at wrong position (%c) is given as not in\n",
                        data[i+1] );
                free( given->out );
                return WRONG_POSITION_NOT_IN_LETTERS;
            }
            if ( NULL == strchr( given->out, data[i+1] ) ) {
                given->out[i_out] = data[i+1];
                ++ i_out;
            }   // ignore repeats
            break;
        case 'k' :
            if ( given->known[ (i % 10) / 2 ] != '-' &&
                 given->known[ (i % 10) / 2 ] != data[i+1] ) {
                printf( "wordle: different known letters (%c) at same position\n",
                        data[i+1] );
                free( given->out );
                given->out = NULL;
                return CONFLICTING_EXACT_POSITION_LETTERS;
            }
            given->known[ (i % 10) / 2 ] = data[i+1];
            break;
        }
    }
    given->out[i_out] = 0;

    // last pass, process wrong positions (needs known created in 2nd pass)
    int index_at_pos[WORD_SIZE] = { 0 };
    int i_required = 0;
    for ( int i = 0; i < n; i += 2 ) {
        if ( 'w' == data[i] ) {
            if ( NULL == strchr( given->required, data[i+1] ) ) {
                if ( NULL == strchr( given->known, data[i+1] ) ) {
                    // required only if not already known
                    given->required[i_required++] = data[i+1];
                }   // but still impossible at that location
                given->wrong[(i % 10) / 2][index_at_pos[(i % 10) / 2]] = data[i+1] ;
                ++index_at_pos[(i % 10) / 2];
            }
        }
    }
    return SOLVER_DATA_SET;
}

