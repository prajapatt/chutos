#pragma once

#include <stdint.h>

typedef void (*task_entry_t)(void);

typedef enum
{
    SCHEDULER_LANE_SYSTEM = 0,
    SCHEDULER_LANE_USER = 1,
    SCHEDULER_LANE_AI = 2
} scheduler_lane_t;

typedef struct
{
    const char *name;
    scheduler_lane_t lane;
    uint8_t active;
} scheduler_task_info_t;

int scheduler_add(const char *name, task_entry_t entry);
int scheduler_add_lane(const char *name, task_entry_t entry, scheduler_lane_t lane);
uint64_t scheduler_task_count(void);
int scheduler_task_info(uint64_t index, scheduler_task_info_t *info);
void scheduler_set_performance_mode(int enabled);
int scheduler_performance_mode(void);
void scheduler_tick(void);
void scheduler_run_once(void);
uint64_t scheduler_ticks(void);
