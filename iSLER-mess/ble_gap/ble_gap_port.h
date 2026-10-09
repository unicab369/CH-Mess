// Platform services required by the GAP controller and security procedures.
// Applications provide these functions; this header contains declarations
// only and is safe to include independently.
#ifndef GAP_PORT_H
#define GAP_PORT_H

#include <stddef.h>
#include <stdint.h>

#ifndef GAP_RADIO_BUFFER_ATTR
#define GAP_RADIO_BUFFER_ATTR __attribute__((aligned(4)))
#endif

uint32_t GET_MILLIS(void);
// Standard AES byte order; the platform must serialize shared hardware use.
void AES_ENCRYPT_BLOCK(const uint8_t *key, const uint8_t *in, uint8_t *out);

// Platform radio hooks. Frames and addresses use Bluetooth on-air byte order.
const uint8_t *GAP_HW_RX_FRAME(void);
int8_t GAP_HW_RSSI(void);
void GAP_HW_INIT(void);
void GAP_HW_STOP(void);
int GAP_HW_ADV_TX(uint8_t *frame, uint8_t len, uint8_t channel);
// Capability and transmitter for secondary-channel advertising PHYs.
uint8_t GAP_HW_ADV_PHY_MASK(void);
int GAP_HW_ADV_TX_PHY(
    uint8_t *frame, uint8_t len, uint8_t channel,
                          uint8_t phy);
void GAP_HW_LINK_CONFIG(
    uint32_t access_address, uint8_t channel,
                            uint8_t *tx_frame, uint8_t receive_after_tx,
                            uint8_t tx_phy, uint8_t rx_phy);
// Maximum unencrypted data payload supported by the radio (27..251 bytes).
uint16_t GAP_HW_DATA_MAX(void);
// Supported PHY mask (1M is mandatory); configure TX and RX independently.
uint8_t GAP_HW_PHY_MASK(void);
void GAP_HW_LINK_TX(void);
void GAP_HW_LINK_RX(void);
void GAP_HW_SCAN_RX(uint8_t channel);
void GAP_HW_TX_BUFFER(const uint8_t *frame);
void GAP_HW_CRC_INIT(uint32_t crc_init);
int GAP_HW_TX_DONE(void);
void GAP_HW_TX_CLEAR_DONE(void);
uint64_t GAP_HW_TICKS(void);
uint64_t HW_TICKS_FROM_US(uint32_t us);
void GAP_HW_PUBLIC_ADDRESS(uint8_t address[6]);
void GAP_HW_PACKET_READY(void);
void GAP_HW_PACKET_CLEAR(void);
uint8_t GAP_HW_RANDOM_JITTER(void);
void GAP_HW_RANDOM_BYTES(uint8_t *out, size_t len);
// Platform secure-entropy interface: fill all requested bytes with
// cryptographic randomness, or return 0 when unavailable. Never use the
// advertising jitter PRNG here. This hook may run in the RX interrupt.
int GAP_RANDOM_SECURE_BYTES(uint8_t *out, size_t len);
// Save/restore interrupt state around foreground key-state updates.
uint32_t GAP_CRITICAL_ENTER(void);
void GAP_CRITICAL_EXIT(uint32_t state);
// Standard AES key/nonce byte order, in-place CCM, one AAD byte, four-byte MIC.
int GAP_CCM_ENCRYPT(
    const uint8_t key[16], const uint8_t nonce[13],
                        uint8_t aad, uint8_t *data, size_t len, uint8_t mic[4]);
int GAP_CCM_DECRYPT(
    const uint8_t key[16], const uint8_t nonce[13],
                        uint8_t aad, uint8_t *data, size_t len, const uint8_t mic[4]);


#endif // GAP_PORT_H
