
#include "red_black_tree.h"
#include "red_black_node.h"

#include <core/system/memory_pool.h>
#include <core/system/debugger.h>

#include <stdlib.h>
#include <string.h>

/*
#define RUN_TREE_ASSERTIONS
*/

void bsal_red_black_tree_init(struct bsal_red_black_tree *self, int key_size, int value_size)
{
    self->root = NULL;
    self->memory_pool = NULL;
    self->size = 0;

    self->key_size = key_size;
    self->value_size = value_size;

    self->compare = bsal_red_black_tree_compare_memory_content;

    self->cached_last_node = NULL;
    self->cached_lowest_node = NULL;
}

void bsal_red_black_tree_destroy(struct bsal_red_black_tree *self)
{
    if (self->root != NULL) {
        bsal_red_black_tree_free_node(self, self->root);
        self->root = NULL;
    }

    self->memory_pool = NULL;
    self->size = 0;
    self->key_size = 0;
    self->value_size = 0;
}

void *bsal_red_black_tree_add_key_and_value(struct bsal_red_black_tree *self, void *key, void *value)
{
    void *new_value;

    new_value = bsal_red_black_tree_add(self, key);
    bsal_memory_copy(new_value, value, self->value_size);

    return new_value;
}

void *bsal_red_black_tree_add(struct bsal_red_black_tree *self, void *key)
{
    struct bsal_red_black_node *node;
    struct bsal_red_black_node *current_node;
    struct bsal_red_black_node *left_node;
    struct bsal_red_black_node *right_node;
    int result;
    int inserted;

    node = bsal_memory_pool_allocate(self->memory_pool, sizeof(struct bsal_red_black_node));

    self->cached_last_node = node;

    bsal_red_black_node_init(node, self->key_size, key, self->value_size, NULL, self->memory_pool);

    if (self->root == NULL) {
        self->root = node;
        bsal_red_black_tree_insert_case1(self, node);

        /*
         * This is the first item.
         * So this is the lowest key.
         */
        self->cached_lowest_node = node;

        ++self->size;
        return node->value;
    }

    current_node = self->root;
    inserted = 0;

    while (inserted == 0) {

        result = bsal_red_black_tree_compare(self, node->key, current_node->key);

        if (result < 0) {

            left_node = bsal_red_black_node_left_node(current_node);

            if (left_node == NULL) {
                current_node->left_node = node;
                node->parent = current_node;
                ++self->size;
                bsal_red_black_tree_insert_case1(self, node);
                inserted = 1;
            } else {
                current_node = left_node;
            }
        } else /* if (bsal_red_black_node_key(node) < bsal_red_black_node_key(current_node)) */ {

            right_node = bsal_red_black_node_right_node(current_node);

            if (right_node == NULL) {
                current_node->right_node = node;
                node->parent = current_node;
                ++self->size;
                bsal_red_black_tree_insert_case1(self, node);
                inserted = 1;
            } else {
                current_node = right_node;
            }
        }
    }

#ifdef RUN_TREE_ASSERTIONS
    /*
     * If the current node is RED, then the parent must be black
     */
    if (node->color == BSAL_COLOR_RED) {
        if (node->parent != NULL) {
            BSAL_DEBUGGER_ASSERT(node->parent->color == BSAL_COLOR_BLACK);
        }
    }

    bsal_red_black_node_run_assertions(node, self);
#endif

    /*
     * Maintain the cache for the lowest key
     * To do so, we just need to check if the inserted key is lower than
     * the key for cached_lowest_node.
     *
     * This is O(1).
     */

    if (self->cached_lowest_node == NULL
              || bsal_red_black_tree_compare(self, node->key, self->cached_lowest_node->key)) {

        self->cached_lowest_node = node;
    }

    return node->value;
}

void bsal_red_black_tree_delete(struct bsal_red_black_tree *self, void *key)
{
    /*
     * Nothing to delete.
     */
    if (self->size == 0) {
        return;
    }

    /*
     * Update the lowest value cache.
     */

    BSAL_DEBUGGER_ASSERT(self->cached_lowest_node != NULL);

    /*
     * If we are deleting the loewst key, the new lowest key
     * is the parent node for this key (which might be NULL).
     */
    if (bsal_red_black_tree_compare(self, key, self->cached_lowest_node->key) == 0) {
        self->cached_lowest_node = self->cached_lowest_node->parent;
    }

    /*
     * Delete the node.
     */

    --self->size;
}

/*
 * This function checks the rules.
 *
 * \see http://en.wikipedia.org/wiki/Red%E2%80%93black_tree
 *
 * The 5 rules are:
 *
 * 1. A node is red or black.
 * 2. The root is black.
 * 3. All leaf nodes are black.
 * 4. Any red node has 2 black child nodes.
 * 5. Every path from given node to any of its descendant leaf node contains
 *    the same number of black nodes.
 */
