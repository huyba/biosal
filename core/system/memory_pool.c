
#include "memory_pool.h"

#include <core/system/tracer.h>

#include <core/helpers/bitmap.h>

#include <core/structures/queue.h>
#include <core/structures/map_iterator.h>

#include <stdio.h>

/*
 * Some flags.
 */
#define FLAG_ENABLE_TRACKING 0
#define FLAG_DISABLED 1
#define FLAG_ENABLE_SEGMENT_NORMALIZATION 2

void bsal_memory_pool_init(struct bsal_memory_pool *self, int block_size)
{
    bsal_map_init(&self->recycle_bin, sizeof(int), sizeof(struct bsal_queue));
    bsal_map_init(&self->allocated_blocks, sizeof(void *), sizeof(int));
    bsal_set_init(&self->large_blocks, sizeof(void *));

    self->current_block = NULL;

    bsal_queue_init(&self->dried_blocks, sizeof(struct bsal_memory_block *));
    bsal_queue_init(&self->ready_blocks, sizeof(struct bsal_memory_block *));

    self->block_size = block_size;

    /*
     * Configure flags
     */
    self->flags = 0;
    bsal_bitmap_set_bit_uint32_t(&self->flags, FLAG_ENABLE_TRACKING);
    bsal_bitmap_clear_bit_uint32_t(&self->flags, FLAG_ENABLE_SEGMENT_NORMALIZATION);
    bsal_bitmap_clear_bit_uint32_t(&self->flags, FLAG_DISABLED);
}

void bsal_memory_pool_destroy(struct bsal_memory_pool *self)
{
    struct bsal_queue *queue;
    struct bsal_map_iterator iterator;
    struct bsal_memory_block *block;

    /* destroy recycled objects
     */
    bsal_map_iterator_init(&iterator, &self->recycle_bin);

    while (bsal_map_iterator_has_next(&iterator)) {
        bsal_map_iterator_next(&iterator, NULL, (void **)&queue);

        bsal_queue_destroy(queue);
    }
    bsal_map_iterator_destroy(&iterator);
    bsal_map_destroy(&self->recycle_bin);

    /* destroy allocated blocks */
    bsal_map_destroy(&self->allocated_blocks);

    /* destroy dried blocks
     */
    while (bsal_queue_dequeue(&self->dried_blocks, &block)) {
        bsal_memory_block_destroy(block);
        bsal_memory_free(block);
    }
    bsal_queue_destroy(&self->dried_blocks);

    /* destroy ready blocks
     */
    while (bsal_queue_dequeue(&self->ready_blocks, &block)) {
        bsal_memory_block_destroy(block);
        bsal_memory_free(block);
    }
    bsal_queue_destroy(&self->ready_blocks);

    /* destroy the current block
     */
    if (self->current_block != NULL) {
        bsal_memory_block_destroy(self->current_block);
        bsal_memory_free(self->current_block);
        self->current_block = NULL;
    }

    bsal_set_destroy(&self->large_blocks);
}

void *bsal_memory_pool_allocate(struct bsal_memory_pool *self, size_t size)
{
    void *pointer;
    size_t new_size;

    if (self != NULL
                 && bsal_bitmap_get_bit_uint32_t(&self->flags, FLAG_ENABLE_SEGMENT_NORMALIZATION)) {
        new_size = bsal_memory_pool_normalize_segment_length(self, size);
#if 0
        printf("NORMALIZE %zu -> %zu\n", size, new_size);
#endif
        size = new_size;
    }

    pointer = bsal_memory_pool_allocate_private(self, size);

    if (pointer == NULL) {
        printf("Error, requested %zu bytes, returned pointer is NULL\n",
                        size);

        bsal_tracer_print_stack_backtrace();

        exit(1);
    }

    return pointer;
}

void *bsal_memory_pool_allocate_private(struct bsal_memory_pool *self, size_t size)
{
    struct bsal_queue *queue;
    void *pointer;

    if (size == 0) {
        return NULL;
    }

#ifdef BSAL_MEMORY_ALIGNMENT_ENABLED
    /* Align memory to avoid problems with performance and/or
     * Bus errors...
     */
    size = bsal_memory_align(size);
#endif

    if (self == NULL || bsal_bitmap_get_bit_uint32_t(&self->flags, FLAG_DISABLED)) {
        return bsal_memory_allocate(size);
    }

    /*
     * First, check if the size is larger than the maximum size.
     * If memory blocks can not fulfil the need, use the memory system
     * directly.
     */

    if (size >= (size_t)self->block_size) {
        pointer = bsal_memory_allocate(size);

        bsal_set_add(&self->large_blocks, &pointer);

        return pointer;
    }

    queue = NULL;

    if (bsal_bitmap_get_bit_uint32_t(&self->flags, FLAG_ENABLE_TRACKING)) {
        queue = bsal_map_get(&self->recycle_bin, &size);
    }

    /* recycling is good for the environment
     */
    if (queue != NULL && bsal_queue_dequeue(queue, &pointer)) {

        if (bsal_bitmap_get_bit_uint32_t(&self->flags, FLAG_ENABLE_TRACKING)) {
            bsal_map_add_value(&self->allocated_blocks, &pointer, &size);
        }

#ifdef BSAL_MEMORY_POOL_DISCARD_EMPTY_QUEUES
        if (bsal_queue_empty(queue)) {
            bsal_queue_destroy(queue);
            bsal_map_delete(&self->recycle_bin, &size);
        }
#endif

        return pointer;
    }

    if (self->current_block == NULL) {

        bsal_memory_pool_add_block(self);
    }

    pointer = bsal_memory_block_allocate(self->current_block, size);

    /* the current block is exausted...
     */
    if (pointer == NULL) {
        bsal_queue_enqueue(&self->dried_blocks, &self->current_block);
        self->current_block = NULL;

        bsal_memory_pool_add_block(self);

        pointer = bsal_memory_block_allocate(self->current_block, size);
    }

    if (bsal_bitmap_get_bit_uint32_t(&self->flags, FLAG_ENABLE_TRACKING)) {
        bsal_map_add_value(&self->allocated_blocks, &pointer, &size);
    }

    return pointer;
}

