
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "wstats.h"
#include "wdict.h"

typedef struct {
    // words are listed in an array of array of lists, such that for each
    // position in a word [0-4] there is a separate array of list of words
    // containing each possible letter [a-z] at that position:
    word_node *letter_pos[ALPHABET_SIZE][WORD_SIZE]; // words with letter @ position

    // a separate array keeps the number of words in each list
    int n_letter_pos[ALPHABET_SIZE][WORD_SIZE];      // n words with letter @ position
} word_stats;

static void init_word_stats( word_stats *ws )
{
    memset( ws->letter_pos, 0, sizeof( word_node *) * ALPHABET_SIZE * WORD_SIZE );
    memset( ws->n_letter_pos, 0, sizeof(int) * ALPHABET_SIZE * WORD_SIZE );
}

static void analyze_word( void *ctxt, char *word )
{
    word_stats *ws = ctxt;
    for ( int k = 0; k < WORD_SIZE; ++k ) {
        int j = word[k] - 'a';
        assert( j < ALPHABET_SIZE );
        word_node *prev = NULL;
        for ( word_node *cur = ws->letter_pos[j][k]; cur; prev = cur, cur = cur->next );
        word_node *node = malloc( sizeof( word_node ) );
        node->next = NULL;
        node->word = word;
        if ( NULL == prev ) {
            ws->letter_pos[j][k] = node;
        } else {
            assert( NULL == prev->next );
            prev->next = node;
        }
        ++ws->n_letter_pos[j][k];
    }
}

static void discard_word_stats( word_stats *ws )
{
    for ( int k = 0; k < WORD_SIZE; ++k ) {
        for ( int i = 0; i < ALPHABET_SIZE; ++i ) {
            word_node *next;
            for ( word_node *cur = ws->letter_pos[i][k]; cur; cur = next ) {
                next = cur->next;
                cur->next = NULL;
                free( cur );
            }
        }
    }
    memset( ws->letter_pos, 0, sizeof( word_node *) * ALPHABET_SIZE * WORD_SIZE );
    memset( ws->n_letter_pos, 0, sizeof(int) * ALPHABET_SIZE * WORD_SIZE );
}

static void set_word_stats_from_dict( word_stats *ws )
{
    init_word_stats( ws );
    for_each_word_in_dictionary( analyze_word, (void *)ws );
}

typedef struct _starting_word {
    struct _starting_word   *next,      // NULL terminated forward chain
                            *prev,      // circular backward chain
                            *follow;    // subsequent words
    int                     weight;
    char                    *word;
} starting_word;

static void free_starting_words( starting_word *root )
{
    while ( root ) {
        starting_word *next = root->next;
        free_starting_words( root->follow );
        root->follow = NULL;
        root->next = NULL;
        root->prev = NULL;
        free( root );
        root = next;
    }
}

static void indent( int n )
{
    for ( int i = 0; i < n; ++i ) {
        fputs( "  ", stdout );
    }
}

static void print_starting_words( starting_word *root, int weight,
                           int depth, int max_depth )
{
    if ( 0 == depth ) {
        printf( "\nBest starting words:" );
    } else if ( max_depth == depth ) {
        printf( " total weight: %d", weight );
        return;
    }

    for ( starting_word *sw = root; sw; sw = sw->next ) {
        if ( NULL == sw->word )
            break;
        printf( "\n" );
        indent( depth );
        printf( "  %s (weight %d)", sw->word, sw->weight );
        if ( sw->follow ) {
            print_starting_words( sw->follow, sw->weight + weight,
                                  depth + 1, max_depth );
        } else {
            printf( " total weight: %d (sequence complete)", sw->weight + weight );
        }
    }
}

typedef struct {
    char *out;
    int  depth;
    int  length;
} starting_context;

