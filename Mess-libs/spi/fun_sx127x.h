// Stolen from https://github.com/sandeepmistry/arduino-LoRa

#include "ch32fun.h"
#include <stdint.h>

//! ####################################
//! SPI FUNCTIONS
//! ####################################

u8 LORA_CS_PIN = -1;

u8 sx127x_transfer(u8 reg, u8 value) {
    u8 resp;

    if (LORA_CS_PIN != -1) funDigitalWrite(LORA_CS_PIN, 0);
    resp = SPI_transfer_8(reg);
    resp = SPI_transfer_8(value);
    if (LORA_CS_PIN != -1) funDigitalWrite(LORA_CS_PIN, 1);
    return resp;
}

u8 sx127x_read(u8 reg) {
    return sx127x_transfer(reg & 0x7f, 0x00);
}

u8 sx127x_write(u8 reg, u8 value) {
    return sx127x_transfer(reg | 0x80, value);
}


//! ####################################
//! SETUP FUNCTIONS
//! ####################################

#define PA_BOOST                    0x80

void fun_sx127x_setFrequency(uint32_t frequency) {
    uint64_t frf = ((uint64_t)frequency << 19) / 32E6;

    //# 0x06: freq_MSB, 0x07: freq_MID, 0x08: freq_LSB
    sx127x_write(0x06, (u8)(frf >> 16));
    sx127x_write(0x07, (u8)(frf >> 8));
    sx127x_write(0x08, (u8)(frf >> 0));

    // u8 fre0 = sx127x_read(REG_FRF_MSB);        // expect 0xE4 for 915Mhz
    // u8 fre1 = sx127x_read(REG_FRF_MID);        // expect 0xE4 for 915Mhz
    // u8 fre2 = sx127x_read(REG_FRF_LSB);        // expect 0x00 for 915Mhz
    // printf("Freq Read: 0x%02X 0x%02X 0x%02X\r\n", fre0, fre0, fre2);
}

void sx772xx_setOCP(u8 mA) {
    u8 ocpTrim = 27;

    if (mA <= 120) {
        ocpTrim = (mA - 45) / 5;
    } else if (mA <=240) {
        ocpTrim = (mA + 30) / 10;
    }

    //# 0x0B: RegOcp
    sx127x_write(0x0B, 0x20 | (0x1F & ocpTrim));
}

void fun_sx127x_setTxPower(u8 level) {
    //# 0x4D: RegPaDac
    u8 RegPaDac = 0x4D;

    if (level > 17) {
        if (level > 20) {
            level = 20;
        }

        // subtract 3 from level, so 18 - 20 maps to 15 - 17
        level -= 3;

        // High Power +20 dBm Operation (Semtech SX1276/77/78/79 5.4.3.)
        sx127x_write(RegPaDac, 0x87);
        sx772xx_setOCP(140);
    } else {
        if (level < 2) {
            level = 2;
        }

        sx127x_write(RegPaDac, 0x84);
        sx772xx_setOCP(100);
    }

    //# 0x09: RegPaConfig
    sx127x_write(0x09, PA_BOOST | (level - 2));
}

// Modem Config
#define REG_MODEM_CONFIG_2          0x1E

// Bandwidth
#define SX127X_BW_78000             0x00
#define SX127X_BW_10400             0x01
#define SX127X_BW_15600             0x02
#define SX127X_BW_20800             0x03
#define SX127X_BW_31250             0x04
#define SX127X_BW_41700             0x05
#define SX127X_BW_62500             0x06
#define SX127X_BW_125000            0x07
#define SX127X_BW_250000            0x08
#define SX127X_BW_500000            0x09


void fun_sx127x_config1(u8 headerMode, u8 cr, u8 bw) {
    // bit0 (LSB): 0 = explicit header mode, 1 = implicit header mode
    headerMode = headerMode & 0x01; // Ensure 1 bit value

    // bit1-3: Coding Rate. CR1 = 4/5, CR2 = 4/6, CR3 = 4/7, CR4 = 4/8
    cr = cr < 1 ? 1 : cr;
    cr = cr > 4 ? 4 : cr;
    cr = cr & 0b00000111;   // Ensure 3 bit value

    // bit4-7: Bandwidth
    bw = bw & 0b00001111;   // ensure 4 bit value

    //# 0x1D: RegModemConfig1
    u8 configValue = (bw << 4) | (cr << 1) | headerMode ;
    sx127x_write(0x1D, configValue);
}


//! ####################################
//! INIT FUNCTIONS
//! ####################################

#define SX127X_REG_OP_MODE          0x01
#define SX127X_LONGRANGE_MODE       0b10000000      // bit7: 1 = LoRa, 0 = FSK/OOK
#define SX127X_MODE_SLEEP           0b00000000      // bit0-2: 0 = Sleep
#define SX127X_MODE_STDBY           0b00000001      // bit0-2: 1 = Standby
#define SX127X_MODE_TX              0b00000011      // bit0-2: 3 = TX
#define SX127X_MODE_RX_SINGLE       0b00000110      // bit0-2: 6 = RX Single

#define SX127X_FIFO_RX_CURRENTADDR      0x10

u8 LORA_OK = 0;

void sx127x_setMode(u8 mode) {
    sx127x_write(SX127X_REG_OP_MODE, SX127X_LONGRANGE_MODE | mode);
}

u8 sx127x_getMode() {
    // mask the first 3 bits
    return sx127x_read(SX127X_REG_OP_MODE) & 0b00000111;
}

