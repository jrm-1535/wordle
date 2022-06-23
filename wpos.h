
#ifndef __WPOS_H__
#define __WPOS_H__

#include "wsolve.h"

// consume 10 bytes of data at a time, update solver_data known, required, out
// and wrong from consumed data and return an error code if needed.
// Expect sd->out to point to a buffer holding as many letters as possible,
// i.e. (MAX_TRIES*WORD_SIZE) and index_at_pos to point to an array of WORD_SIZE
// int, initialized to 0 before the first call.
extern solver_data_status update_solver_data( solver_data *sd,
                                              char *data, int *index_at_pos );

// for each letter in word, set position as '-' if the letter is not in ref,
// 'w' if it is in ref but not at the right position and 'r' if it is at the
// same position in ref. Returns -1 if the word does not exist in dictionary.
// Expect pos to point to an array of WORD_SIZE+1 bytes
extern int get_position_from_words( const char *ref, const char *word,
                                    char *pos );

#endif /* __WPOS_H__ */