static starting_word *get_best_starting_words( starting_context *sc, word_stats *ws )
{
    word_node *list = get_all_words_in_dict_not_sharing_letters( sc->out );

    if ( NULL == list ) return NULL;

    // for each word compute letter at position count
    starting_word *root = NULL, *prev = NULL, *last;
    for ( int i = 0; i < sc->length; ++i ) {
        starting_word *sw = malloc( sizeof( starting_word ) );
        assert( sw );
        sw->weight = 0;
        sw->word = NULL;
        sw->follow = NULL;
        sw->next = root;
        if ( prev ) {
            prev->prev = sw;
        } else {
            last = sw;
        }
        root = sw;
        prev = sw;
    }
    root->prev = last;

    for ( word_node *wn = list; wn; wn = wn->next ) {
        int weight = 0;
//        printf( "  %s:", wn->word );
        for ( int k = 0; k < WORD_SIZE; ++k ) {
//            printf( " %c=%d", wn->word[k], n_letter_pos[wn->word[k]-'a'][k] );
            weight += ws->n_letter_pos[wn->word[k]-'a'][k];
        }
//        printf( " => weight=%d\n", weight );
        for ( starting_word *cur = root; cur; cur = cur->next ) {
            if ( weight > cur->weight ) {
                // try reuse last in list, insert in front of cur if not the same
                starting_word *last = root->prev;
                assert( last->next == NULL );
                assert( last->prev != NULL );

                if ( cur != last ) {
                    last->next = cur;           // move above cur
                    last->prev->next = NULL;    // update forward chain

                    if ( cur != root ) {        // update backward chain
                        root->prev = last->prev;
                        last->prev = cur->prev;
                        cur->prev->next = last;
                        cur->prev = last;
                    } else {
                        root = last;            // no backaward chain change
                    }
                }

                assert( root->prev->next == NULL );
                assert( root->prev->prev != NULL );

                last->weight = weight;
                last->word = wn->word;
                break;
            }
        }
    }

    if ( sc->depth > 1 ) {
        starting_context ncontext;
        ncontext.depth = sc->depth - 1;
        ncontext.length = sc->length;
        ncontext.out = malloc( strlen( sc->out ) + WORD_SIZE + 1 );
        strcpy( ncontext.out, sc->out );
//        int n = 0;
        for ( starting_word *cur = root; cur; cur = cur->next ) {
            if ( NULL == cur->word ) break;
            strcpy( &ncontext.out[strlen(sc->out)], cur->word );

//            printf( " depth %d word #%d: %s @ weight=%d getting followers (out %s)\n",
//                      sc->depth, n, cur->word, cur->weight, ncontext.out );
//            ++n;

            cur->follow = get_best_starting_words( &ncontext, ws );
        }
        free( ncontext.out );
    }
    free_word_list( list );
    return root;
}

extern char *select_most_likely_word( word_node *list )
{
    word_stats ws;
    init_word_stats( &ws );
    int n = 0;
    for ( word_node *wn = list; wn; wn = wn->next ) {
        analyze_word( &ws, wn->word );
        ++n;
    }
    char *best = NULL;
    if ( n > 2 ) {  // otherwise no choice or equal probability
        // for each word compute letter at position count
        int max_weight = 0;

        for ( word_node *wn = list; wn; wn = wn->next ) {

            int weight = 0;
    //        printf( "  %s:", wn->word );
            for ( int k = 0; k < WORD_SIZE; ++k ) {
    //            printf( " %c=%d", wn->word[k], n_letter_pos[wn->word[k]-'a'][k] );
                weight += ws.n_letter_pos[wn->word[k]-'a'][k];
            }
    //        printf( " => weight=%d\n", weight );
            if ( weight > max_weight ) {
                max_weight = weight;
                best = wn->word;
            }
        }
//        printf( "Best: %s @ weight %d\n", best, max_weight );
    }
    discard_word_stats( &ws );
    return best;
}

typedef struct {
    int pos_sorted_letter[ALPHABET_SIZE][WORD_SIZE];
    int pos_sorted_count[ALPHABET_SIZE][WORD_SIZE];
    int global_n[ ALPHABET_SIZE ];
} letter_rank;

// this assumes that word_stats is already populated
static void sort_letter_pos( word_stats *wsp, letter_rank *lr )
{
    memset( lr->pos_sorted_letter, 0, sizeof(int) * ALPHABET_SIZE * WORD_SIZE );
    memset( lr->pos_sorted_count, 0, sizeof(int) * ALPHABET_SIZE * WORD_SIZE );
    memset( lr->global_n, 0, sizeof(int) * ALPHABET_SIZE );

    for ( int k = 0; k < WORD_SIZE; ++k ) { // for each position
        int rank = 0;
        int letter_n[ALPHABET_SIZE] = { 0 }; 
        for ( int j = 0; j < ALPHABET_SIZE; ++j ) { // for each letter @pos
            letter_n[j] = wsp->n_letter_pos[j][k];  // n words with letter @pos
        }
        while ( true ) {                            // sort by frequency
            int highest_n = 0;
            int highest_l = 0;                      // inefficient n^2
            for ( int j = 0; j < ALPHABET_SIZE; ++j ) {
                if ( highest_n < letter_n[j] ) {
                    highest_n = letter_n[j];
                    highest_l = j;
                }
            }
            if ( 0 == highest_n ) {
                break;
            }
//            printf( "  %c: %.2f%%\n", 'a'+ highest_l,
//                               (double)(highest_n*100)/(double)n_words );

            lr->pos_sorted_letter[rank][k] = 'a' + highest_l;
            lr->pos_sorted_count[rank++][k] = highest_n;
            lr->global_n[ highest_l ] += highest_n;

            letter_n[highest_l] = 0;    // remove from list
        }
    }
}

