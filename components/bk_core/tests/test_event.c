#include <assert.h>
#include <stdio.h>
#include "bk_core.h"
#include "bk_event.h"

static int order[4];
static size_t order_count;
static bk_err_t callback_result;
static int callback_calls;

static void record_first(const bk_event_t *event, void *ctx)
{
    (void)event;
    order[order_count++] = *(int *)ctx;
}

static void record_second(const bk_event_t *event, void *ctx)
{
    (void)event;
    order[order_count++] = *(int *)ctx;
}

static void reset_order(void)
{
    order_count = 0;
    order[0] = order[1] = order[2] = order[3] = 0;
}

static void record_subscriber(const bk_event_t *event, void *ctx)
{
    (void)event;
    order[order_count++] = *(int *)ctx;
}

static void subscribe_during_publish(const bk_event_t *event, void *ctx)
{
    static int late_value = 3;
    int *value = ctx;

    callback_calls++;
    callback_result = bk_event_subscribe(event->id, record_subscriber, &late_value);
    order[order_count++] = *value;
}

static void deinit_during_publish(const bk_event_t *event, void *ctx)
{
    (void)event;
    (void)ctx;
    callback_calls++;
    callback_result = bk_core_deinit();
}

static void recursive_publish(const bk_event_t *event, void *ctx)
{
    (void)ctx;
    callback_calls++;
    callback_result = bk_event_publish(event);
}

static void test_lifecycle_errors(void)
{
    bk_event_t event = {.id = 1, .data = NULL, .size = 0};
    bk_event_t invalid_event = {.id = 1, .data = NULL, .size = 1};
    int context = 1;

    assert(bk_core_start() == BK_ERR_NOT_INITIALIZED);
    assert(bk_event_publish(&event) == BK_ERR_NOT_INITIALIZED);
    assert(bk_event_publish(NULL) == BK_ERR_INVALID_ARG);
    assert(bk_event_publish(&invalid_event) == BK_ERR_INVALID_ARG);
    assert(bk_event_subscribe(1, record_first, &context) == BK_ERR_NOT_INITIALIZED);
    assert(bk_core_deinit() == BK_ERR_NOT_INITIALIZED);
    assert(bk_core_init(NULL) == BK_ERR_INVALID_ARG);
}

static void test_order_and_duplicate_subscription(void)
{
    bk_core_config_t config = {0};
    bk_event_t event = {.id = 7, .data = NULL, .size = 0};
    int first = 1;
    int second = 2;

    assert(bk_core_init(&config) == BK_OK);
    assert(bk_core_init(&config) == BK_ERR_ALREADY_INITIALIZED);
    assert(bk_event_subscribe(event.id, record_first, &first) == BK_OK);
    assert(bk_event_subscribe(event.id, record_second, &second) == BK_OK);
    assert(bk_event_subscribe(event.id, record_first, &first) == BK_ERR_ALREADY_EXISTS);
    assert(bk_event_publish(&event) == BK_ERR_NOT_STARTED);
    assert(bk_core_start() == BK_OK);
    assert(bk_core_start() == BK_ERR_ALREADY_STARTED);

    reset_order();
    assert(bk_event_publish(&event) == BK_OK);
    assert(order_count == 2);
    assert(order[0] == 1);
    assert(order[1] == 2);
    assert(bk_event_publish(NULL) == BK_ERR_INVALID_ARG);

    assert(bk_core_deinit() == BK_OK);
    assert(bk_core_deinit() == BK_ERR_NOT_INITIALIZED);
}

static void test_unsubscribed_event(void)
{
    bk_core_config_t config = {0};
    bk_event_t event = {.id = 99, .data = NULL, .size = 0};

    assert(bk_core_init(&config) == BK_OK);
    assert(bk_core_start() == BK_OK);
    assert(bk_event_publish(&event) == BK_ERR_NOT_FOUND);
    assert(bk_core_deinit() == BK_OK);
}

static void test_publish_reentrancy_and_callback_mutation(void)
{
    bk_core_config_t config = {0};
    bk_event_t event = {.id = 100, .data = NULL, .size = 0};
    int first = 1;

    assert(bk_core_init(&config) == BK_OK);
    assert(bk_core_start() == BK_OK);
    callback_calls = 0;
    assert(bk_event_subscribe(event.id, recursive_publish, NULL) == BK_OK);
    assert(bk_event_publish(&event) == BK_OK);
    assert(callback_calls == 1);
    assert(callback_result == BK_ERR_REENTRANT);
    assert(bk_core_deinit() == BK_OK);

    assert(bk_core_init(&config) == BK_OK);
    assert(bk_core_start() == BK_OK);
    reset_order();
    callback_calls = 0;
    assert(bk_event_subscribe(event.id, subscribe_during_publish, &first) == BK_OK);
    assert(bk_event_publish(&event) == BK_OK);
    assert(callback_calls == 1);
    assert(callback_result == BK_OK);
    assert(order_count == 1);
    assert(order[0] == 1);
    reset_order();
    assert(bk_event_publish(&event) == BK_OK);
    assert(order_count == 2);
    assert(order[0] == 1);
    assert(order[1] == 3);
    assert(bk_core_deinit() == BK_OK);

    assert(bk_core_init(&config) == BK_OK);
    assert(bk_core_start() == BK_OK);
    callback_calls = 0;
    assert(bk_event_subscribe(event.id, deinit_during_publish, NULL) == BK_OK);
    assert(bk_event_publish(&event) == BK_OK);
    assert(callback_calls == 1);
    assert(callback_result == BK_ERR_BUSY);
    assert(bk_core_deinit() == BK_OK);
}

static void test_subscription_capacity(void)
{
    bk_core_config_t config = {.max_subscribers = 17};
    bk_core_config_t capacity_config = {.max_subscribers = 16};
    int contexts[17] = {0};
    size_t i;

    assert(bk_core_init(&config) == BK_ERR_LIMIT);
    assert(bk_core_init(&capacity_config) == BK_OK);
    for (i = 0; i < 16; ++i) {
        assert(bk_event_subscribe((bk_event_id_t)i, record_subscriber, &contexts[i]) == BK_OK);
    }
    assert(bk_event_subscribe(16, record_subscriber, &contexts[16]) == BK_ERR_LIMIT);
    assert(bk_core_deinit() == BK_OK);
}

int main(void)
{
    test_lifecycle_errors();
    test_order_and_duplicate_subscription();
    test_unsubscribed_event();
    test_publish_reentrancy_and_callback_mutation();
    test_subscription_capacity();
    puts("test_event: PASS");
    return 0;
}
