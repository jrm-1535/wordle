
#include "wordle.h"

// printout letter statistics from the wordle dictionary
// dictionary must be loaded before...
extern void print_letter_stats( );

// given a list of words, calculate letter statistics and select the
// "most likely" word in the list, that is the word where letters have
// the hihest propbability to appread at the right position. That does
// not give necessarily the winning choice, but that will provide more
// information than any other word if it is not. If the list has less
// than 3 items, it returns NULL (no word at all, only 1 word or equal
// probabilities). If the list has 3 or more items, a single most likely
// word is always returned, even if multiple words have the same probability.
// The returned string points into the given word list. It is up to the
// caller to free the list after use.
extern const char *select_most_likely_word( word_node *list );
