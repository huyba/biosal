
#include "sequence_store_manager.h"

#include "sequence_store.h"

#include <structures/vector_iterator.h>
#include <structures/dynamic_hash_table_iterator.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <inttypes.h>

struct bsal_script bsal_sequence_store_manager_script = {
    .name = BSAL_SEQUENCE_STORE_MANAGER_SCRIPT,
    .init = bsal_sequence_store_manager_init,
    .destroy = bsal_sequence_store_manager_destroy,
    .receive = bsal_sequence_store_manager_receive,
    .size = sizeof(struct bsal_sequence_store_manager)
};

void bsal_sequence_store_manager_init(struct bsal_actor *actor)
{
    struct bsal_sequence_store_manager *concrete_actor;

    concrete_actor = (struct bsal_sequence_store_manager *)bsal_actor_concrete_actor(actor);

    bsal_dynamic_hash_table_init(&concrete_actor->spawner_store_count, 128, sizeof(int), sizeof(int));
    bsal_dynamic_hash_table_init(&concrete_actor->spawner_stores, 128, sizeof(int), sizeof(struct bsal_vector));
    bsal_vector_init(&concrete_actor->indices, sizeof(int));

    concrete_actor->ready_spawners = 0;
    concrete_actor->spawners = 0;
}

void bsal_sequence_store_manager_destroy(struct bsal_actor *actor)
{
    struct bsal_sequence_store_manager *concrete_actor;
    struct bsal_dynamic_hash_table_iterator iterator;
    struct bsal_vector *vector;

    concrete_actor = (struct bsal_sequence_store_manager *)bsal_actor_concrete_actor(actor);

    bsal_dynamic_hash_table_destroy(&concrete_actor->spawner_store_count);

    bsal_dynamic_hash_table_iterator_init(&iterator, &concrete_actor->spawner_stores);

    while (bsal_dynamic_hash_table_iterator_has_next(&iterator)) {
        bsal_dynamic_hash_table_iterator_next(&iterator, NULL, (void **)&vector);

        bsal_vector_destroy(vector);
    }

    bsal_dynamic_hash_table_iterator_destroy(&iterator);

    bsal_dynamic_hash_table_destroy(&concrete_actor->spawner_stores);

    bsal_vector_destroy(&concrete_actor->indices);
}

