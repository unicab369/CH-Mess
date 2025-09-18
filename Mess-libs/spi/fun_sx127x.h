// Stolen from https://github.com/sandeepmistry/arduino-LoRa

#include "ch32fun.h"
#include <stdint.h>

//! ####################################
//! SPI FUNCTIONS
//! ####################################

uint8_t LORA_CS_PIN = -1;

uint8_t sx72xx_transfer(uint8_t reg, uint8_t value) {
    uint8_t resp;

    if (LORA_CS_PIN != -1) funDigitalWrite(LORA_CS_PIN, 0);
    resp = SPI_transfer_8(reg);
    resp = SPI_transfer_8(value);
    if (LORA_CS_PIN != -1) funDigitalWrite(LORA_CS_PIN, 1);
    return resp;
}

uint8_t sx72xx_read(uint8_t reg) {
    return sx72xx_transfer(reg & 0x7f, 0x00);
}

uint8_t sx72xx_write(uint8_t reg, uint8_t value) {
    return sx72xx_transfer(reg | 0x80, value);
}


//! ####################################
//! SETUP FUNCTIONS
//! ####################################

#define REG_FRF_MSB                 0x06
#define REG_FRF_MID                 0x07
#define REG_FRF_LSB                 0x08

#define REG_PA_CONFIG               0x09
#define PA_BOOST                    0x80
#define REG_PA_DAC                  0x4d
#define REG_OCP                     0x0b

void fun_sx72xx_setFrequency(uint32_t frequency) {
    uint64_t frf = ((uint64_t)frequency << 19) / 32000000;

    sx72xx_write(REG_FRF_MSB, (uint8_t)(frf >> 16));
    sx72xx_write(REG_FRF_MID, (uint8_t)(frf >> 8));
    sx72xx_write(REG_FRF_LSB, (uint8_t)(frf >> 0));

    // uint8_t fre0 = sx72xx_read(REG_FRF_MSB);        // expect 0xE4 for 915Mhz
    // uint8_t fre1 = sx72xx_read(REG_FRF_MID);        // expect 0xE4 for 915Mhz
    // uint8_t fre2 = sx72xx_read(REG_FRF_LSB);        // expect 0x00 for 915Mhz
    // printf("Freq Read: 0x%02X 0x%02X 0x%02X\r\n", fre0, fre0, fre2);
}

void sx772xx_setOCP(uint8_t mA) {
    uint8_t ocpTrim = 27;

    if (mA <= 120) {
        ocpTrim = (mA - 45) / 5;
    } else if (mA <=240) {
        ocpTrim = (mA + 30) / 10;
    }

    sx72xx_write(REG_OCP, 0x20 | (0x1F & ocpTrim));
}

void fun_sx72xx_setTxPower(uint8_t level) {
    if (level > 17) {
        if (level > 20) {
            level = 20;
        }

        // subtract 3 from level, so 18 - 20 maps to 15 - 17
        level -= 3;

        // High Power +20 dBm Operation (Semtech SX1276/77/78/79 5.4.3.)
        sx72xx_write(REG_PA_DAC, 0x87);
        sx772xx_setOCP(140);
    } else {
        if (level < 2) {
            level = 2;
        }

        sx72xx_write(REG_PA_DAC, 0x84);
        sx772xx_setOCP(100);
    }

    sx72xx_write(REG_PA_CONFIG, PA_BOOST | (level - 2));
}

#define REG_OP_MODE                 0x01
#define MODE_LONG_RANGE_MODE        0x80
#define MODE_STDBY                  0x01
#define MODE_TX                     0x03

#define REG_FIFO_TX_BASE_ADDR       0x0e
#define REG_FIFO_RX_BASE_ADDR       0x0f
#define REG_FIFO_RX_CURRENT_ADDR    0x10
#define REG_MODEM_CONFIG_3          0x26
#define REG_LNA                     0x0c

uint8_t LORA_OK = 0;

void sx72xx_idle() {
    sx72xx_write(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);
}

void fun_sx72xx_init(uint32_t frequency, uint8_t rst_pin, uint8_t cs_pin) {
    if (rst_pin != -1) {
        LORA_CS_PIN = cs_pin;
        funPinMode(cs_pin, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP);
        funDigitalWrite(cs_pin, 1);
    }

    uint8_t version = sx72xx_read(0x42);
    printf("LoRa version: 0x%02x\n", version);         // expect 0x12
    LORA_OK = version == 0x12;

    //# Set Mode
    uint8_t sleep = sx72xx_write(0x01, 0x80 | 0x00);

    //# Set frequency
    fun_sx72xx_setFrequency(frequency);

    //# Set base address
    sx72xx_write(REG_FIFO_TX_BASE_ADDR, 0);
    sx72xx_write(REG_FIFO_RX_BASE_ADDR, 0);

    //# Set LNA boost
    sx72xx_write(REG_LNA, sx72xx_read(REG_LNA) | 0x03);
    // uint8_t read_LNA2 = sx72xx_read(REG_LNA);
    // printf("LNA2: 0x%02X\n", read_LNA2);            // expect 0x23

    //# Set AGC
    sx72xx_write(REG_MODEM_CONFIG_3, 0x04);
    // uint8_t read_modem = sx72xx_read(REG_MODEM_CONFIG_3);
    // printf("Modem: 0x%02X\n", read_modem);          // expect 0x04

    sx72xx_idle();
}