int bsal_red_black_tree_has_ignored_rules(struct bsal_red_black_tree *self)
{
    if (self->root == NULL) {
        return 0;
    }

    if (bsal_red_black_node_color(self->root) != BSAL_COLOR_BLACK) {
        printf("Rule 2 is ignored !\n");
        return 1;
    }

    return 0;
}

void bsal_red_black_tree_set_memory_pool(struct bsal_red_black_tree *self,
                struct bsal_memory_pool *memory_pool)
{
    self->memory_pool = memory_pool;
}

int bsal_red_black_tree_size(struct bsal_red_black_tree *self)
{
    return self->size;
}

void bsal_red_black_tree_free_node(struct bsal_red_black_tree *self,
                struct bsal_red_black_node *node)
{
    struct bsal_red_black_node *left_node;
    struct bsal_red_black_node *right_node;

    if (node == NULL) {
        return;
    }

#if 0
    printf("free_node\n");
#endif

    left_node = bsal_red_black_node_left_node(node);
    right_node = bsal_red_black_node_right_node(node);

    bsal_red_black_tree_free_node(self, left_node);
    bsal_red_black_tree_free_node(self, right_node);

    bsal_red_black_node_destroy(node, self->memory_pool);

#if 0
    printf("free %p\n", (void *)node);
#endif
    bsal_memory_pool_free(self->memory_pool, node);
}

/*
 * Case 1: the node is the root.
 *
 * \see http://en.wikipedia.org/wiki/Red%E2%80%93black_tree
 */
void bsal_red_black_tree_insert_case1(struct bsal_red_black_tree *self,
                struct bsal_red_black_node *node)
{
    if (node->parent == NULL) {
        /*
         * Adding at the root adds one black node to all paths.
         */
        bsal_red_black_node_set_color(self->root, BSAL_COLOR_BLACK);
    } else {
        bsal_red_black_tree_insert_case2(self, node);
    }
}

/*
 * Case 2: the color of the parent of the node is black.
 *
 * \see http://en.wikipedia.org/wiki/Red%E2%80%93black_tree
 *
 * Nothing to do.
 */
void bsal_red_black_tree_insert_case2(struct bsal_red_black_tree *self,
                struct bsal_red_black_node *node)
{
    if (node->parent->color == BSAL_COLOR_BLACK) {
        return;
    } else {

        bsal_red_black_tree_insert_case3(self, node);
    }
}

/*
 * Case 3: node is red, parent is red, grandparent is black and uncle is red.
 *
 * \see http://en.wikipedia.org/wiki/Red%E2%80%93black_tree
 */
void bsal_red_black_tree_insert_case3(struct bsal_red_black_tree *self,
                struct bsal_red_black_node *node)
{
    struct bsal_red_black_node *parent;
    struct bsal_red_black_node *grandparent;
    struct bsal_red_black_node *uncle;

    uncle = bsal_red_black_node_uncle(node);

    if (uncle != NULL && uncle->color == BSAL_COLOR_RED) {
        parent = bsal_red_black_node_parent(node);

        BSAL_DEBUGGER_ASSERT(node->color == BSAL_COLOR_RED);

        parent->color = BSAL_COLOR_BLACK;
        uncle->color = BSAL_COLOR_BLACK;
        grandparent = bsal_red_black_node_grandparent(node);

        BSAL_DEBUGGER_ASSERT(grandparent->color == BSAL_COLOR_BLACK);

        grandparent->color = BSAL_COLOR_RED;

        bsal_red_black_tree_insert_case1(self, grandparent);
    } else {
        bsal_red_black_tree_insert_case4(self, node);
    }
}

/*
 * Case 4: the parent is red, but the uncle is black
 *
 * \see http://en.wikipedia.org/wiki/Red%E2%80%93black_tree
 */
void bsal_red_black_tree_insert_case4(struct bsal_red_black_tree *self,
                struct bsal_red_black_node *node)
{
    struct bsal_red_black_node *grandparent;
    struct bsal_red_black_node *parent;

#if DEBUG_TREE
    printf("insert_case4 node %d\n", node->key);
    bsal_red_black_tree_print(self);
#endif

    grandparent = bsal_red_black_node_grandparent(node);
    parent = bsal_red_black_node_parent(node);

