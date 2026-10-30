// CH58x/iSLER implementation of the GAP platform hooks.
// Include after ble_lib/ble_gap.h, iSLER.h, ble_lib/ble_crypto.h, and aes_cmm.h.
#ifndef GAP_PORT_CH58X_ISLER_H
#define GAP_PORT_CH58X_ISLER_H


// Application must override this with a cryptographic entropy source before
// using GAP features that require random values. Weak pseudo-random bytes are
// not a safe fallback for addresses, connection parameters, or security material.
__attribute__((weak)) int GAP_RANDOM_BYTES(uint8_t *out, size_t len) {
    (void)out; (void)len;
    return 0;
}

// Bond storage is application-owned until a reserved persistent region is
// assigned. The generic GAP bond API safely reports storage as unavailable.
#if GAP_SECURITY_SUPPORT >= 2
__attribute__((weak)) int GAP_BOND_LOAD(uint8_t slot, gap_bond *bond) {
    (void)slot;
    if (bond)
        memset(bond, 0, sizeof(*bond));
    return -1;
}
__attribute__((weak)) int GAP_BOND_SAVE(uint8_t slot, const gap_bond *bond) {
    (void)slot; (void)bond;
    return 0;
}
__attribute__((weak)) int GAP_BOND_DELETE(uint8_t slot) {
    (void)slot;
    return 0;
}
#endif

uint32_t GAP_CRITICAL_ENTER(void) {
    uint32_t state = __get_MSTATUS();
    __disable_irq();
    return state;
}
void GAP_CRITICAL_EXIT(uint32_t state) { __set_MSTATUS(state); }
int GAP_CCM_ENCRYPT(
    const uint8_t key[16], const uint8_t nonce[13],
                    uint8_t aad, uint8_t *data, size_t len, uint8_t mic[4]
) {
    return ccm_encrypt_and_tag(key, nonce, 13, &aad, 1, data, len,
                               data, mic, 4) == CCM_OK;
}
int GAP_CCM_DECRYPT(
    const uint8_t key[16], const uint8_t nonce[13],
                    uint8_t aad, uint8_t *data, size_t len,
                    const uint8_t mic[4]
) {
    return ccm_auth_decrypt(key, nonce, 13, &aad, 1, data, len,
                            mic, 4, data) == CCM_OK;
}

// iSLER adapter state for GAP's connection and advertising radio operations.
static struct {
    uint64_t tx_due_ticks;
    uint32_t access_address;
    uint8_t channel, tx_phy, rx_phy, receive_after_tx;
    volatile uint8_t reply_armed;
    volatile uint8_t ifs_tx_active;
    volatile uint8_t scheduled_tx_armed;
} gap_link;

static void gap_radio_tx_buffer(const uint8_t *frame) {
#ifdef CH571_CH573
    DMA->TXBUF = (uint32_t)frame;
#else
    LL->TXBUF = (uint32_t)frame;
#endif
}

// GAP exposes one coded-PHY capability bit; iSLER distinguishes S=2 and S=8.
#ifndef GAP_CODED_PHY_MODE
#define GAP_CODED_PHY_MODE PHY_S8
#endif
#if GAP_CODED_PHY_MODE != PHY_S2 && GAP_CODED_PHY_MODE != PHY_S8
#error "GAP_CODED_PHY_MODE must be PHY_S2 or PHY_S8"
#endif

static uint8_t gap_phy_mode(uint8_t phy) {
    return phy == GAP_PHY_CODED ? GAP_CODED_PHY_MODE
                                : phy == GAP_PHY_2M
                                ? PHY_2M : PHY_1M;
}

