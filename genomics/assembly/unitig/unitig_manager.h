
#ifndef BIOSAL_UNITIG_MANAGER_H
#define BIOSAL_UNITIG_MANAGER_H

#include <engine/thorium/actor.h>

#include <core/system/timer.h>

#define SCRIPT_UNITIG_MANAGER 0x3bf29ca1

/*
 * A manager for unitig walkers
 */
struct biosal_unitig_manager {
    struct core_vector spawners;
    struct core_vector graph_stores;
    struct core_vector visitors;
    struct core_vector walkers;

    int completed;
    int manager;

    struct core_timer timer;
    int state;

    int writer_process;
};

extern struct thorium_script biosal_unitig_manager_script;

void biosal_unitig_manager_init(struct thorium_actor *self);
void biosal_unitig_manager_destroy(struct thorium_actor *self);
void biosal_unitig_manager_receive(struct thorium_actor *self, struct thorium_message *message);

#endif
