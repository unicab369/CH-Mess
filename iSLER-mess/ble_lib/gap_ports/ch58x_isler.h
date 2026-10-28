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
    uint32_t access_address;
    uint8_t channel, tx_phy, rx_phy, receive_after_tx;
    const uint8_t *tx_frame;
} gap_link;

// GAP exposes one coded-PHY capability bit; iSLER distinguishes S=2 and S=8.
#ifndef GAP_CODED_PHY_MODE
#define GAP_CODED_PHY_MODE PHY_S8
#endif
#if GAP_CODED_PHY_MODE != PHY_S2 && GAP_CODED_PHY_MODE != PHY_S8
#error "GAP_CODED_PHY_MODE must be PHY_S2 or PHY_S8"
#endif

static uint8_t gap_phy_mode(uint8_t phy) {
    if (phy == GAP_PHY_2M)
        return PHY_2M;
    if (phy == GAP_PHY_CODED)
        return GAP_CODED_PHY_MODE;
    return PHY_1M;
}

// For different TX/RX rates, arm RX from TX completion within the peer's IFS.
void iSLER_Radio_Transmitted_Cb(void) {
    uint32_t status = tx_done;
    if (!(status & LL_TX)) return;
#if GAP_CENTRAL_SUPPORT
    if (gap_connect_tx.phase == GAP_CONNECT_TX_SENDING) {
        gap_connect_tx.end_ticks = GAP_HW_TICKS();
        gap_connect_tx.phase = GAP_CONNECT_TX_DONE;
        gs_iSLERLink.is_open = 0;
        return;
    }
#endif

#if GAP_EXT_ADV_SUPPORT
    if (gap_ext_adv_scan.response_started)
        gap_ext_adv_scan.waiting = 0;
#endif
    if (gap_link.receive_after_tx &&
        gap_link.tx_phy != gap_link.rx_phy) {
        iSLERLinkConfig(gap_link.access_address, gap_link.channel,
                       gap_phy_mode(gap_link.rx_phy), NULL, 0);
        iSLERLinkRX();
    }
}

const uint8_t *GAP_HW_RX_FRAME(void) { return (const uint8_t *)LLE_BUF; }
int8_t GAP_HW_RSSI(void) { return (int8_t)iSLERRSSI(); }
void GAP_HW_INIT(void) { iSLERInit(LL_TX_POWER_0_DBM); }

void GAP_HW_STOP(void) {
    gap_link.receive_after_tx = 0;
    iSLERStop();
    gs_iSLERLink.is_open = 0;
}

#if GAP_CENTRAL_SUPPORT

// iSLER assumes a 2 MHz LL25 timer on CH582: 240 counts target 120 us.
// Verify this rate and the on-air TX timing on hardware.
void GAP_HW_CONNECT_TX_TIMER_BEGIN(void) {
    LL->LLSTATUS = INT_ISLER_TMR0;
    LL->TMR0 = 240;
}

void GAP_HW_CONNECT_TX_TIMER_CANCEL(void) {
    gap_connect_tx.phase = GAP_CONNECT_TX_IDLE;
    LL->TMR0 = 0;
    LL->LLSTATUS = INT_ISLER_TMR0;
}

int GAP_HW_CONNECT_TX_TIMER_ARM(uint8_t *frame, uint8_t len, uint8_t channel) {
    gap_link.receive_after_tx = 0;
    frame[1] = (uint8_t)(len - 2);
    iSLERLinkConfig(BLE_ADV_ACCESS_ADDRESS, channel, PHY_1M, frame, 0);
    iSLERStop();
    iSLERSetMode(DEVMODE_TX);

    // Let the radio settle while TMR0 counts down to the TX start.
    // Leave enough time to exit the RX ISR before the timer expires.
    // At the assumed 2 MHz timer rate, 20 counts are 10 us; reject setup if
    // less than that remains or the timer interrupt has already fired.
    if (LL->TMR0 < 20 || (LL->LLSTATUS & INT_ISLER_TMR0)) {
        GAP_HW_CONNECT_TX_TIMER_CANCEL();
        GAP_HW_STOP();
        return 0;
    }
    return 1;
}

