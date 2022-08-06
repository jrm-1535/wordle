
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "wdict.h"
#include "wpos.h"

/*
    Contains code related to how letter positions are handled,
    both within the player and the solver:
*/

// get_position_from_words first checks if the candidate word belongs in the
// dictionary and returns an error if it does not. If it does, it compares the
// candidate word to the reference ref and updates the position according to
// the results of the comparison: e.g pos="-r-wr", where '-' indicates that the
// letter at that position in word does not belong in ref, 'r' indicates that
// the letter at that position in word matches the letter in ref at the same
// position and 'w' indicates that altough the letter at that position in word
// belongs in ref, it does not appear at the same position in ref. It expects
// pos to point to an array of WORD_SIZE+1 bytes, and does not allocate any
// memory for it.
//
// Note that if the candidate word contains the same letter at multiple
// positions, it may set different values in pos, depending on the case:
// - if that letter does not belong in ref, all positions are set at '-'
// - if that letter belongs in ref, one position may be set to 'r' if ref
//   has the same letter at that position, a second or third position may
//   be set to 'r' if ref has also the same letter at that second or third
//   position, or 'w' if ref has also the same letter but not at that second
//   or third position.
//
// it returns 0 in case of success or -1 in case of error.

extern int get_position_from_words( const char *ref, const char *word,
                                     char *pos )
{
    assert( WORD_SIZE == strnlen( ref, WORD_SIZE+ 1 ) );
    char ref_tmp[WORD_SIZE+1];
    strcpy( ref_tmp, ref );

    assert( WORD_SIZE == strnlen( word, WORD_SIZE+1 ) );
    if ( ! is_word_in_dictionary( word ) ) {
        return -1;
    }        
    char word_tmp[WORD_SIZE+1];
    strcpy( word_tmp, word );

    memset( pos, '-', WORD_SIZE );
    pos[WORD_SIZE] = 0;

    // 2 passes to reproduce NYT wordle behavior
    for ( int i = 0; i < WORD_SIZE; ++i ) {
        if ( word_tmp[i] == ref_tmp[i] ) {
            pos[i] = 'r';
            word_tmp[i] = ' ';  // remove letter at exact position
            ref_tmp[i] = ' ';   // from word and ref
        }
    }
    for ( int i = 0; i < WORD_SIZE; ++i ) {
        if ( word_tmp[i] == ' ' ) {
            continue;   // do not erase pos[i] if already set in previous pass
        }

        char *ref_pos = strchr( ref_tmp, word_tmp[i] );
        if ( NULL !=  ref_pos ) {
            pos[i] = 'w';
            *ref_pos = ' '; // remove letter at wrong position from ref
        }
    }
    return 0;
}