    if (node == parent->right_node && parent == grandparent->left_node) {

        bsal_red_black_tree_rotate_left(self, parent);

#ifdef DEBUG_TREE
        printf("insert_case4 rotate_left %d\n", parent->key);
#endif

        node = node->left_node;

    } else if (node == parent->left_node && parent == grandparent->right_node) {
        bsal_red_black_tree_rotate_right(self, parent);

        node = node->right_node;
    }

#ifdef DEBUG_TREE
    printf("Before call to insert_case5\n");
    bsal_red_black_tree_print(self);
#endif
    bsal_red_black_tree_insert_case5(self, node);
}

/*
 * Case 5.
 *
 * \see http://en.wikipedia.org/wiki/Red%E2%80%93black_tree
 */
void bsal_red_black_tree_insert_case5(struct bsal_red_black_tree *self,
                struct bsal_red_black_node *node)
{
    struct bsal_red_black_node *grandparent;
    struct bsal_red_black_node *parent;

#ifdef DEBUG_TREE
    printf("insert_case5 %d\n", node->key);
#endif

    grandparent = bsal_red_black_node_grandparent(node);
    parent = bsal_red_black_node_parent(node);

    parent->color = BSAL_COLOR_BLACK;
    grandparent->color = BSAL_COLOR_RED;

    if (node == parent->left_node) {
        bsal_red_black_tree_rotate_right(self, grandparent);
    } else {
        bsal_red_black_tree_rotate_left(self, grandparent);
    }
}

/*
 * Left rotation around N (order: C N E D F G)
 *
 *          G
 *      N
 *    C   D
 *       E F
 *
 * (N can also be the right_node of G).
 *
 * becomes
 *
 *          G
 *      D
 *    N   F
 *   C E
 *
 * Changes:
 */
void bsal_red_black_tree_rotate_left(struct bsal_red_black_tree *self,
                struct bsal_red_black_node *node)
{
    struct bsal_red_black_node *node_N;
    struct bsal_red_black_node *node_G;
    struct bsal_red_black_node *node_D;
    struct bsal_red_black_node *node_E;

#ifdef DEBUG_TREE
    printf("before rotate_left rotate_left node %d\n", node->key);
    bsal_red_black_tree_print(self);
#endif

    /*
     * List:
     *
     * C -> N [x]
     * N -> E [x]
     */
    node_N = node;
    node_G = node_N->parent;
    node_D = node_N->right_node;
    node_E = node_D->left_node;

    node_N->right_node = node_E;
    if (node_E != NULL) {
        node_E->parent = node_N;
    }

    node_D->left_node = node_N;
    node_N->parent = node_D;

    if (node_G != NULL) {
        if (node_G->left_node == node_N)
            node_G->left_node = node_D;
        else
            node_G->right_node = node_D;
    } else {
        self->root = node_D;
    }
    node_D->parent = node_G;

#ifdef RUN_TREE_ASSERTIONS
    bsal_red_black_node_run_assertions(node_D, self);
    bsal_red_black_node_run_assertions(node_N, self);
    bsal_red_black_node_run_assertions(node_G, self);
    bsal_red_black_node_run_assertions(node_E, self);
    bsal_red_black_node_run_assertions(self->root, self);
#endif
}

/*
 * Right rotation around N (order is: G F D E N C)
 *
 * Before
 *
 *          G
 *              N
 *            D   C
 *           F E
 *
 * (N can also be the left_node of G).
 *
 * After
 *
 *
 *          G
 *             D
 *            F  N
 *              E C
 */
void bsal_red_black_tree_rotate_right(struct bsal_red_black_tree *self,
                struct bsal_red_black_node *node)
{
    struct bsal_red_black_node *node_N;
    struct bsal_red_black_node *node_G;
    struct bsal_red_black_node *node_D;
    struct bsal_red_black_node *node_E;

#if 0
    printf("before rotate_right node %d\n", node->key);
    bsal_red_black_tree_print(self);
#endif

    node_N = node;
    node_G = node_N->parent;
    node_D = node_N->left_node;
    node_E = node_D->right_node;

    node_N->left_node = node_E;
    if (node_E != NULL) {
        node_E->parent = node_N;
    }

    node_D->right_node = node_N;
    node_N->parent = node_D;

    if (node_G != NULL) {
        if (node_G->right_node == node_N)
            node_G->right_node = node_D;
        else
            node_G->left_node = node_D;
    } else {
        self->root = node_D;
    }
    node_D->parent = node_G;

#if 0
    printf("Before assertions\n");
    bsal_red_black_tree_print(self);
#endif
#ifdef RUN_TREE_ASSERTIONS
    bsal_red_black_node_run_assertions(node_N, self);
    bsal_red_black_node_run_assertions(node_G, self);
    bsal_red_black_node_run_assertions(node_D, self);
    bsal_red_black_node_run_assertions(node_E, self);
    bsal_red_black_node_run_assertions(self->root, self);
#endif

}

