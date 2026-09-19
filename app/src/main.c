#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/task_wdt/task_wdt.h>
#include<zephyr/task_wdt/task_wdt.h>

#define APP_LOG_LEVEL LOG_LEVEL_DBG
#define PROCESSOR_STACK_SIZE 1024

LOG_MODULE_REGISTER(demo, APP_LOG_LEVEL);

#define SAMPLE_COUNT         12
#define SENSOR_QUEUE_DEPTH    8
#define PRODUCER_STACK_SIZE 1024
#define HEALTH_STACK_SIZE   1024
#define CONSUMER_WDT_TIMEOUT_MS 500
#define CONSUMER_SLEEP_MS      1000

/* ================================================================== */
/*  Messages and communication objects                                */
/* ================================================================== */

struct sensor_sample {
    uint32_t seq;
    int32_t value;
    uint32_t timestamp_ms;
};

K_MSGQ_DEFINE(sensor_queue, sizeof(struct sensor_sample), SENSOR_QUEUE_DEPTH, 4);
K_SEM_DEFINE(demo_done, 0, 3);

static int consumer_wdt_id;

static void consumer_wdt_cb(int channel_id, void *user_data)
{
    k_tid_t thread = (k_tid_t)user_data;

    LOG_ERR("[WATCHDOG] consumer stalled: channel=%d thread=%s",
        channel_id, k_thread_name_get(thread));
}

/* ================================================================== */
/*  Producer                                                          */
/* ================================================================== */

static void producer_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    for (uint32_t seq = 0; seq < SAMPLE_COUNT; seq++) {
        struct sensor_sample sample = {
            .seq = seq,
            .value = 100 + (int32_t)seq,
            .timestamp_ms = k_uptime_get_32(),
        };

        LOG_DBG("[PRODUCER] acquired seq=%u value=%d timestamp=%u",
            sample.seq, sample.value, sample.timestamp_ms);

        int ret = k_msgq_put(&sensor_queue, &sample, K_MSEC(100));

        if (ret != 0) {
            LOG_WRN("[PRODUCER] queue full, dropped seq=%u", seq);
        } else {
            LOG_DBG("[PRODUCER] queued seq=%u used=%u/%u", 
                seq, k_msgq_num_used_get(&sensor_queue), SENSOR_QUEUE_DEPTH);
        }

        k_msleep(50);
    }

    LOG_INF("[PRODUCER] done");
    k_sem_give(&demo_done);
}

/* ================================================================== */
/*  Processor                                                         */
/* ================================================================== */

static void processor_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    consumer_wdt_id = task_wdt_add(CONSUMER_WDT_TIMEOUT_MS,
        consumer_wdt_cb, (void *)k_current_get());
    if (consumer_wdt_id < 0) {
        LOG_ERR("[CONSUMER] watchdog setup failed: %d", consumer_wdt_id);
        k_sem_give(&demo_done);
        return;
    }

    for (int i = 0; i < SAMPLE_COUNT; i++) {
        struct sensor_sample sample;

        int ret = k_msgq_get(&sensor_queue, &sample, K_FOREVER);

        if (ret != 0) {
            LOG_ERR("[PROCESSOR] receive failed: %d", ret);
            break;
        }

        LOG_DBG("[PROCESSOR] seq=%u value=%d",
            sample.seq, sample.value);

        task_wdt_feed(consumer_wdt_id);

        /* Deliberately longer than the watchdog timeout to simulate a stall. */
        k_sleep(K_MSEC(CONSUMER_SLEEP_MS));
    }

    LOG_INF("[PROCESSOR] done");
    k_sem_give(&demo_done);
}

/* ================================================================== */
/*  Health monitor                                                    */
/* ================================================================== */

static void health_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);


    for (int i = 0; i < 50; i++) {
        k_msleep(120);
        LOG_DBG("[HEALTH] checking queue fill level");
        uint32_t used = k_msgq_num_used_get(&sensor_queue);
        LOG_DBG("[HEALTH] queue used=%u/%u", used, SENSOR_QUEUE_DEPTH);
        if (used >= (SENSOR_QUEUE_DEPTH * 3 / 4)) {
            LOG_WRN("[HEALTH] queue at %u/%u (at least 75%% capacity)",
                used, SENSOR_QUEUE_DEPTH);
        }
    }
    LOG_INF("[HEALTH] done");
    k_sem_give(&demo_done);
}

/* ================================================================== */
/*  Threads                                                           */
/* ================================================================== */

K_THREAD_DEFINE(producer_thread, PRODUCER_STACK_SIZE, producer_fn,
                NULL, NULL, NULL, 5, 0, 0);

K_THREAD_DEFINE(processor_thread, PROCESSOR_STACK_SIZE, processor_fn,
                NULL, NULL, NULL, 5, 0, 0);

K_THREAD_DEFINE(health_thread, HEALTH_STACK_SIZE, health_fn,
                NULL, NULL, NULL, 6, 0, 0);

/* ================================================================== */
/*  Main                                                              */
/* ================================================================== */

int main(void)
{
    int ret = task_wdt_init(NULL);

    if (ret != 0) {
        LOG_ERR("Task watchdog initialization failed: %d", ret);
        return ret;
    }

    LOG_INF("=== L5 Demo 2: Resource Budget Audit ===");

    LOG_INF("producer stack=%d bytes", PRODUCER_STACK_SIZE);

    LOG_INF("processor stack=%d bytes", PROCESSOR_STACK_SIZE);

    LOG_INF("health stack=%d bytes", HEALTH_STACK_SIZE);

    LOG_INF("sensor queue=%u bytes",
        (uint32_t)(sizeof(struct sensor_sample) * SENSOR_QUEUE_DEPTH));


    k_sem_take(&demo_done, K_FOREVER);
    k_sem_take(&demo_done, K_FOREVER);
    k_sem_take(&demo_done, K_FOREVER);


    return 0;
}