// For different TX/RX rates, arm RX from TX completion within the peer's IFS.
void iSLER_Radio_Transmitted_Cb(void) {
    if (!(tx_done & LL_TX)) return;
#if GAP_CENTRAL_SUPPORT
    if (gap_connect_tx.phase == GAP_CONNECT_TX_SENDING) {
        gap_connect_tx.end_ticks = RADIO_TICKS();
        gap_connect_tx.phase = GAP_CONNECT_TX_DONE;
        return;
    }
#endif

#if GAP_EXT_ADV_SUPPORT
    if (gap_periodic_train.active) {
        gap_extended_adv_set *set = &gap_ext_adv[gap_periodic_train.set_id];
        if (gap_periodic_train.offset < set->periodic.data_len &&
            !gap_periodic_train_arm_next()) {
            gap_periodic_train.active = 0;
            gap_periodic_train.failed = 1;
        }
        return;
    }
    if (gap_ext_aux.phase == GAP_EXT_AUX_PRIMARY) {
        gap_ext_aux_primary_done();
        return;
    }
    if (gap_ext_aux.phase == GAP_EXT_AUX_PENDING &&
        gap_ext_adv[gap_ext_aux.set_id].scannable) {
        gap_ext_aux_rx_start();
        return;
    }

#if GAP_PERIPHERAL_SUPPORT
    if (gap_ext_aux.phase == GAP_EXT_AUX_PENDING &&
        gap_ext_adv[gap_ext_aux.set_id].connectable) {
        gap_ext_aux_rx_start();
        return;
    }
    if (gap_ext_adv_connect.response_started)
        gap_ext_adv_connect.waiting = 0;
#endif

#if GAP_CENTRAL_SUPPORT
    if (gap_ext_pawr.connect.phase == GAP_PAWR_CONNECT_TX_PENDING) {
        gap_ext_pawr.connect.request_end_ticks = RADIO_TICKS();
        gap_ext_pawr.connect.tx_end_valid = 1;
    }
#endif
    if (gap_ext_adv_scan.response_started)
        gap_ext_adv_scan.waiting = 0;
#endif

    if (gap_link.ifs_tx_active) {
        gap_link.ifs_tx_active = 0;

#if GAP_EXT_ADV_SUPPORT
        if (gap_ext_aux.phase == GAP_EXT_AUX_SCAN_WINDOW &&
            gap_ext_adv_scan.response_started) {
            gap_link.receive_after_tx = 0;
            gap_extended_adv_set *set = &gap_ext_adv[gap_ext_aux.set_id];
            if (set->scan_resp_len > GAP_EXT_ADV_FINAL_PDU_DATA_MAX - 4) {
                uint16_t remaining = set->scan_resp_len - GAP_EXT_ADV_FIRST_PDU_DATA_MAX;
                uint64_t next_start = gap_ext_adv_scan.response_ticks + HW_TICKS_FROM_US(
                    ((gap_phy_airtime_us(gap_ext_adv_frame[1], set->aux_phy) + 629) / 30) * 30);
                gap_ext_aux.phase = GAP_EXT_AUX_IDLE;
                if (!gap_ext_chain_send(gap_ext_aux.set_id, remaining, gap_ext_aux.adi,
                                        gap_ext_aux.address_type, next_start))
                    gap_ext_aux.phase = GAP_EXT_AUX_FAILED;
            }
            return;
        }

        if (gap_ext_scan.request_phase == GAP_EXT_REQ_SCAN_TX)
            gap_ext_scan.request_phase = GAP_EXT_REQ_SCAN_RX;
        else if (gap_ext_scan.request_phase == GAP_EXT_REQ_CONNECT_TX) {
            gap_ext_scan.request_end_ticks = RADIO_TICKS();
            gap_ext_scan.request_phase = GAP_EXT_REQ_CONNECT_RX;
        }
#endif
        if (gap_link.receive_after_tx) {
            if (gap_link.tx_phy != gap_link.rx_phy)
                iSLERLinkConfig(gap_link.access_address, gap_link.channel,
                                gap_phy_mode(gap_link.rx_phy), NULL, 0);
            iSLERLinkRX();
        }
        return;
    }
    if (gap_link.receive_after_tx &&
        gap_link.tx_phy != gap_link.rx_phy) {
        iSLERLinkConfig(gap_link.access_address, gap_link.channel,
                       gap_phy_mode(gap_link.rx_phy), NULL, 0);
        iSLERLinkRX();
    }
}

const uint8_t *RADIO_RX_FRAME(void) {
    return (const uint8_t *)LLE_BUF;
}

int8_t RADIO_RSSI(void) {
    return (int8_t)iSLERRSSI();
}

void RADIO_INIT(void) {
    iSLERInit(LL_TX_POWER_0_DBM);
}

void RADIO_STOP(void) {
    gap_link.reply_armed = 0;
    gap_link.ifs_tx_active = 0;
    gap_link.scheduled_tx_armed = 0;
    gap_link.receive_after_tx = 0;
    iSLERStop();
}

