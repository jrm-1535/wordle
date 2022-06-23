
#ifndef __WSOLVE_H__
#define __WSOLVE_H__

typedef struct {
    char *out;
    char *wrong[WORD_SIZE];
    int  required_count[WORD_SIZE+1];   // must have same size as required
    char required[WORD_SIZE+1];
    char known[WORD_SIZE+1];

} solver_data;

// data is given as an array of sets, each of 5 couples { code, letter }
// both codes and letters are char; 3 codes are possible: 'k','w','n'
// for 'known position', 'wrong position' and 'not in word'.
// e.g. nsnlnantwenfkenvkekr
// solver data must have been initialized before the call to preprocess it:
// typical call sequence:
// solver_data given;
// init_solver_data( &given );
// for ( ; ; ) {
//    <obtain new data or break if none>
//    set_solver_data( &given, data );
//    res = get_solutions( &given)
//    reset_solver_data( &given );
// }
// discard_solver_data( &given );

extern void init_solver_data( solver_data *data );

typedef enum {
    SOLVER_DATA_SET,

    NON_MODULO_10_DATA_STRING_LENGTH, 
    DATA_STRING_LENGTH_TOO_LARGE,
    INVALID_CODE_IN_DATA,
    INVALID_LETTER_IN_DATA,

    CONFLICTING_EXACT_POSITION_LETTERS,
    EXACT_POSITION_LETTER_NOT_IN_WORD,
    WRONG_POSITION_LETTER_IN_EXACT_POSITION,
    WRONG_POSITION_LETTER_NOT_IN_WORD,
    TOO_MANY_WRONG_POSITION_LETTERS

} solver_data_status;

extern solver_data_status set_solver_data( solver_data *given, char *data );
extern void print_solver_data( solver_data *given );

extern void reset_solver_data( solver_data *data );
extern void discard_solver_data( solver_data *data );

// get_solutions returns a list of word nodes allocated on the heap
// after use this list must be freed by the caller (free_node_list).
extern word_node *get_solutions( solver_data *given );

#endif /* __WSOLVE_H__ */