void bsal_sequence_store_manager_receive(struct bsal_actor *actor, struct bsal_message *message)
{
    int tag;
    struct bsal_vector spawners;
    struct bsal_vector_iterator iterator;
    int *bucket;
    int index;
    struct bsal_sequence_store_manager *concrete_actor;
    struct bsal_vector *stores;
    int script;
    int spawner;
    int workers;
    int stores_per_worker;
    void *buffer;
    int source;
    int store;
    int child;
    int i;
    struct bsal_vector all_stores;
    int new_count;
    void *new_buffer;
    struct bsal_message new_message;

    source = bsal_message_source(message);
    buffer = bsal_message_buffer(message);
    concrete_actor = (struct bsal_sequence_store_manager *)bsal_actor_concrete_actor(actor);
    tag = bsal_message_tag(message);
    script = BSAL_SEQUENCE_STORE_SCRIPT;

    if (tag == BSAL_ACTOR_START) {

        printf("DEBUG manager actor/%d starts\n",
                        bsal_actor_name(actor));

        bsal_vector_unpack(&spawners, buffer);

        concrete_actor->spawners = bsal_vector_size(&spawners);

        printf("DEBUG manager actor/%d starts, supervisor is actor/%d, %d spawners provided\n",
                        bsal_actor_name(actor), bsal_actor_supervisor(actor),
                        (int)bsal_vector_size(&spawners));

        bsal_vector_iterator_init(&iterator, &spawners);

        while (bsal_vector_iterator_has_next(&iterator)) {

            bsal_vector_iterator_next(&iterator, (void **)&bucket);

            spawner = *bucket;
            index = bsal_actor_add_acquaintance(actor, spawner);

            bsal_vector_push_back(&concrete_actor->indices, &index);

            printf("DEBUG manager actor/%d add store vector and store count for spawner actor/%d\n",
                            bsal_actor_name(actor), spawner);

            stores = (struct bsal_vector *)bsal_dynamic_hash_table_add(&concrete_actor->spawner_stores, &index);

            printf("DEBUG adding %d to table\n", index);

            bucket = (int *)bsal_dynamic_hash_table_add(&concrete_actor->spawner_store_count, &index);
            *bucket = 0;

#ifdef BSAL_SEQUENCE_STORE_MANAGER_DEBUG
            printf("DEBUG685-1 spawner %d index %d bucket %p\n", spawner, index, (void *)bucket);
            bsal_vector_print_int(bsal_actor_acquaintance_vector(actor));
#endif

            bsal_vector_init(stores, sizeof(int));

            bsal_actor_send_empty(actor, spawner, BSAL_ACTOR_GET_NODE_WORKER_COUNT);
        }

        bsal_vector_iterator_destroy(&iterator);
        bsal_vector_destroy(&spawners);

    } else if (tag == BSAL_ACTOR_GET_NODE_WORKER_COUNT_REPLY) {

        workers = *(int *)buffer;
        stores_per_worker = 4;

        index = bsal_actor_get_acquaintance_index(actor, source);

        printf("DEBUG manager actor/%d says that spawner actor/%d is on a node with %d workers\n",
                        bsal_actor_name(actor), source, workers);

        printf("DEBUG getting table index %d\n", index);
        bucket = (int *)bsal_dynamic_hash_table_get(&concrete_actor->spawner_store_count, &index);

#ifdef BSAL_SEQUENCE_STORE_MANAGER_DEBUG
        printf("DEBUG685-2 spawner %d index %d bucket %p\n", source, index, (void *)bucket);
        bsal_vector_print_int(bsal_actor_acquaintance_vector(actor));
#endif

        *bucket = workers * stores_per_worker;

        bsal_actor_send_reply_int(actor, BSAL_ACTOR_SPAWN, script);

    } else if (tag == BSAL_ACTOR_SPAWN_REPLY) {

        store = *(int *)buffer;
        index = bsal_actor_get_acquaintance_index(actor, source);

        stores = (struct bsal_vector *)bsal_dynamic_hash_table_get(&concrete_actor->spawner_stores, &index);
        bucket = (int *)bsal_dynamic_hash_table_get(&concrete_actor->spawner_store_count, &index);

        bsal_vector_push_back(stores, &store);

        printf("DEBUG manager actor/%d receives store actor/%d from spawner actor/%d, now %d/%d\n",
                        bsal_actor_name(actor), store, source,
                        (int)bsal_vector_size(stores), *bucket);

        if (bsal_vector_size(stores) == *bucket) {

            concrete_actor->ready_spawners++;

            printf("DEBUG manager actor/%d says that spawner actor/%d is ready, %d/%d\n",
                        bsal_actor_name(actor), source,
                        concrete_actor->ready_spawners, concrete_actor->spawners);

            if (concrete_actor->ready_spawners == concrete_actor->spawners) {

                printf("DEBUG manager actor/%d says that all spawners are ready\n",
                        bsal_actor_name(actor));

                bsal_vector_init(&all_stores, sizeof(int));

                bsal_vector_iterator_init(&iterator, &concrete_actor->indices);

                while (bsal_vector_iterator_has_next(&iterator)) {
                    bsal_vector_iterator_next(&iterator, (void **)&bucket);

                    index = *bucket;

                    stores = (struct bsal_vector *)bsal_dynamic_hash_table_get(&concrete_actor->spawner_stores,
                                    &index);

                    bsal_vector_push_back_vector(&all_stores, stores);
                }

                bsal_vector_iterator_destroy(&iterator);

                new_count = bsal_vector_pack_size(&all_stores);
                new_buffer = malloc(new_count);
                bsal_vector_pack(&all_stores, new_buffer);
                bsal_vector_destroy(&all_stores);

                bsal_message_init(&new_message, BSAL_ACTOR_START_REPLY, new_count, new_buffer);
                bsal_actor_send_to_supervisor(actor, &new_message);

                free(new_buffer);

                bsal_message_destroy(&new_message);
            }
        } else {

            bsal_actor_send_reply_int(actor, BSAL_ACTOR_SPAWN, script);
        }
    } else if (tag == BSAL_ACTOR_ASK_TO_STOP) {

        printf("DEBUG manager actor/%d dies\n",
                        bsal_actor_name(actor));

        for (i = 0; i < bsal_actor_child_count(actor); i++) {

            child = bsal_actor_get_child(actor, i);

            bsal_actor_send_empty(actor, child, BSAL_ACTOR_ASK_TO_STOP);
        }

        bsal_actor_send_to_self_empty(actor, BSAL_ACTOR_STOP);
    }
}