__HIGH_CODE void iSLER_Connect_Tx_Timer_Cb(void) {
    if (gap_connect_tx.phase != GAP_CONNECT_TX_ARMED) return;
    tx_done = 0;

    BB->CTRL_CFG |= CTRL_CFG_START_TX;
    BB->CTRL_TX &= ~((1 << 0) | (1 << 1));
    gap_connect_tx.phase = GAP_CONNECT_TX_SENDING;
    LL->LL0 = LL_TX;
}
#endif

int GAP_HW_ADV_TX(uint8_t *frame, uint8_t len, uint8_t channel) {
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

    gs_iSLERLink.is_open = 0;
    return tx_done != 0;
}

int GAP_HW_ADV_TX_PHY(
    uint8_t *frame, uint8_t len, uint8_t channel,
                      uint8_t phy
) {
    uint8_t mode = gap_phy_mode(phy);
    if (!(GAP_HW_ADV_PHY_MASK & phy)) return 0;

    gap_link.receive_after_tx = 0;
    iSLERTX(BLE_ADV_ACCESS_ADDRESS, frame, len, channel, mode);
    return tx_done != 0;
}

void GAP_HW_LINK_CONFIG(
    uint32_t access_address, uint8_t channel,
    uint8_t *tx_frame, uint8_t receive_after_tx,
    uint8_t tx_phy, uint8_t rx_phy
) {
    gap_link.access_address = access_address;
    gap_link.channel = channel;
    gap_link.tx_frame = tx_frame;
    gap_link.tx_phy = tx_phy;
    gap_link.rx_phy = rx_phy;
    gap_link.receive_after_tx = receive_after_tx;
    iSLERLinkConfig(access_address, channel,
                   gap_phy_mode(receive_after_tx ? tx_phy : rx_phy),
                   tx_frame, receive_after_tx && tx_phy == rx_phy);
}

uint8_t GAP_HW_PHY_MASK(void) {
#ifdef CH571_CH573
    return GAP_PHY_1M;
#elif defined(CH582_CH583) || defined(CH32V208)
    return GAP_PHY_1M | GAP_PHY_2M | GAP_PHY_CODED;
#else
    return GAP_PHY_1M | GAP_PHY_2M;
#endif
}

void GAP_HW_LINK_TX(void) {
    if (gs_iSLERLink.phy_mode != gap_phy_mode(gap_link.tx_phy)) {
        iSLERLinkConfig(gap_link.access_address, gap_link.channel,
                       gap_phy_mode(gap_link.tx_phy), (uint8_t *)gap_link.tx_frame,
                       gap_link.receive_after_tx && gap_link.tx_phy == gap_link.rx_phy);
    }

    iSLERLinkTX();
}

void GAP_HW_LINK_RX(void) { iSLERLinkRX(); }

void GAP_HW_SCAN_RX(uint8_t channel) {
    gap_link.receive_after_tx = 0;
    iSLERRX(BLE_ADV_ACCESS_ADDRESS, channel, PHY_1M);
}

void GAP_HW_TX_BUFFER(const uint8_t *frame) {
    gap_link.tx_frame = frame;
#ifdef CH571_CH573
    DMA->TXBUF = (uint32_t)frame;
#else
    LL->TXBUF = (uint32_t)frame;
#endif
}

void GAP_HW_CRC_INIT(uint32_t crc_init) {
    BB->CRCINIT1 = crc_init;
#ifdef CH570_CH572
    BB->CRCINIT2 = crc_init;
#endif
}

int GAP_HW_TX_DONE(void) { return (tx_done & LL_TX) != 0; }
void GAP_HW_TX_CLEAR_DONE(void) { tx_done = 0; }
uint64_t GAP_HW_TICKS(void) { return funSysTick64(); }
uint64_t HW_TICKS_FROM_US(uint32_t us) { return Ticks_from_Us(us); }

void GAP_HW_PUBLIC_ADDRESS(uint8_t address[6]) {
    const uint8_t *stored = (const uint8_t *)ROM_CFG_MAC_ADDR;
    for (uint8_t i = 0; i < 6; i++) address[i] = stored[5 - i];
}

#endif // GAP_PORT_CH58X_ISLER_H
