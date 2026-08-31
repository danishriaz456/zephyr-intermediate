/*
 * Lecture 3 - Homework Starter Code
 *
 * GOAL: Convert a polling loop to an event-driven workqueue architecture.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <stdbool.h>

LOG_MODULE_REGISTER(homework, LOG_LEVEL_DBG);

#define STACK_SIZE    1024
#define SENSOR_MS     100    /* sensor fires every 100ms */
#define POLL_MS       10     /* polling consumer checks every 10ms */
#define EVENT_COUNT   10     /* total sensor events to produce */

/* Statistics */
static int total_events;
static int total_wakeups;
static int total_processed;

static void sensor_handler(struct k_work *work)
{
    ARG_UNUSED(work);
    total_processed++;
    LOG_INF("[HANDLER] processed event %d  tick=%u",
            total_processed, k_uptime_get_32());
}
K_WORK_DELAYABLE_DEFINE(debounce_work, sensor_handler);
/* ------------------------------------------------------------------ */
/*  sensor_sim - fires EVENT_COUNT events, 100ms apart               */
/* ------------------------------------------------------------------ */

static void sensor_sim_fn(void *p1, void *p2, void *p3)
{
    for (int i = 0; i < EVENT_COUNT; i++) {

        for(int j = 0; j < 5; j++) {
            k_msleep(20);
            total_events++;
            LOG_INF("[SENSOR] event %d.%d  tick=%u", i, j, k_uptime_get_32());
            int ret = k_work_reschedule(&debounce_work, K_MSEC(30));
            if (ret < 0) { LOG_ERR("reschedule failed: %d", ret); }
        }
        k_msleep(40);
    }

    LOG_INF("[SENSOR] all events produced");
}

K_THREAD_DEFINE(sensor_thread,  STACK_SIZE, sensor_sim_fn, NULL, NULL, NULL, 5, 0, 0);


int main(void)
{
    LOG_INF("=== L3 Homework: Polling to Workqueue ===");
    LOG_INF("Starter: polling every %dms, sensor fires every %dms",
            POLL_MS, SENSOR_MS);
    LOG_INF("Expected wasted wakeups: ~%d per event",
            (SENSOR_MS / POLL_MS) - 1);
    LOG_INF("Run this, count wakeups, then convert to workqueue.");

    /* Wait long enough for all events to complete */
    k_msleep((EVENT_COUNT + 2) * SENSOR_MS + 500);

    return 0;
}