void fun_sx127x_init(uint32_t frequency, u8 cs_pin) {
    //! configure CS Pin
    if (cs_pin != -1) {
        LORA_CS_PIN = cs_pin;
        funPinMode(cs_pin, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP);
        funDigitalWrite(cs_pin, 1);
    }

    u8 version = sx127x_read(0x42);
    printf("LoRa version: 0x%02x\n", version);         // expect 0x12
    LORA_OK = version == 0x12;

    //! Set Mode sleep (*REQUIRED*)
    sx127x_setMode(SX127X_MODE_SLEEP);

    //# Set frequency
    fun_sx127x_setFrequency(frequency);

    //# 0x1D: RegModemConfig1 (headerMode, cr, bw)
    fun_sx127x_config1(0, 1, SX127X_BW_125000);

    //# 0x0E: TX_baseAddr, 0x0F: RX_baseAddr
    sx127x_write(0x0E, 0);
    sx127x_write(0x0F, 0);

    //# 0x0C: RegLna
    // 0b00: boost off (default current), 0b11: boost on (150% LNA current)
    u8 read = sx127x_read(0x0C);
    sx127x_write(0x0C, read | 0b11);

    //# 0x26: RegModemConfig3
    read = sx127x_read(0x26);
    printf("Config3: 0x%02X\n", read);          // expect 0x04

    sx127x_write(0x26, 0x04);
    // u8 read_modem = sx127x_read(REG_MODEM_CONFIG_3);
    // printf("Modem: 0x%02X\n", read_modem);          // expect 0x04

    sx127x_setMode(SX127X_MODE_STDBY);
}


//! ####################################
//! TRANSMITION FUNCTIONS
//! ####################################

#define REG_FIFO_ADDR_PTR           0x0D
#define REG_PAYLOAD_LENGTH          0x22
#define REG_IRQ_FLAGS               0x12
#define IRQ_TX_DONE_MASK            0x08
#define REG_FIFO                    0x00

void fun_sx127x_send(u8 *data, u8 size) {
    if (!LORA_OK) {
        printf("Err: LoRa not initialized\n");
        return;
    }

    // reset FIFO address and payload length
    sx127x_write(REG_FIFO_ADDR_PTR, 0);
    sx127x_write(REG_PAYLOAD_LENGTH, 0);

    //# write data
    for (int i = 0; i < size; i++) {
        sx127x_write(REG_FIFO, data[i]);
    }

    // update len
    sx127x_write(REG_PAYLOAD_LENGTH, size);

    //# Send packet
    sx127x_setMode(SX127X_MODE_TX);
    Delay_Ms(1);

    // read = sx127x_read(REG_IRQ_FLAGS);
    // printf("IRQ: 0x%02X\n", read);
    
    //# clear IRQ's
    sx127x_write(REG_IRQ_FLAGS, IRQ_TX_DONE_MASK);
}


//! ####################################
//! RECEIVING FUNCTIONS
//! ####################################

#define IRQ_RX_DONE_MASK            0x40
#define IRQ_PAYLOAD_CRC_ERROR_MASK  0x20
#define REG_RX_NB_BYTES             0x13

int LORA_PACKET_INDEX;

int fun_sx127x_parsePacket() {
    if (!LORA_OK) return 0;
    int packetLength = 0;

    // clear IRQ's
    int irqFlags = sx127x_read(REG_IRQ_FLAGS);
    sx127x_write(REG_IRQ_FLAGS, irqFlags);

    if ((irqFlags & IRQ_RX_DONE_MASK) && (irqFlags & IRQ_PAYLOAD_CRC_ERROR_MASK) == 0) {
        // received a packet
        LORA_PACKET_INDEX = 0;

        // read packet length
        // if (_implicitHeaderMode) {
        //     packetLength = sx127x_read(REG_PAYLOAD_LENGTH);
        // } else {
            packetLength = sx127x_read(REG_RX_NB_BYTES);
        // }

        // set FIFO address to current RX address
        u8 rxAdress = sx127x_read(SX127X_FIFO_RX_CURRENTADDR);
        sx127x_write(REG_FIFO_ADDR_PTR, rxAdress);

        // put in standby mode
        sx127x_setMode(SX127X_MODE_STDBY);

    } else if (sx127x_getMode() != SX127X_MODE_RX_SINGLE) {
        // not currently in RX mode
        // reset FIFO address
        sx127x_write(REG_FIFO_ADDR_PTR, 0);

        // put in single RX mode
        sx127x_setMode(SX127X_MODE_RX_SINGLE);
    }

    return packetLength;
}

int sx127x_available() {
    return sx127x_read(REG_RX_NB_BYTES) - LORA_PACKET_INDEX;
}

void fun_sx127x_readPacket(char* buff) {
    while (sx127x_available()) {
        LORA_PACKET_INDEX++;
        *buff++ = sx127x_read(REG_FIFO);
    }
}

#define REG_PKT_RSSI_VALUE       0x1a
#define RF_MID_BAND_THRESHOLD    525E6
#define RSSI_OFFSET_HF_PORT      157
#define RSSI_OFFSET_LF_PORT      164

int fun_sx127x_getRssi(uint32_t frequency) {
    int offset = (frequency < 525E6) ? RSSI_OFFSET_LF_PORT : RSSI_OFFSET_HF_PORT;
    return sx127x_read(REG_PKT_RSSI_VALUE) - offset;
}