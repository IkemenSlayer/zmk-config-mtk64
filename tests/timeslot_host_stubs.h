/* Host-only boundary mocks. No real Zephyr scheduler, interrupts, or radio. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include <stdatomic.h>
#define IS_ENABLED(x) (x)
#define CONFIG_MPSL_ASSERT_HANDLER 0
#define CONFIG_ESB_MPSL_RADIO 1
#ifndef CONFIG_ZMK_SPLIT_ESB_TIMESLOT_LENGTH_US
#define CONFIG_ZMK_SPLIT_ESB_TIMESLOT_LENGTH_US 10000
#endif
#define CONFIG_ZMK_SPLIT_ESB_TIMESLOT_REQUEST_TIMEOUT_US 1000000
#define CONFIG_ZMK_SPLIT_ESB_TIMESLOT_EXT_MARGIN_MARGIN 4000
#define CONFIG_ZMK_SPLIT_ESB_TIMESLOT_REQ_EARLIEST_MARGIN 2000
#define MPSL_TIMESLOT_EXTENSION_MARGIN_MIN_US 87UL
#define BUILD_ASSERT(c, m) _Static_assert(c, m)
#define LOG_MODULE_REGISTER(...)
#define LOG_DBG(...)
#define LOG_WRN(...)
#define LOG_ERR(...)
#define K_THREAD_DEFINE(...)
#define K_MSGQ_DEFINE(name, ...) static int name
#define K_MSEC(x) (x)
#define K_FOREVER (-1)
#define NRF_EFAULT 14
#define NRF_EAGAIN 11
#define NRF_ENOENT 2
typedef enum { APP_TS_STARTED, APP_TS_STOPPED } zmk_split_esb_timeslot_callback_type_t;
typedef void (*zmk_split_esb_timeslot_callback_t)(zmk_split_esb_timeslot_callback_type_t);
typedef uint8_t mpsl_timeslot_session_id_t;
enum { MPSL_TIMESLOT_SIGNAL_START, MPSL_TIMESLOT_SIGNAL_TIMER0,
       MPSL_TIMESLOT_SIGNAL_RADIO, MPSL_TIMESLOT_SIGNAL_EXTEND_FAILED,
       MPSL_TIMESLOT_SIGNAL_EXTEND_SUCCEEDED, MPSL_TIMESLOT_SIGNAL_BLOCKED,
       MPSL_TIMESLOT_SIGNAL_CANCELLED, MPSL_TIMESLOT_SIGNAL_SESSION_IDLE,
       MPSL_TIMESLOT_SIGNAL_INVALID_RETURN, MPSL_TIMESLOT_SIGNAL_SESSION_CLOSED,
       MPSL_TIMESLOT_SIGNAL_OVERSTAYED };
enum { MPSL_TIMESLOT_SIGNAL_ACTION_NONE, MPSL_TIMESLOT_SIGNAL_ACTION_EXTEND,
       MPSL_TIMESLOT_SIGNAL_ACTION_END, MPSL_TIMESLOT_SIGNAL_ACTION_REQUEST };
#define MPSL_TIMESLOT_REQ_TYPE_EARLIEST 0
#define MPSL_TIMESLOT_HFCLK_CFG_NO_GUARANTEE 0
#define MPSL_TIMESLOT_PRIORITY_NORMAL 0
typedef struct {
    int request_type;
    union { struct { int hfclk, priority; uint32_t length_us, timeout_us; } earliest; } params;
} mpsl_timeslot_request_t;
typedef struct {
    int callback_action;
    union {
        struct { uint32_t length_us; } extend;
        struct { mpsl_timeslot_request_t *p_next; } request;
    } params;
} mpsl_timeslot_signal_return_param_t;
typedef mpsl_timeslot_signal_return_param_t *(*mpsl_timeslot_callback_t)(uint8_t, uint32_t);
static struct { uint32_t POWER; } mock_radio;
#define NRF_RADIO (&mock_radio)
#define RADIO_IRQn 1
#define RADIO_POWER_POWER_Disabled 0
#define RADIO_POWER_POWER_Enabled 1
#define RADIO_POWER_POWER_Pos 0
#define NRF_TIMER0 0
#define NRF_TIMER_BIT_WIDTH_32 32
#define NRF_TIMER_CC_CHANNEL0 0
#define NRF_TIMER_CC_CHANNEL1 1
#define NRF_TIMER_EVENT_COMPARE0 0
#define NRF_TIMER_EVENT_COMPARE1 1
#define NRF_TIMER_INT_COMPARE0_MASK 1
#define NRF_TIMER_INT_COMPARE1_MASK 2
static uint32_t compare[2], enabled;
static bool events[2];
static int started, stopped, radio_calls;
static void NVIC_ClearPendingIRQ(int irq) { (void)irq; }
static void NVIC_DisableIRQ(int irq) { (void)irq; }
static void nrf_timer_bit_width_set(int t, int b) { (void)t; (void)b; }
static void nrf_timer_cc_set(int t, int c, uint32_t v) { (void)t; compare[c] = v; }
static uint32_t nrf_timer_cc_get(int t, int c) { (void)t; return compare[c]; }
static void nrf_timer_int_enable(int t, uint32_t m) { (void)t; enabled |= m; }
static void nrf_timer_int_disable(int t, uint32_t m) { (void)t; enabled &= ~m; }
static bool nrf_timer_event_check(int t, int c) { (void)t; return events[c]; }
static void nrf_timer_event_clear(int t, int c) { (void)t; events[c] = false; }
void esb_mpsl_radio_irq_handler(void) { radio_calls++; }
static void mock_raw_radio(const void *p) { (void)p; radio_calls++; }
void *__ptr__radio_dynamic_irq_handler = (void *)mock_raw_radio;
static int queue[512], head, tail, worker_budget;
static int open_error, requested_id = -1, close_count, open_count, request_count;
static int request_error, close_error;
static jmp_buf worker_exit;
static int k_msgq_put(int *q, const void *item, int timeout) {
    (void)q; (void)timeout;
    assert(tail < 512);
    queue[tail++] = *(const int *)item;
    return 0;
}
static int k_msgq_get(int *q, void *item, int timeout) {
    (void)q; (void)timeout;
    if (!worker_budget-- || head == tail) longjmp(worker_exit, 1);
    *(int *)item = queue[head++];
    return 0;
}
static void k_sleep(int t) { (void)t; }
static int mpsl_timeslot_session_open(mpsl_timeslot_callback_t cb, uint8_t *id) {
    (void)cb;
    open_count++;
    if (open_error) return open_error;
    *id = 3;
    return 0;
}
static int mpsl_timeslot_request(uint8_t id, const mpsl_timeslot_request_t *p) {
    (void)p; requested_id = id; request_count++; return request_error;
}
static int mpsl_timeslot_session_close(uint8_t id) { (void)id; close_count++; return close_error; }
