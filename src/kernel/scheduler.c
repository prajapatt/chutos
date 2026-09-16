#include "scheduler.h"

#define MAX_TASKS 16

typedef struct
{
    const char *name;
    task_entry_t entry;
    scheduler_lane_t lane;
    uint8_t active;
} task_t;

static task_t tasks[MAX_TASKS];
static uint64_t task_count;
static uint64_t next_task;
static uint64_t ticks;
static uint8_t lane_credits[3];
static uint8_t performance_mode;

static uint8_t lane_weight(scheduler_lane_t lane)
{
    if (performance_mode)
    {
        if (lane == SCHEDULER_LANE_USER)
        {
            return 6;
        }
        if (lane == SCHEDULER_LANE_SYSTEM)
        {
            return 2;
        }
        return 1;
    }
    if (lane == SCHEDULER_LANE_USER)
    {
        return 3;
    }
    if (lane == SCHEDULER_LANE_AI)
    {
        return 2;
    }
    return 1;
}

static uint8_t active_lane_mask(void)
{
    uint8_t mask = 0;
    for (uint64_t task_index = 0; task_index < task_count; ++task_index)
    {
        if (tasks[task_index].active)
        {
            mask |= (uint8_t)(1u << tasks[task_index].lane);
        }
    }
    return mask;
}

static void replenish_lane_credits(uint8_t active_lanes)
{
    for (uint8_t lane = 0; lane < 3; ++lane)
    {
        lane_credits[lane] = (active_lanes & (uint8_t)(1u << lane)) != 0
                                 ? lane_weight((scheduler_lane_t)lane)
                                 : 0;
    }
}

int scheduler_add(const char *name, task_entry_t entry)
{
    return scheduler_add_lane(name, entry, SCHEDULER_LANE_SYSTEM);
}

int scheduler_add_lane(const char *name, task_entry_t entry, scheduler_lane_t lane)
{
    if (entry == 0 || task_count == MAX_TASKS || lane > SCHEDULER_LANE_AI)
    {
        return -1;
    }
    tasks[task_count++] = (task_t){name, entry, lane, 1};
    return 0;
}

uint64_t scheduler_task_count(void)
{
    return task_count;
}

int scheduler_task_info(uint64_t index, scheduler_task_info_t *info)
{
    if (info == 0 || index >= task_count)
    {
        return -1;
    }
    info->name = tasks[index].name;
    info->lane = tasks[index].lane;
    info->active = tasks[index].active;
    return 0;
}

void scheduler_set_performance_mode(int enabled)
{
    performance_mode = enabled != 0;
    lane_credits[0] = 0;
    lane_credits[1] = 0;
    lane_credits[2] = 0;
}

int scheduler_performance_mode(void)
{
    return performance_mode != 0;
}

void scheduler_tick(void)
{
    ++ticks;
}

void scheduler_run_once(void)
{
    if (task_count == 0)
    {
        return;
    }
    uint8_t active_lanes = active_lane_mask();
    if (active_lanes == 0)
    {
        return;
    }

    uint8_t runnable_with_credit = 0;
    for (uint64_t task_index = 0; task_index < task_count; ++task_index)
    {
        task_t *task = &tasks[task_index];
        if (task->active && lane_credits[task->lane] != 0)
        {
            runnable_with_credit = 1;
            break;
        }
    }
    if (!runnable_with_credit)
    {
        replenish_lane_credits(active_lanes);
    }

    uint8_t best_credit = 0;
    uint64_t selected = next_task;
    for (uint64_t attempts = 0; attempts < task_count; ++attempts)
    {
        uint64_t index = (next_task + attempts) % task_count;
        task_t *task = &tasks[index];
        if (task->active && lane_credits[task->lane] > best_credit)
        {
            selected = index;
            best_credit = lane_credits[task->lane];
        }
    }

    if (best_credit == 0)
    {
        return;
    }

    --lane_credits[tasks[selected].lane];
    next_task = (selected + 1) % task_count;
    tasks[selected].entry();
}

uint64_t scheduler_ticks(void)
{
    return ticks;
}
