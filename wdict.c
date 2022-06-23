
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "wdict.h"

/*
    With WORD_SIZE and ALPHABET_SIZE being so small, it is possible to
    quickly compute a unique hash for each word: 32 ^ 5 (33,554,432),
    which can then used as an index modulo the word table size. This
    hash value fits easily in 32 bits (4,294,967,296)
*/
typedef struct _dict_node {
    struct _dict_node *next;    // list only in case of modulo collisions
    uint32_t          hash;
    char              *word;
} dict_node;

static dict_node *dict_table[MAX_WORD_HASH_ENTRIES];
static char *word_table[MAX_WORD_NUMBER];
static int  n_dict_words;

static uint32_t get_hash( const char *word )
{
    uint32_t hash = 0;
    for ( int i = 0; i < WORD_SIZE; ++i ) {
        hash |= word[i] - 'a';  // reduce from char (8 bits) to [0-25] (5 bits)
        hash <<= 5;
    }
    return hash;
}

static void insert_word( char *word )
{
    uint32_t hash = get_hash( word );
    uint32_t index = hash % (uint32_t)MAX_WORD_HASH_ENTRIES;
    dict_node *dn = malloc( sizeof( dict_node ) );
    assert( dn );
    dn->hash = hash;
    dn->word = word;

    if ( NULL == dict_table[index] ) {
        dn->next = NULL;
    } else {
        dn->next = dict_table[index];
    }
    dict_table[index] = dn;
    word_table[n_dict_words] = word;
    if ( ++n_dict_words >= MAX_WORD_NUMBER ) {
        printf("Dictionary too large to fit in memory\n");
        exit(1);
    }
}

extern int  get_dictionary_size( void )
{
    return n_dict_words;
}

extern const char *get_word_in_dictionary( const char *word )
{
    uint32_t hash = get_hash( word );
    uint32_t index = hash % (uint32_t)MAX_WORD_HASH_ENTRIES;

    for ( dict_node *dn = dict_table[index]; dn; dn = dn->next ) {
        if ( hash == dn->hash ) return dn->word;
    }
    return NULL;
}

extern bool is_word_in_dictionary( const char *word )
{
    uint32_t hash = get_hash( word );
    uint32_t index = hash % (uint32_t)MAX_WORD_HASH_ENTRIES;

    for ( dict_node *dn = dict_table[index]; dn; dn = dn->next ) {
        if ( hash == dn->hash ) return true;
    }
    return false;
}

typedef struct _dict_iterator {
    int         index;
} dict_iterator;

static void reset_iterator( dict_iterator *d )
{
    d->index = -1;
}

static inline char *iterate_word_in_dictionary( dict_iterator *d )
{
    if ( ++d->index < n_dict_words ) {
        return word_table[d->index];
    }
    return NULL;
}

extern void for_each_word_in_dictionary( do_fct f, void *c )
{
    dict_iterator di;
    reset_iterator( &di );

    while ( true ) {
        char *word = iterate_word_in_dictionary( &di );
        if ( NULL == word )
            break;          // end iteration

        f( c, word );
    }
}

extern const char *get_nth_word_in_dictionary( int index )
{
    if ( index >= 0 && index < n_dict_words ) {
        return word_table[index];
    }
    return NULL;
}

// note that one can have any length, but two must be exactly WORD_SIZE
static bool do_words_share_letters( const char *one, const char *two )
{
    for ( int i = 0; i < WORD_SIZE; ++i ) {
        if ( NULL != strchr( one, two[i] ) ) {
            return true;
        }
    }
    return false;
}

extern word_node *get_all_words_in_dict_not_sharing_letters( const char *letters )
{
    word_node *root = NULL;

    dict_iterator di;
    reset_iterator( &di );
    while ( true ) {
        char *word = iterate_word_in_dictionary( &di );
        if ( NULL == word )
            break;          // end iteration

        if ( NULL != letters && do_words_share_letters( letters, word ) )
            continue;       // skip words sharing letters

        word_node *wn = malloc( sizeof( word_node ) );
        wn->word = word;
        wn->next = root;
        root = wn;
    }
    return root;
}

extern size_t get_word_count( word_node *words )
{
    size_t count = 0;
    for ( word_node *wn = words; wn; wn = wn->next ) ++count;
    return count;
}

extern void free_word_list( word_node *words )
{
    word_node *next;
    for ( word_node *wn = words; wn; wn = next ) {
        next = wn->next;
        wn->next = NULL;
        wn->word = NULL;
        free( wn );
    }
}

#if 0 // not used
extern word_node *get_all_words_in_dict( void )
{
    word_node *root = NULL;

    dict_iterator di;
    reset_iterator( &di );
    while ( true ) {
        char *word = iterate_word_in_dictionary( &di );
        if ( NULL == word )
            break;

        word_node *wn = malloc( sizeof( word_node ) );
        wn->word = word;
        wn->next = root;
        root = wn;
    }
    return root;
}

static void get_dictionary_table_collisions( collisions *c )
{
    c->n_collisions = 0;
    c->max_collision_chain = 0;

    for ( int i = 0; i < MAX_WORD_HASH_ENTRIES; ++i ) {
        if ( NULL != dict_table[i] ) {
            int chain = 0;
            for (  dict_node *dn = dict_table[i]; dn; dn = dn->next ) {
                ++chain;
            }
            if ( chain > 1 ) {
                ++c->n_collisions;
                if ( c->max_collision_chain < (chain-1) )
                    c->max_collision_chain = chain-1;
            }
        }
    }
}

extern void print_collisions( void )
{
    collisions col_stats;
    get_dictionary_table_collisions( &col_stats );
    printf("Number of collisions: %d max collision chain: %d\n",
            col_stats.n_collisions, col_stats.max_collision_chain );
}
#endif
extern void load_dictionary( const char *path )
{
    FILE *f = fopen( path, "rb" );
    if ( NULL == f ) {
        printf( "Failed to open dictionary file %s exiting\n", path );
        exit(1);
    }

    while ( ! feof( f ) ) {
        char *word = malloc( WORD_SIZE+1 );
        if ( 1 != fscanf( f, "%5s", word ) ) {
            free(word);
            break;
        }
        // assumes words are in lower case, only [a-z] chars
        insert_word( word );
    }
    fclose( f );
}

extern void discard_dictionary( void )
{
    for ( int index = 0; index < MAX_WORD_HASH_ENTRIES; ++index ) {
        dict_node *next;
        for ( dict_node *dn = dict_table[index]; dn; dn = next ) {
            next = dn->next;
            free( dn->word );
            free( dn );
        }
        dict_table[index] = NULL;
    }
    for ( int index = 0; index < n_dict_words; ++index ) {
        word_table[index] = NULL;
    }
    n_dict_words = 0;
}