/*
    update_solver_data transforms the received raw data into data that is more
    suitable for the solver. The raw data must use the same codes as used in
    get_position_from_words ('r' for exact position, 'w' for wrong position,
    or 'n' for not in word).

    It extracts 4 pieces of information from data:
    - the list of required letters at an exact position: e.g. sd->known="-l--e",
      where both letter and position are available.
    - the list of required letters at a wrong position: e.g. sd->required="a".
    - the list of letters that cannot be at specific positions: e.g. 
      sd->wrong[2]="a", where 2 is the position and 'a' is the letter.
    - the list of letters that cannot be at any position after their required
      number has been already satisfied (from sd->known position if known at an
      exact position or from a position in sd->wrong[] where the letter is not
      listed if found in required): e.g. as->out="st".

    Note that the same letter can appear up to once in sd->known, up to twice in
    sd->required (in the rare case of words with 2 or 3 times the same letter,
    such as "mamma" or "daddy") and up to once in as->out (must be rejected
    after the number of required instances of the same letter have been
    satisfied).
*/
extern solver_data_status update_solver_data( solver_data *sd,
                                              char *data, int *index_at_pos )
{
    assert( sd && data );

    if ( 0 == *data ) return SOLVER_DATA_SET;
    if ( strlen(data) < 10 ) return NON_MODULO_10_DATA_STRING_LENGTH;

    int n_required = strlen( sd->required );
    int n_out = strlen( sd->out );

    char round_required[WORD_SIZE+1];
    int  round_required_count[WORD_SIZE+1];
    memset( round_required, 0, sizeof(char) * (WORD_SIZE+1) );
    memset( round_required_count, 0, sizeof(int) * (WORD_SIZE+1) );
    int n_rr = 0;   // round required count
    
    char *rlp;      // required letter pointer
    int  rli;       // required letter index

    // process one row of 5 letters
    for ( int i = 0; i < (2 * WORD_SIZE); i += 2 ) {
        int li = i + 1;
        int pos = i >> 1;
        if ( data[li] < 'a' || data[li] > 'z' ) {
            printf( "wordle: invalid letter (%c) in data\n", data[li] );
            return INVALID_LETTER_IN_DATA;
        }
        switch ( data[i] ) {
        case 'r' :                              // required at exact position
            if ( sd->known[pos] != data[li] ) {
                // letter was not previously known at exact position
                if ( '-' == sd->known[pos] ) {
                    // no letter was previously known at that position
                    sd->known[pos] = data[li];  // save letter & position
                    // if the new letter at that position was previously in
                    // required, a previous instance must now be subtracted
                    rlp = strchr( sd->required, data[li] );
                    if ( NULL != rlp ) { // remove 1 previous letter instance
                        rli = rlp - sd->required;
                        if ( 0 == --sd->required_count[rli] ) { // count reached 0
                            for ( ; rli < WORD_SIZE; ++rli ) {  // => remove letter
                                sd->required[rli] = sd->required[rli+1];
                                sd->required_count[rli] = sd->required_count[rli+1];
                            }
                            --n_required; // letter was taken out of required
                        }
                    }
                } else {
                    // a different letter was previously known at that position
                    printf( "wordle: different letters (%c) at same position (%d)\n",
                            data[li], pos );
                    return CONFLICTING_EXACT_POSITION_LETTERS;
                }
            }
            break;
        case 'n' :                              // not in word any more.
            if ( sd->known[pos] == data[li] ) {
                printf( "wordle: letter at exact position is also not in word\n" );
                return EXACT_POSITION_LETTER_NOT_IN_WORD;
            }
            if ( NULL == strchr( sd->out, data[li] ) ) {
                sd->out[n_out] = data[li];
                ++ n_out;
            }
            // note that a letter can be known at an exact location and then not
            // in word anymore, for example if the candidate contains repeating
            // letters while the reference contains only 1 instance of that letter
            // such as candidate 'couch' for reference 'chore'. In that case the
            // first 'c' is shown as exact position and the last 'c' as not in
            // word (anymore). The same is true for a letter at a wrong position
            // and not in the word anymore, in a similar example: with 'afoul
            // reference, trying 'canal' gives the 'a' in second position as
            // wrong and the 'a' in fourth position as not in word anymore. In
            // this case, since 'a' is already required, being not in the word
            // anymore must be remembered as a wrong position as well. Finally,
            // yet another case arises with a letter previously known to be at
            // a wrong position and a new candidate shows the same letter at a
            // different location, still wrong, while a second instance of that
            // letter at the previous wrong position is now indicated as not in
            // word anymore.
            if ( NULL != strchr( round_required, data[li] ) &&   // required at
                (NULL == strchr( sd->wrong[pos], data[li] )) ) { // other pos
                    sd->wrong[pos][index_at_pos[pos]] = data[li] ;
                    ++index_at_pos[pos];    // but impossible at this position
                }
            break;
        case 'w':                               // required but at wrong position
            if ( data[li] == sd->known[pos] ) {
                // previously known to be an the same exact location
                printf( "wordle: conflicting wrong and exact position for the same letter (%c)\n",
                        data[li] );
                return WRONG_POSITION_LETTER_IN_EXACT_POSITION;
            }
            // note that a letter may be required but at a wrong position even
            // if it is also required at a different exact location in case of
            // words with the repeating letter: if the reference is "cacao",
            // an attempt with "couch" will give the first 'c' as exact position
            // and the second 'c' as at wrong position. In any case mark letter
            // as impossible at that location
            sd->wrong[pos][index_at_pos[pos]] = data[li] ;
            ++index_at_pos[pos];
            // and keep track of how many times the same letter is required in
            // this round. It give the minimum number of that letter occurences
            // in the word. This will be dealt in a second phase.
            rlp = strchr( round_required, data[li] );
            if ( NULL != rlp ) {                // already seen in this round
                rli = rlp - round_required;     // get index in list
            } else {
                rli = n_rr++;                   // new letter in this round
                round_required[rli] = data[li]; // add it the list
            }
            ++round_required_count[rli];        // remember hown many in round
            break;
        default:
            printf( "wordle: invalid code (%c) in data\n", data[i] );
            return INVALID_CODE_IN_DATA;
        }
    }

    // second pass for required letters at a wrong position in this round
    for ( rli = 0; rli < n_rr; ++rli ) {
        rlp = strchr( sd->required, round_required[rli] );
        if ( NULL == rlp ) {            // new required letter
            if ( WORD_SIZE == n_required ) {
                printf( "wordle: too many required letters at wrong positions\n" );
                return TOO_MANY_WRONG_POSITION_LETTERS;
            }
            sd->required[n_required] = round_required[rli];
            sd->required_count[n_required] = round_required_count[rli];
            ++n_required;
        } else {                        // already known required letter
            int index = rlp - sd->required; // keep the maximum count
            if ( sd->required_count[index] < round_required_count[rli] )
                sd->required_count[index] = round_required_count[rli];
        }
    }
    return SOLVER_DATA_SET;
}