void bsal_memory_pool_add_block(struct bsal_memory_pool *self)
{
    /* Try to pick a block in the ready block list.
     * Otherwise, create one on-demand today.
     */
    if (!bsal_queue_dequeue(&self->ready_blocks, &self->current_block)) {
        self->current_block = bsal_memory_allocate(sizeof(struct bsal_memory_block));
        bsal_memory_block_init(self->current_block, self->block_size);
    }
}

void bsal_memory_pool_free(struct bsal_memory_pool *self, void *pointer)
{
    struct bsal_queue *queue;
    int size;

    if (pointer == NULL) {
        return;
    }

    if (self == NULL || bsal_bitmap_get_bit_uint32_t(&self->flags, FLAG_DISABLED)) {
        bsal_memory_free(pointer);
        return;
    }

    /* Verify if the pointer is a large block not managed by one of the memory
     * blocks
     */

    if (bsal_set_find(&self->large_blocks, &pointer)) {

        bsal_memory_free(pointer);
        bsal_set_delete(&self->large_blocks, &pointer);
        return;
    }

    /*
     * Return immediately if memory allocation tracking is disabled.
     * For example, the ephemeral memory component of a worker
     * disable tracking (flag FLAG_ENABLE_TRACKING = 0). To free memory,
     * for the ephemeral memory, bsal_memory_pool_free_all is
     * used.
     */
    if (!bsal_bitmap_get_bit_uint32_t(&self->flags, FLAG_ENABLE_TRACKING)) {
        return;
    }

    if (!bsal_map_get_value(&self->allocated_blocks, &pointer, &size)) {
        return;
    }

    queue = bsal_map_get(&self->recycle_bin, &size);

    if (queue == NULL) {
        queue = bsal_map_add(&self->recycle_bin, &size);
        bsal_queue_init(queue, sizeof(void *));
    }

    bsal_queue_enqueue(queue, &pointer);

    bsal_map_delete(&self->allocated_blocks, &pointer);
}

void bsal_memory_pool_disable_tracking(struct bsal_memory_pool *self)
{
    bsal_bitmap_clear_bit_uint32_t(&self->flags, FLAG_ENABLE_TRACKING);
}

void bsal_memory_pool_enable_normalization(struct bsal_memory_pool *self)
{
    bsal_bitmap_set_bit_uint32_t(&self->flags, FLAG_ENABLE_SEGMENT_NORMALIZATION);
}

void bsal_memory_pool_disable_normalization(struct bsal_memory_pool *self)
{
    bsal_bitmap_clear_bit_uint32_t(&self->flags, FLAG_ENABLE_SEGMENT_NORMALIZATION);
}

void bsal_memory_pool_enable_tracking(struct bsal_memory_pool *self)
{
    bsal_bitmap_set_bit_uint32_t(&self->flags, FLAG_ENABLE_TRACKING);
}

void bsal_memory_pool_free_all(struct bsal_memory_pool *self)
{
    struct bsal_memory_block *block;
    int i;
    int size;

    /* reset the current block
     */
    if (self->current_block != NULL) {
        bsal_memory_block_free_all(self->current_block);
    }

    /* reset all ready blocks
     */

    size = bsal_queue_size(&self->ready_blocks);
    i = 0;
    while (i < size
                   && bsal_queue_dequeue(&self->ready_blocks, &block)) {
        bsal_memory_block_free_all(block);
        bsal_queue_enqueue(&self->ready_blocks, &block);

        i++;
    }

    /* reset all dried blocks */
    while (bsal_queue_dequeue(&self->dried_blocks, &block)) {
        bsal_memory_block_free_all(block);
        bsal_queue_enqueue(&self->ready_blocks, &block);
    }
}

void bsal_memory_pool_disable(struct bsal_memory_pool *self)
{
    bsal_bitmap_set_bit_uint32_t(&self->flags, FLAG_DISABLED);
}

size_t bsal_memory_pool_normalize_segment_length(struct bsal_memory_pool *self, size_t size)
{
    uint32_t value;
    size_t maximum;
    size_t return_value;

    /*
     * Pick up the next power of 2.
     */

    /*
     * Check if the value fits in 32 bits.
     */
    value = 0;
    value--;

    maximum = value;

    if (size > maximum) {
        /*
         * Use a manual approach for things that don't fit in a uint32_t.
         */

        return_value = 1;

        while (return_value < size) {
            return_value *= 2;
        }

        return return_value;
    }

    /*
     * Otherwise, use the fancy algorithm.
     * The algorithm is from http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
     */

    value = size;

    value--;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value++;

    return_value = value;

    return value;
}