typedef struct  {
    char *word;
    int  n_letters;
    int  count;
} repeat;

typedef struct {
    repeat max_repeats[ALPHABET_SIZE];
    int    index;
} repeat_stats;

static void update_repeat( void *ctxt, char *word )
{
    repeat_stats *rs = ctxt;

    int r = 0;
    for ( int k = 0; k < WORD_SIZE; ++k ) {
        if ( word[k] == 'a' + rs->index ) {
            ++r;
        }
    }
    if ( rs->max_repeats[rs->index].n_letters < r ) {
        rs->max_repeats[rs->index].n_letters = r;
        rs->max_repeats[rs->index].count = 1;
        rs->max_repeats[rs->index].word = word;
    } else if ( rs->max_repeats[rs->index].n_letters == r ) {
        ++rs->max_repeats[rs->index].count;
    }
}

extern void print_letter_stats( void )
{
    word_stats stats, *wsp = &stats;
    set_word_stats_from_dict( wsp );

    letter_rank lr;
    int n_words = get_dictionary_size();
    printf( "%d words in dictionary\n", n_words );

    sort_letter_pos( wsp, &lr );

    for ( int k = 0; k < WORD_SIZE; ++k ) {
        printf( "Frequency of letters appearing in position %d:\n", k );
        for ( int rank = 0; rank < ALPHABET_SIZE; ++rank ) {
            if ( lr.pos_sorted_letter[rank][k] == 0 )
                break;

            int highest_n = lr.pos_sorted_count[rank][k];
            printf( "  %c: %.2f%%\n", lr.pos_sorted_letter[rank][k],
                             (double)(highest_n*100)/(double)n_words );
        }
        printf( "\n" );
    }

    printf( "Global frequencies:\n" );
    int total = 0;
    for ( int i = 0; i < ALPHABET_SIZE; ++i ) {
        total += lr.global_n[i];
    }
    double check = 0;
    while ( true ) {
        int highest_n = 0;
        int highest_l = 0;
        for ( int j = 0; j < ALPHABET_SIZE; ++j ) {
            if ( highest_n < lr.global_n[j] ) {
                highest_n = lr.global_n[j];
                highest_l = j;
            }
        }
        if ( 0 == highest_n ) {
            break;
        }
        printf( "  %c: %.2f%%\n", 'a'+ highest_l,
                             (double)(highest_n*100) / (double)total );
        check += (double)highest_n / (double)total;
        lr.global_n[highest_l] = 0;
    }
//    printf( "\nChecking frequency sums: %f\n", check );

    repeat_stats rs;
    memset( rs.max_repeats, 0, sizeof(repeat) * ALPHABET_SIZE );

    for ( rs.index = 0; rs.index < ALPHABET_SIZE; ++rs.index ) {
        for_each_word_in_dictionary( update_repeat, &rs );
    }
    printf( "\nMax repeats:\n");
    for ( int i = 0; i < ALPHABET_SIZE; ++i ) {
        if ( rs.max_repeats[i].n_letters > 1 ) {
            printf( " letter %c appears %d times in %d word(s) (e.g. %s)\n",
                    'a' + i, rs.max_repeats[i].n_letters,
                    rs.max_repeats[i].count, rs.max_repeats[i].word );
        }
    }

    starting_context sc;
    sc.out = "";
    sc.depth = MAX_SEARCH_DEPTH;
    sc.length = MAX_SEARCH_WIDTH;

    starting_word *root = get_best_starting_words( &sc, wsp );
    print_starting_words( root, 0, 0, MAX_SEQUENCE_DEPTH );
    printf("\n");

    free_starting_words( root );
    discard_word_stats( wsp );
}

