#include <assert.h>
#include <stdio.h>
#include "bk_core.h"
#include "bk_event.h"

static int order[4];
static size_t order_count;

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

int main(void)
{
    test_lifecycle_errors();
    test_order_and_duplicate_subscription();
    test_unsubscribed_event();
    puts("test_event: PASS");
    return 0;
}
