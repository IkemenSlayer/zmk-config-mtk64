/* Included after the actual dependency timeslot.c in a single translation unit. */
static void notify(zmk_split_esb_timeslot_callback_type_t type) {
    if (type == APP_TS_STARTED) started++; else stopped++;
}
static int signal_action(uint32_t signal) {
    mpsl_timeslot_signal_return_param_t *r = mpsl_timeslot_callback(3, signal);
    return r ? r->callback_action : MPSL_TIMESLOT_SIGNAL_ACTION_NONE;
}
static void start_slot(void) {
    zmk_split_esb_timeslot_init(notify);
    m_sess_open = true;
    assert(signal_action(MPSL_TIMESLOT_SIGNAL_START) == MPSL_TIMESLOT_SIGNAL_ACTION_NONE);
    assert(started == 1 && enabled == 3);
}
static void work_steps(int count) {
    worker_budget = count;
    if (setjmp(worker_exit) == 0) mpsl_nonpreemptible_thread();
}
int main(int argc, char **argv) {
    assert(argc == 2);
    const char *test = argv[1];
    if (!strcmp(test, "late_start")) {
        zmk_split_esb_timeslot_init(notify);
        m_sess_open = false;
        assert(signal_action(MPSL_TIMESLOT_SIGNAL_START) == MPSL_TIMESLOT_SIGNAL_ACTION_END);
        assert(started == 0 && enabled == 0);
    } else if (!strcmp(test, "open_failure_reproduced")) {
        open_error = -12;
        zmk_split_esb_timeslot_open_session();
        work_steps(2);
        assert(requested_id == 255); /* Known unfixed bug, not a success criterion. */
        puts("KNOWN DEFECT: open failure still requests with invalid session ID 255");
    } else if (!strcmp(test, "close_idle_reproduced")) {
        m_sess_open = true;
        zmk_split_esb_timeslot_close_session();
        signal_action(MPSL_TIMESLOT_SIGNAL_SESSION_IDLE);
        assert(tail == 2 && queue[0] == REQ_CLOSE_SESSION && queue[1] == REQ_MAKE_REQUEST);
        puts("KNOWN DEFECT: IDLE queues a request behind CLOSE");
    } else {
        start_slot();
        if (!strcmp(test, "close_timer")) {
            zmk_split_esb_timeslot_close_session();
            events[0] = true;
            assert(signal_action(MPSL_TIMESLOT_SIGNAL_TIMER0) == MPSL_TIMESLOT_SIGNAL_ACTION_END);
            assert(stopped == 1 && !m_in_timeslot && enabled == 0 && !events[0]);
            signal_action(MPSL_TIMESLOT_SIGNAL_RADIO);
            assert(radio_calls == 0);
        } else if (!strcmp(test, "close_extend")) {
            events[0] = true;
            assert(signal_action(MPSL_TIMESLOT_SIGNAL_TIMER0) == MPSL_TIMESLOT_SIGNAL_ACTION_EXTEND);
            zmk_split_esb_timeslot_close_session();
            assert(signal_action(MPSL_TIMESLOT_SIGNAL_EXTEND_SUCCEEDED) == MPSL_TIMESLOT_SIGNAL_ACTION_END);
            assert(stopped == 1 && enabled == 0);
        } else if (!strcmp(test, "no_event")) {
            events[0] = true;
            assert(signal_action(MPSL_TIMESLOT_SIGNAL_TIMER0) == MPSL_TIMESLOT_SIGNAL_ACTION_EXTEND);
            assert(signal_action(MPSL_TIMESLOT_SIGNAL_TIMER0) == MPSL_TIMESLOT_SIGNAL_ACTION_NONE);
        } else if (!strcmp(test, "extend_success")) {
            events[0] = true;
            assert(signal_action(MPSL_TIMESLOT_SIGNAL_TIMER0) == MPSL_TIMESLOT_SIGNAL_ACTION_EXTEND);
            uint32_t old = compare[0];
            assert(signal_action(MPSL_TIMESLOT_SIGNAL_EXTEND_SUCCEEDED) == MPSL_TIMESLOT_SIGNAL_ACTION_NONE);
            assert(compare[0] == old + 10000 && enabled == 3 && stopped == 0);
            signal_action(MPSL_TIMESLOT_SIGNAL_RADIO);
            assert(radio_calls == 1);
        } else if (!strcmp(test, "extend_failure")) {
            events[0] = true;
            signal_action(MPSL_TIMESLOT_SIGNAL_TIMER0);
            assert(signal_action(MPSL_TIMESLOT_SIGNAL_EXTEND_FAILED) == MPSL_TIMESLOT_SIGNAL_ACTION_NONE);
            assert(stopped == 1);
            events[1] = true;
            assert(signal_action(MPSL_TIMESLOT_SIGNAL_TIMER0) == MPSL_TIMESLOT_SIGNAL_ACTION_REQUEST);
            assert(signal_callback_return_param.params.request.p_next == &timeslot_request_earliest);
            assert(enabled == 0);
        } else { assert(!"unknown case"); }
    }
    printf("PASS %s\n", test);
    return 0;
}
