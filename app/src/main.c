#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(demo, LOG_LEVEL_DBG);

#define STACK_SIZE      1024
#define PRIO            5
#define INCREMENTS      1000   /* Lower count recommended when forcing yield to keep runtimes reasonable */

/* Select test mode: 1 for unprotected with forced yield, 0 for mutex-protected */
#define TEST_UNPROTECTED 0

/* Shared state - intentionally unprotected */
static volatile uint32_t counter;

static struct k_sem done_sem;
static K_MUTEX_DEFINE(counter_mutex);

void worker_fn(void *p1, void *p2, void *p3)
{
    const char *name = k_thread_name_get(k_current_get());

    for (int i = 0; i < INCREMENTS; i++) {
#if TEST_UNPROTECTED
        /* Unprotected read-modify-write broken down with a forced context switch */
        uint32_t temp = counter;
        k_yield();          /* Forces scheduler to switch to the other thread */
        counter = temp + 1;
#else
        /* Mutex-protected version */
        k_mutex_lock(&counter_mutex, K_FOREVER);
        counter++;
        k_mutex_unlock(&counter_mutex);
#endif
    }

    LOG_INF("[%s] finished", name);
    k_sem_give(&done_sem);
}

K_THREAD_DEFINE(worker_a, STACK_SIZE, worker_fn, NULL, NULL, NULL,
                PRIO, 0, 0);
K_THREAD_DEFINE(worker_b, STACK_SIZE, worker_fn, NULL, NULL, NULL,
                PRIO, 0, 0);

int main(void)
{
    k_sem_init(&done_sem, 0, 2);
    int64_t time = k_uptime_get();

    LOG_INF("=== L2 Demo: Race Condition vs Mutex ===");
    LOG_INF("Mode: %s", TEST_UNPROTECTED ? "Unprotected (with k_yield)" : "Mutex Protected");
    LOG_INF("Expected final value: %d", INCREMENTS * 2);

    /* Wait for both workers to complete */
    k_sem_take(&done_sem, K_FOREVER);
    k_sem_take(&done_sem, K_FOREVER);

    LOG_INF("Actual  final value: %u", counter);

    if (counter == INCREMENTS * 2) {
        LOG_WRN("No race this run");
    } else {
        LOG_ERR("Race condition confirmed: lost %d updates",
                (INCREMENTS * 2) - counter);
    }
    LOG_INF("Execution time: %lld ms", k_uptime_delta(&time));

    return 0;
}