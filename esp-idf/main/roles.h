#pragma once

#include <stdint.h>

// Device roles. Stored in NVS; keep the numbers stable when adding roles
// (ranging master/slave come next).
enum Role : uint8_t {
    ROLE_TX = 0,
    ROLE_RX = 1,
    ROLE_COUNT
};

const char *role_name(Role r);

// Each role runs forever.
void role_tx_run(void);
void role_rx_run(void);