// Both connection initiation and connection replies use this deadline.
// Radio configuration may overwrite TMR0, so program it after setup.
// TODO: Replace this provisional 120 us target after measuring RX-end to
// TX-start timing and the TMR0 rate on CH582 hardware.
#define GAP_IFS_TX_TARGET_US 120
void RADIO_TX_TIMER_BEGIN(void) {
    gap_link.tx_due_ticks = RADIO_TICKS() + HW_TICKS_FROM_US(GAP_IFS_TX_TARGET_US);
}

static int gap_tx_timer_arm(void) {
    uint64_t now = RADIO_TICKS();

    if (now >= gap_link.tx_due_ticks ||
        gap_link.tx_due_ticks - now < HW_TICKS_FROM_US(10)
    ) return 0;

    uint64_t remaining_us = (gap_link.tx_due_ticks - now) / HW_TICKS_FROM_US(1);
    if (remaining_us < 10) return 0;

    tx_done = 0;
    LL->LLSTATUS = INT_ISLER_TMR0;
    LL->TMR0 = (uint32_t)(remaining_us * 2);
    return 1;
}

#if GAP_CENTRAL_SUPPORT

void RADIO_CONNECT_TX_TIMER_CANCEL(void) {
    gap_connect_tx.phase = GAP_CONNECT_TX_IDLE;
    LL->TMR0 = 0;
    LL->LLSTATUS = INT_ISLER_TMR0;
}

int RADIO_CONNECT_TX_TIMER_ARM(uint8_t *frame, uint8_t len, uint8_t channel) {
    gap_link.receive_after_tx = 0;
    frame[1] = (uint8_t)(len - 2);
    iSLERLinkConfig(BLE_ADV_ACCESS_ADDRESS, channel, PHY_1M, frame, 0);
    iSLERStop();
    iSLERSetMode(DEVMODE_TX);

    if (!gap_tx_timer_arm()) {
        RADIO_CONNECT_TX_TIMER_CANCEL();
        RADIO_STOP();
        return 0;
    }
    return 1;
}

#endif

int RADIO_IFS_TX_TIMER_ARM(const uint8_t *frame) {
    if (gs_iSLERLink.phy_mode != gap_phy_mode(gap_link.tx_phy)) {
        iSLERLinkConfig(gap_link.access_address, gap_link.channel,
                       gap_phy_mode(gap_link.tx_phy),
                       NULL,
                       gap_link.receive_after_tx && gap_link.tx_phy == gap_link.rx_phy);
    }
    gap_radio_tx_buffer(frame);

    iSLERStop();
    iSLERSetMode(DEVMODE_TX);
    LL->LL1 &= ~LL_AUTO;

    if (!gap_tx_timer_arm()) {
        RADIO_STOP();
        return 0;
    }
    gap_link.reply_armed = 1;
    gap_link.ifs_tx_active = 1;
    return 1;
}

// Start the configured link TX at an absolute controller tick. This has a
// separate logical arm state from IFS replies, though both use hardware TMR0.
// Link configuration must be complete first.
int RADIO_LINK_TX_AT(const uint8_t *frame, uint64_t start_ticks) {
    // TMR0 is a shared hardware resource, but this absolute event has its own
    // logical arm state and must not replace an IFS reply already in flight.
    if (gap_link.reply_armed || gap_link.ifs_tx_active ||
        gap_link.scheduled_tx_armed
    ) return 0;

    if (gs_iSLERLink.phy_mode != gap_phy_mode(gap_link.tx_phy)) {
        iSLERLinkConfig(gap_link.access_address, gap_link.channel,
                       gap_phy_mode(gap_link.tx_phy), NULL,
                       gap_link.receive_after_tx && gap_link.tx_phy == gap_link.rx_phy);
    }
    gap_radio_tx_buffer(frame);
    gap_link.tx_due_ticks = start_ticks;
    iSLERStop();
    iSLERSetMode(DEVMODE_TX);
    LL->LL1 &= ~LL_AUTO;

    if (!gap_tx_timer_arm()) {
        RADIO_STOP();
        return 0;
    }
    gap_link.scheduled_tx_armed = 1;
    return 1;
}