void print_spaces(int depth)
{
    while (depth--) {
        printf("    ");
    }
    printf("-->");
}

void bsal_red_black_tree_print_node(struct bsal_red_black_tree *self,
                struct bsal_red_black_node *node, int depth)
{
    if (node == NULL) {
        print_spaces(depth);
        printf("(NIL, BLACK)\n");
    } else {
        print_spaces(depth);

        if (node->color == BSAL_COLOR_RED) {
            printf("(%d, RED)\n", bsal_red_black_node_get_key_as_int(node, self->key_size));
        } else {
            printf("(%d, BLACK)\n", bsal_red_black_node_get_key_as_int(node, self->key_size));
        }

#if 0
        bsal_red_black_node_run_assertions(node);
#endif

        bsal_red_black_tree_print_node(self, node->left_node, depth + 1);
        bsal_red_black_tree_print_node(self, node->right_node, depth + 1);
    }
}

void bsal_red_black_tree_print(struct bsal_red_black_tree *self)
{
    printf("Red-black tree content (%d non-NIL nodes):\n", self->size);
    bsal_red_black_tree_print_node(self, self->root, 0);
    printf("\n");
}

void *bsal_red_black_tree_get(struct bsal_red_black_tree *self, void *key)
{
    struct bsal_red_black_node *node;
    void *value;
    int result;

    if (self->cached_last_node != NULL
                    && self->cached_last_node->key == key) {
        return self->cached_last_node->value;
    }

    value = NULL;
    node = self->root;

    while (node != NULL && value == NULL) {
        result = bsal_red_black_tree_compare(self, key, node->key);

        if (result < 0) {
            node = node->left_node;
        } else if (result > 0) {
            node = node->right_node;
        } else {
            value = node->value;
        }
    }

    self->cached_last_node = node;

    return value;
}

void *bsal_red_black_tree_get_lowest_key(struct bsal_red_black_tree *self)
{
    struct bsal_red_black_node *node;

    if (self->cached_lowest_node != NULL) {
        self->cached_last_node = self->cached_lowest_node;
        return self->cached_lowest_node->key;
    }

    node = self->root;

    /*
     * Empty tree.
     */
    if (node == NULL) {
        return NULL;
    }

    while (1) {

        /*
         * This is the lowest value.
         */
        if (node->left_node == NULL) {
            return node->key;
        }

        node = node->left_node;
    }

    return NULL;
}

int bsal_red_black_tree_compare(struct bsal_red_black_tree *self, void *key1, void *key2)
{
    return self->compare(self, key1, key2);
}

int bsal_red_black_tree_compare_memory_content(struct bsal_red_black_tree *self, void *key1, void *key2)
{
    int result;

#ifdef DEBUG_COMPARE_MEMORY_CONTENT
    int integer1;
    int integer2;

    integer1 = *(int *)key1;
    integer2 = *(int *)key2;
#endif

    result = memcmp(key1, key2, self->key_size);

#ifdef DEBUG_COMPARE_MEMORY_CONTENT
    printf("DEBUG compare_memory_content integer1 %d integer2 %d result %d\n",
                    integer1, integer2, result);
#endif

    return result;
}

int bsal_red_black_tree_compare_uint64_t(struct bsal_red_black_tree *self, void *key1, void *key2)
{
    uint64_t key1_as_uint64_t;
    uint64_t key2_as_uint64_t;

    key1_as_uint64_t = *(uint64_t *)key1;
    key2_as_uint64_t = *(uint64_t *)key2;

    if (key1_as_uint64_t < key2_as_uint64_t) {
        return -1;
    } else if (key1_as_uint64_t > key2_as_uint64_t) {
        return 1;
    }

    return 0;
}

void bsal_red_black_tree_use_uint64_t_keys(struct bsal_red_black_tree *self)
{
    self->compare = bsal_red_black_tree_compare_uint64_t;
}

void bsal_red_black_tree_run_assertions(struct bsal_red_black_tree *self)
{
    bsal_red_black_tree_run_assertions_on_node(self, self->root);
}

void bsal_red_black_tree_run_assertions_on_node(struct bsal_red_black_tree *self, struct bsal_red_black_node *node)
{
    if (node == NULL)
        return;

#if 0
    printf("run_assertions_on_node node %d\n",
                    bsal_red_black_node_get_key_as_int(node, self->key_size));
#endif

    bsal_red_black_node_run_assertions(node, self);

    bsal_red_black_tree_run_assertions_on_node(self, node->left_node);
    bsal_red_black_tree_run_assertions_on_node(self, node->right_node);
}
