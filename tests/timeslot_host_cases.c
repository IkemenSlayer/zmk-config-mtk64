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
#ifdef SESSION_LIFECYCLE_PROBE
    zmk_split_esb_timeslot_open_session();
    service_session();
#endif
    m_sess_open = true;
    assert(signal_action(MPSL_TIMESLOT_SIGNAL_START) == MPSL_TIMESLOT_SIGNAL_ACTION_NONE);
    assert(started == 1 && enabled == 3);
}
static void work_steps(int count) {
#ifdef SESSION_LIFECYCLE_PROBE
    while (count--) service_session();
#else
    worker_budget = count;
    if (setjmp(worker_exit) == 0) mpsl_nonpreemptible_thread();
#endif
}
int main(int argc, char **argv) {
    assert(argc == 2);
    const char *test = argv[1];
    if (!strcmp(test, "late_start")) {
        zmk_split_esb_timeslot_init(notify);
        m_sess_open = false;
        assert(signal_action(MPSL_TIMESLOT_SIGNAL_START) == MPSL_TIMESLOT_SIGNAL_ACTION_END);
        assert(started == 0 && enabled == 0);
    } else if (!strcmp(test, "open_failure")) {
        open_error = -12;
        zmk_split_esb_timeslot_open_session();
        work_steps(2);
        assert(requested_id == -1);
#ifdef SESSION_LIFECYCLE_PROBE
        assert(!m_sess_open && session_state == SESSION_CLOSED);
        open_error = 0;
        work_steps(1);
        assert(requested_id == 3 && m_sess_open);
#endif
    } else if (!strcmp(test, "close_idle")) {
        zmk_split_esb_timeslot_open_session();
        work_steps(2);
        requested_id = -1;
        zmk_split_esb_timeslot_close_session();
        signal_action(MPSL_TIMESLOT_SIGNAL_SESSION_IDLE);
        work_steps(2);
        assert(requested_id == -1);
#ifdef SESSION_LIFECYCLE_PROBE
    } else if (!strcmp(test, "reopen_waits")) {
        start_slot();
        zmk_split_esb_timeslot_close_session();
        work_steps(1);
        zmk_split_esb_timeslot_open_session();
        work_steps(3);
        assert(open_count == 1 && close_count == 1 && request_count == 1);
        assert(signal_action(MPSL_TIMESLOT_SIGNAL_TIMER0) == MPSL_TIMESLOT_SIGNAL_ACTION_END);
        signal_action(MPSL_TIMESLOT_SIGNAL_SESSION_CLOSED);
        work_steps(1);
        assert(open_count == 2 && request_count == 2 && m_sess_open);
    } else if (!strcmp(test, "busy_request")) {
        request_error = -NRF_EAGAIN;
        zmk_split_esb_timeslot_open_session();
        work_steps(5);
        assert(open_count == 1 && close_count == 0 && request_count == 1);
        request_error = 0;
        signal_action(MPSL_TIMESLOT_SIGNAL_SESSION_IDLE);
        work_steps(1);
        assert(request_count == 2 && close_count == 0);
    } else if (!strcmp(test, "missing_session")) {
        request_error = -NRF_ENOENT;
        zmk_split_esb_timeslot_open_session();
        work_steps(1);
        assert(!m_sess_open && request_count == 1);
        request_error = 0;
        work_steps(1);
        assert(open_count == 2 && requested_id == 3 && request_count == 2);
    } else if (!strcmp(test, "close_retry")) {
        start_slot();
        close_error = -99;
        zmk_split_esb_timeslot_close_session();
        work_steps(2);
        assert(close_count == 2 && !m_sess_open && open_count == 1);
        close_error = 0;
        work_steps(2);
        assert(close_count == 3 && session_state == SESSION_CLOSING);
        signal_action(MPSL_TIMESLOT_SIGNAL_SESSION_CLOSED);
        work_steps(1);
        assert(session_state == SESSION_CLOSED && open_count == 1);
    } else if (!strcmp(test, "close_already_closed")) {
        start_slot();
        close_error = -NRF_EAGAIN;
        zmk_split_esb_timeslot_close_session();
        work_steps(2);
        assert(session_state == SESSION_CLOSED && close_count == 1);
    } else if (!strcmp(test, "coalesced_requests")) {
        for (int i = 0; i < 1000; i++) {
            zmk_split_esb_timeslot_open_session();
            zmk_split_esb_timeslot_close_session();
        }
        work_steps(1);
        assert(open_count == 0 && request_count == 0);
        zmk_split_esb_timeslot_open_session();
        work_steps(1);
        for (int i = 0; i < 1000; i++) signal_action(MPSL_TIMESLOT_SIGNAL_SESSION_IDLE);
        work_steps(1);
        assert(open_count == 1 && request_count == 2);
#endif
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
