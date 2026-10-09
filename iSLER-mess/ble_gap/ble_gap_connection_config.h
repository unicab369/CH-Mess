// Central initiation and initial connection timing configuration.
// Included after advertising and per-link state declarations.
#ifndef GAP_CONNECTION_CONFIG_H
#define GAP_CONNECTION_CONFIG_H

// Initial LE connection settings. Connection interval is in 1.25 ms units,
// supervision timeout in 10 ms units, and attempt timeout in milliseconds.
typedef struct {
    uint16_t interval, latency, supervision_timeout;
    uint16_t background_scan_interval_ms, background_scan_window_ms;
    uint32_t attempt_timeout_ms;
} gap_connection_timing;

static struct {
    uint8_t active, any_peer, selective, auto_connect;
    uint8_t peer_type, peer_address[6], request[36];
    uint32_t deadline_ms;
} gap_central_connect;
static gap_connection_timing gap_connection_timing = {
    24, 0, 200, 1280, 12, 30720
};


// Configure initial Central connection parameters and the finite scan timeout
// used by Direct, General, and Selective Connection Establishment.
int gap_connection_timing_set(const gap_connection_timing *timing) {
    if (!timing || timing->interval < 6 || timing->interval > 3200 ||
        timing->latency > 499 || timing->supervision_timeout < 10 ||
        timing->supervision_timeout > 3200 || !timing->attempt_timeout_ms ||
        timing->attempt_timeout_ms > 0x7fffffffUL || gap_scanning ||
        gap_advertising.enabled || GAP_EXT_ADVERTISING_ENABLED || gap_conn.active ||
        gap_central_connect.active || timing->background_scan_interval_ms < 3 ||
        timing->background_scan_interval_ms >= 40960 ||
        timing->background_scan_window_ms < 3 ||
        timing->background_scan_window_ms > timing->background_scan_interval_ms)
        return 0;
    if ((uint32_t)timing->supervision_timeout * 4u <=
        (uint32_t)(timing->latency + 1u) * timing->interval)
        return 0;
    gap_connection_timing = *timing;
    return 1;
}

// Read the current initial connection and attempt timing settings.
int gap_connection_timing_get(gap_connection_timing *timing) {
    if (!timing) return 0;
    *timing = gap_connection_timing;
    return 1;
}

#endif // GAP_CONNECTION_CONFIG_H