//! ####################################
//! TRANSMITION FUNCTIONS
//! ####################################

#define REG_FIFO_ADDR_PTR           0x0d
#define REG_PAYLOAD_LENGTH          0x22
#define REG_IRQ_FLAGS               0x12
#define IRQ_TX_DONE_MASK            0x08
#define REG_FIFO                    0x00

#define REG_MODEM_CONFIG_1          0x1d
#define REG_MODEM_CONFIG_2          0x1e

// 0xFE = explicit header mode, 0x01 = implicit header mode
void sx72xx_headerMode(uint8_t mode) {
    sx72xx_write(REG_MODEM_CONFIG_1, sx72xx_read(REG_MODEM_CONFIG_1) & mode);
}

void fun_sx72xx_send(uint8_t *data, uint8_t size) {
    if (!LORA_OK) {
        printf("Err: LoRa not initialized\n");
        return;
    }

    // explicit header mode
    sx72xx_headerMode(0xFE);

    // reset FIFO address and paload length
    sx72xx_write(REG_FIFO_ADDR_PTR, 0);
    sx72xx_write(REG_PAYLOAD_LENGTH, 0);

    //# write data
    for (int i = 0; i < size; i++) {
        sx72xx_write(REG_FIFO, data[i]);
    }

    // update len
    sx72xx_write(REG_PAYLOAD_LENGTH, size);

    //# Send packet
    sx72xx_write(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_TX);
    Delay_Ms(1);

    // read = sx72xx_read(REG_IRQ_FLAGS);
    // printf("IRQ: 0x%02X\n", read);
    
    //# clear IRQ's
    sx72xx_write(REG_IRQ_FLAGS, IRQ_TX_DONE_MASK);
}


//! ####################################
//! RECEIVING FUNCTIONS
//! ####################################

#define IRQ_RX_DONE_MASK            0x40
#define IRQ_PAYLOAD_CRC_ERROR_MASK  0x20
#define REG_RX_NB_BYTES             0x13
#define MODE_RX_SINGLE              0x06

int LORA_PACKET_INDEX;

int fun_sx72xx_parsePacket() {
    if (!LORA_OK) return 0;
    int packetLength = 0;

    // explicit header mode
    sx72xx_headerMode(0xFE);

    // clear IRQ's
    int irqFlags = sx72xx_read(REG_IRQ_FLAGS);
    sx72xx_write(REG_IRQ_FLAGS, irqFlags);

    if ((irqFlags & IRQ_RX_DONE_MASK) && (irqFlags & IRQ_PAYLOAD_CRC_ERROR_MASK) == 0) {
        // received a packet
        LORA_PACKET_INDEX = 0;

        // read packet length
        // if (_implicitHeaderMode) {
        //     packetLength = sx72xx_read(REG_PAYLOAD_LENGTH);
        // } else {
            packetLength = sx72xx_read(REG_RX_NB_BYTES);
        // }

        // set FIFO address to current RX address
        sx72xx_write(REG_FIFO_ADDR_PTR, sx72xx_read(REG_FIFO_RX_CURRENT_ADDR));

        // put in standby mode
        sx72xx_idle();

    } else if (sx72xx_read(REG_OP_MODE) != (MODE_LONG_RANGE_MODE | MODE_RX_SINGLE)) {
        // not currently in RX mode

        // reset FIFO address
        sx72xx_write(REG_FIFO_ADDR_PTR, 0);

        // put in single RX mode
        sx72xx_write(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_RX_SINGLE);
    }

    return packetLength;
}

int sx72xx_available() {
    return sx72xx_read(REG_RX_NB_BYTES) - LORA_PACKET_INDEX;
}

void fun_sx72xx_readPacket(char* buff) {
    while (sx72xx_available()) {
        LORA_PACKET_INDEX++;
        *buff++ = sx72xx_read(REG_FIFO);
    }
}

#define REG_PKT_RSSI_VALUE       0x1a
#define RF_MID_BAND_THRESHOLD    525E6
#define RSSI_OFFSET_HF_PORT      157
#define RSSI_OFFSET_LF_PORT      164

int fun_sx72xx_getRssi(uint32_t frequency) {
    int offset = (frequency < 525E6) ? RSSI_OFFSET_LF_PORT : RSSI_OFFSET_HF_PORT;
    return sx72xx_read(REG_PKT_RSSI_VALUE) - offset;
}