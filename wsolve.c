
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "wdict.h"
#include "wpos.h"
#include "wsolve.h"

typedef struct {
    word_node       *subset;            // current subset
    char            *out;               // definitely not in
    char            *known;             // must be @ position
    char            *required;          // required but
    char            *wrong[WORD_SIZE];  // must not be @ position
    int             *required_count;    // required letter counts
    int             n_required;
} subset_constraints;

static void match( void *ctxt, const char *word )
{
    subset_constraints *sc = ctxt;

//    printf( "word: %s required: %s known: %s out: %s\n",
//            word, sc->required, sc->known, sc->out );
    int required_count[WORD_SIZE]; // local copy (modifed inside function)
    memcpy( required_count, sc->required_count, sizeof( int ) * WORD_SIZE );

    // for each position in word
    for ( int i = 0; i < WORD_SIZE; ++i ) {
        // first check if required letter at that position
        if ( sc->known[i] != '-' ) {
            // if not the same as candidate letter at that position
            if ( sc->known[i] != word[i] ) {
                return;     // reject candidate
            }   // otherwise match, move to next letter
        } else {    // no required letter at that position
                        // check if required at some other position
            char *rlp = strchr( sc->required, word[i] );
            if ( rlp ) {    // letter is potentially required
                if ( sc->wrong[i] && strchr( sc->wrong[i], word[i] ) ) {
                    return; // letter at wrong position: reject candidate
                }
                int rli = rlp-sc->required;
                if ( required_count[rli] > 0 ) { // instance still possible
                    -- required_count[rli];
                    continue;       // accept instance and move to next letter
                }
            }
            // candidate letter is not (or no longer) required
            if ( NULL != strchr( sc->out, word[i] ) ) { // and is no more in
                return;         // reject, letter can no longer be in word
            }   // else accept candidate and move on to next letter
        }
    }
    // reject if all required have not been exhausted
    for ( int i = 0; i < sc->n_required ; i++ ) {
        if ( required_count[i] != 0 )
            return;
    }
    word_node *wn = malloc( sizeof(word_node) );
    assert( wn );
    wn->word = word;
    wn->next = sc->subset;
    sc->subset = wn;
}

extern word_node *get_solutions( solver_data *given )
{
    //print_solver_data( given );
    subset_constraints constraints;
    constraints.subset = NULL;
    constraints.out = given->out;
    constraints.known = given->known;
    constraints.required = given->required;
    constraints.required_count = given->required_count;
    constraints.n_required = strlen( constraints.required );
    memcpy( constraints.wrong, given->wrong, sizeof(char *) * WORD_SIZE );
    for_each_word_in_dictionary( match, &constraints );
    return constraints.subset;
}

extern void print_solver_data( solver_data *given )
{
    printf( "solver data: required = %s, out = %s, known = %s\n",
            given->required, given->out, given->known );
    printf( "required number: %d, %d, %d, %d, %d\n",
            given->required_count[0], given->required_count[1],
            given->required_count[2], given->required_count[3],
            given->required_count[4] );
    for ( int i = 0; i < WORD_SIZE; ++i ) {
        printf( "position %d:", i );
        for ( int j = 0; j < WORD_SIZE; ++j ) {
            if ( 0 == given->wrong[i][j] )
                break;
            printf( " %c", given->wrong[i][j] );
        }
        printf( "\n" );
    }
}

extern void init_solver_data( solver_data *data )
{
    strcpy( data->known, "-----" );
    data->out = NULL;
    memset( data->required, 0, WORD_SIZE+1 );
    memset( data->required_count, 0, sizeof(int) *(WORD_SIZE+1) );
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
    memset( data->required_count, 0, sizeof(int) *(WORD_SIZE+1) );
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
    int n_letters = n >> 1;
    if ( n_letters >= MAX_LETTER_COUNT ) {
        printf("wordle: too many attempts (%d)\n", n_letters / 5 );
        return DATA_STRING_LENGTH_TOO_LARGE;
    }

    int index_at_pos[WORD_SIZE] = { 0 };
    given->out = malloc( n_letters + 1 );
    memset( given->out, 0, n_letters + 1 );

    //if ( 0 == strcmp( data, "nsnlnantwenfnenvkewr" ) ) {
    //    printf( "Processing data: nsnlnantwenfnenvkewr\n" );
    //}
    for ( int i = 0; i < n; i += (2 * WORD_SIZE) ) {
        solver_data_status status = update_solver_data( given, &data[i], index_at_pos );
        if ( SOLVER_DATA_SET != status ) {
            reset_solver_data( given );
            return status;
        }
    }
    return SOLVER_DATA_SET;
}