__HIGH_CODE void iSLER_Tx_Timer_Cb(void) {
    if (gap_link.reply_armed) {
        gap_link.reply_armed = 0;
    } else if (gap_link.scheduled_tx_armed) {
        gap_link.scheduled_tx_armed = 0;
    }
#if GAP_CENTRAL_SUPPORT
    else if (gap_connect_tx.phase == GAP_CONNECT_TX_ARMED) {
        gap_connect_tx.phase = GAP_CONNECT_TX_SENDING;
    }
    else return;
#else
    else return;
#endif
    tx_done = 0;

    BB->CTRL_CFG |= CTRL_CFG_START_TX;
    BB->CTRL_TX &= ~((1 << 0) | (1 << 1));
    LL->LL0 = LL_TX;
}

int RADIO_ADV_TX(uint8_t *frame, uint8_t len, uint8_t channel) {
    gap_link.receive_after_tx = 0;
    frame[1] = (uint8_t)(len - 2);
    iSLERLinkConfig(BLE_ADV_ACCESS_ADDRESS, channel, PHY_1M, frame, 0);

// iSLERLinkTX()
    iSLERStop();
    iSLERSetMode(DEVMODE_TX);

    // // Wait for the radio to settle after switching to TX mode.
    // // RF26 bit 24 still needs verification on CH582.
    // for (int timeout = Ticks_from_Us(100);
    //      !(RF->RF26 & 0x1000000) && timeout >= 0; timeout--);

    tx_done = 0;
    BB->CTRL_CFG |= CTRL_CFG_START_TX;
    BB->CTRL_TX &= ~((1 << 0) | (1 << 1));
    LL->LL0 = LL_TX;

    return tx_done != 0;
}

void RADIO_LINK_CONFIG(
    uint32_t access_address, uint8_t channel,
    uint8_t receive_after_tx,
    uint8_t tx_phy, uint8_t rx_phy, uint32_t crc_init
) {
    gap_link.access_address = access_address;
    gap_link.channel = channel;
    gap_link.tx_phy = tx_phy;
    gap_link.rx_phy = rx_phy;
    gap_link.receive_after_tx = receive_after_tx;

    iSLERLinkConfig(access_address, channel,
                   gap_phy_mode(receive_after_tx ? tx_phy : rx_phy),
                   NULL, receive_after_tx && tx_phy == rx_phy);
    BB->CRCINIT1 = crc_init;
#ifdef CH570_CH572
    BB->CRCINIT2 = crc_init;
#endif
}

uint8_t RADIO_PHY_MASK(void) {
    uint8_t phy_mask = GAP_PHY_1M;
#ifndef CH571_CH573
    phy_mask |= GAP_PHY_2M;
#endif
#if defined(CH582_CH583) || defined(CH32V208)
    phy_mask |= GAP_PHY_CODED;
#endif
    return phy_mask;
}

void RADIO_LINK_TX(const uint8_t *frame) {
    if (gs_iSLERLink.phy_mode != gap_phy_mode(gap_link.tx_phy)) {
        iSLERLinkConfig(gap_link.access_address, gap_link.channel,
                       gap_phy_mode(gap_link.tx_phy),
                       NULL,
                       gap_link.receive_after_tx && gap_link.tx_phy == gap_link.rx_phy);
    }

    gap_radio_tx_buffer(frame);
    iSLERLinkTX();
}

void RADIO_LINK_RX(void) {
    iSLERLinkRX();
}

void RADIO_SCAN_RX(uint8_t channel) {
    gap_link.receive_after_tx = 0;
    iSLERLinkConfig(BLE_ADV_ACCESS_ADDRESS, channel, PHY_1M, NULL, 0);
    iSLERLinkRX();
}

int RADIO_TX_DONE(void) {
    return (tx_done & LL_TX) != 0;
}
void RADIO_TX_CLEAR_DONE(void) {
    tx_done = 0;
}

uint64_t RADIO_TICKS(void) {
    return funSysTick64();
}

uint64_t HW_TICKS_FROM_US(uint32_t us) {
    return Ticks_from_Us(us);
}

void RADIO_PUBLIC_ADDRESS(uint8_t address[6]) {
    const uint8_t *stored = (const uint8_t *)ROM_CFG_MAC_ADDR;
    for (uint8_t i = 0; i < 6; i++) address[i] = stored[5 - i];
}

#endif // GAP_PORT_CH58X_ISLER_H
