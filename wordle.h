
#ifndef __WORDLE_H__
#define __WORDLE_H__

typedef struct _word_node {
    struct _word_node *next;
    const char        *word;
} word_node;

// global definitions
#define WORDLE_DICTIONARY       "dict.txt"
#define MAX_TRIES               6
#define WORD_SIZE               5
#define ALPHABET_SIZE           26

#define MAX_WORD_NUMBER         10000
#define MAX_WORD_HASH_ENTRIES   65521   // only about 3% collisions

// playground colors
#define GREEN_BG    "\x1b[30;1;42m"
#define YELLOW_BG   "\x1b[30;1;43m"
#define RED_BG      "\x1b[30;1;41m"
#define BLUE_BG     "\x1b[30;1;44m"

#define UP          "\x1b[1A"

#define DEFAULT     "\x1b[0m     "
#define NORMAL      "\x1b[0;39m\n"

// starting word sequences
#define MAX_SEARCH_DEPTH    5
#define MAX_SEARCH_WIDTH    3

#define MAX_SEQUENCE_DEPTH  3

#endif /* __WORDLE_H__ */
