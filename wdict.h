
#ifndef __WDICT_H__
#define __WDICT_H__

#include <stdbool.h>
#include "wordle.h"

// load wordle dictionary in memory
extern void load_dictionary( const char *path );

// unload the whole wordle dictionary
extern void discard_dictionary( void );

// return a pointer to the word in dictionary or NULL if the given word
// does not exist in the dictionary.
extern const char *get_word_in_dictionary( const char *word );

// return true if the word exists in the dictionary
extern bool is_word_in_dictionary( const char *word );

// return the number of words in the dictionary.
extern int  get_dictionary_size( void );

// return the single word at the given index in dictionary. The result must
// NOT be freed by the caller, since it points directly in the dictionary.
// It will be freed with all others words in the dictionary when calling
// discard_dictionary.
extern const char *get_nth_word_in_dictionary( int index );

// return the list of words in dictionary that do not include any letter in
// a given string (of any length). The result is a word_node list that must
// be freed by the caller after use, by calling free_word_list
extern word_node *get_all_words_in_dict_not_sharing_letters( const char *letters );

// return the number of words in a list
extern size_t get_word_count( word_node *list );

// free a word_node list. The actual words in the dictionary are not deleted
// and will stay available until discard_dictionary is called.
extern void free_word_list( word_node *list );
 
typedef void (*do_fct)( void *ctxt, const char *word );
extern void for_each_word_in_dictionary( do_fct f, void *ctxt );

typedef struct {
    int n_collisions;
    int max_collision_chain;
} collisions;

extern void get_dictionary_table_collisions( collisions *c );

#endif /* __WDICT_H__ */
