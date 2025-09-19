
#include "ch32fun.h"
#include <stdint.h>

//! ####################################
//! SPI FUNCTIONS
//! ####################################

u8 LORA_CS_PIN2 = -1;

void sx126x_transfer_read(
    u8 opCode, u8* out, u8 outLen,
    u8* in, u8 inLen
) {
    if (LORA_CS_PIN2 != -1) funDigitalWrite(LORA_CS_PIN2, 0);
    SPI_transfer_8(opCode);

    for (int i=0; i<outLen; i++) out[i] = SPI_transfer_8(out[i]);
    for (int i=0; i<inLen; i++) in[i] = SPI_transfer_8(in[i]);

    if (LORA_CS_PIN2 != -1) funDigitalWrite(LORA_CS_PIN2, 1);
}

void sx126x_readBuffer(u8 offset, u8 *data, u8 len) {
    u8 buff[2] = { offset, 0x00 };
    sx126x_transfer_read(0x1E, buff, 2, data, len);
}


//# Write/Read Commands

void Asx126x_write_cmd8(u8 opCode, u8 byte) {
    if (LORA_CS_PIN2 != -1) funDigitalWrite(LORA_CS_PIN2, 0);
    SPI_transfer_8(opCode);
    SPI_transfer_8(byte);
    if (LORA_CS_PIN2 != -1) funDigitalWrite(LORA_CS_PIN2, 1);
}

void Asx126x_write_cmd(u8 opCode, u8* data, u8 len) {
    if (LORA_CS_PIN2 != -1) funDigitalWrite(LORA_CS_PIN2, 0);
    SPI_transfer_8(opCode);
    for (int i=0; i<len; i++) SPI_transfer_8(data[i]);
    if (LORA_CS_PIN2 != -1) funDigitalWrite(LORA_CS_PIN2, 1);
}

void Asx126x_read_cmd(u8 opCode, u8* data, u8 len) {
    if (LORA_CS_PIN2 != -1) funDigitalWrite(LORA_CS_PIN2, 0);
    SPI_transfer_8(opCode);
    for (int i=0; i<len; i++) data[i] = SPI_transfer_8(data[i]);
    if (LORA_CS_PIN2 != -1) funDigitalWrite(LORA_CS_PIN2, 1);
}

//# Write/Read Registers

void Asx126x_write_regs(u16 addr, u8 *data, u8 len) {
    if (LORA_CS_PIN2 != -1) funDigitalWrite(LORA_CS_PIN2, 0);
    SPI_transfer_8(0x0D);                   //# 0x0D Write Register
    SPI_transfer_16(addr);
    for (int i=0; i<len; i++) SPI_transfer_8(data[i]);
    if (LORA_CS_PIN2 != -1) funDigitalWrite(LORA_CS_PIN2, 1);
}

u8 Asx126x_read_reg(u16 addr) {
    if (LORA_CS_PIN2 != -1) funDigitalWrite(LORA_CS_PIN2, 0);
    SPI_transfer_8(0x1D);                   //# 0x1D Read Register
    SPI_transfer_16(addr);
    SPI_transfer_8(0x00);                   // Dummy Byte
    
    u8 read = SPI_transfer_8(0x00);         // Read Response
    if (LORA_CS_PIN2 != -1) funDigitalWrite(LORA_CS_PIN2, 1);
    return read;
}

void Asx126x_read_regs(u16 addr, u8 *data, u8 len) {
    if (LORA_CS_PIN2 != -1) funDigitalWrite(LORA_CS_PIN2, 0);
    SPI_transfer_8(0x1D);                   //# 0x1D Read Register
    SPI_transfer_16(addr);
    for (int i=0; i<len; i++) data[i] = SPI_transfer_8(data[i]);
    if (LORA_CS_PIN2 != -1) funDigitalWrite(LORA_CS_PIN2, 1);
}

//# Write/Read Buffer

void Asx126x_write_buffer(u8 offset, u8* data, u8 len) {
    if (LORA_CS_PIN2 != -1) funDigitalWrite(LORA_CS_PIN2, 0);
    SPI_transfer_8(0x0E);                   //# 0x0E Write Buffer
    SPI_transfer_8(offset);

    for (int i=0; i<len; i++) SPI_transfer_8(data[i]);
    if (LORA_CS_PIN2 != -1) funDigitalWrite(LORA_CS_PIN2, 1);
}


//! ####################################
//! OTHER SETUP FUNCTIONS
//! ####################################

// RxGain
#define SX126X_RX_GAIN_POWER_SAVING     0x00        // gain used in Rx mode: power saving gain (default)
#define SX126X_RX_GAIN_BOOSTED          0x01        //                       boosted gain
#define SX126X_POWER_SAVING_GAIN        0x94        // power saving gain register value
#define SX126X_BOOSTED_GAIN             0x96        // boosted gain register value

void fun_sx126x_setRxGain(u8 boost) {
    //# 0x08AC set Rx gain
    // 0x94 = 0dBm, 0x96 = 14dBm
    u8 gain[1] = { 0x94 };
    if (boost) gain[0] = 0x96;
    Asx126x_write_regs(0x08AC, gain, 1);
}

void fun_sx126x_setPacketParams(
    u16 preambleLen, u8 headerType, u8 payloadLen, u8 crcOn, u8 invertIQ
) {
    // header type 0 = explicit, 1 = implicit
    headerType = (headerType == 0) ? 0 : 1;
    u8 buff[6] = {
        preambleLen >> 8, preambleLen,
        headerType,
        payloadLen,
        crcOn,
        invertIQ,
    };

    //# 0x8C set LoRa packet
    Asx126x_write_cmd(0x8C, buff, 6);
}

void fun_sx126x_setSyncWord(u16 syncWord) {
    u8 buf[2];
    buf[0] = syncWord >> 8;
    buf[1] = syncWord & 0xFF;
    if (syncWord <= 0xFF) {
        buf[0] = (syncWord & 0xF0) | 0x04;
        buf[1] = (syncWord << 4) | 0x04;
    }

    //# 0x0740 set sync word
    Asx126x_write_regs(0x0740, buf, 2);
}

u8 fun_sx126x_getMode() {
    u8 mode;
    Asx126x_read_cmd(0xC0, &mode, 1);
    printf("Status0: 0x%02X\n", mode);
    return mode & 0x70;
}

//! ####################################
//! SET FREQUENCY AND MODULATION
//! ####################################

// SetModulationParams for LoRa packet type
#define SX126X_BW_7800          0x00        // LoRa bandwidth: 7.8 kHz
#define SX126X_BW_10400         0x08        //                 10.4 kHz
#define SX126X_BW_15600         0x01        //                 15.6 kHz
#define SX126X_BW_20800         0x09        //                 20.8 kHz
#define SX126X_BW_31250         0x02        //                 31.25 kHz
#define SX126X_BW_41700         0x0A        //                 41.7 kHz
#define SX126X_BW_62500         0x03        //                 62.5 kHz
#define SX126X_BW_125000        0x04        //                 125 kHz
#define SX126X_BW_250000        0x05        //                 250 kHz
#define SX126X_BW_500000        0x06        //                 500 kHz
#define SX126X_CR_4_4           0x00        // LoRa coding rate: 4/4 (no coding rate)
#define SX126X_CR_4_5           0x01        //                   4/5
#define SX126X_CR_4_6           0x02        //                   4/6
#define SX126X_CR_4_7           0x03        //                   4/7
#define SX126X_CR_4_8           0x04        //                   4/8


void fun_sx126x_setFreq(uint32_t frequency) {
    if (frequency < 150000000 || frequency > 960000000) { return; }

    u8 buff[4];
    if (frequency < 446000000) {        // 430 - 440 Mhz
        buff[0] = 0x6B;
        buff[1] = 0x6F;
    }
    else if (frequency < 734000000) {   // 470 - 510 Mhz
        buff[0] = 0x75;
        buff[1] = 0x81;
    }
    else if (frequency < 828000000) {   // 779 - 787 Mhz
        buff[0] = 0xC1;
        buff[1] = 0xC5;
    }
    else if (frequency < 877000000) {   // 863 - 870 Mhz
        buff[0] = 0xD7;
        buff[1] = 0xDB;
    }
    else if (frequency < 1100000000) {  // 902 - 928 Mhz
        buff[0] = 0xE1;
        buff[1] = 0xE9;
    }

    // //# 0x98 set calibration image
    // Asx126x_write_cmd(0x98, buff, 2);

    uint32_t rfFreq = ((uint64_t) frequency << 25) / 32000000;
    buff[0] = (u8)((rfFreq >> 24) & 0xFF);
    buff[1] = (u8)((rfFreq >> 16) & 0xFF);
    buff[2] = (u8)((rfFreq >> 8) & 0xFF);
    buff[3] = (u8)(rfFreq & 0xFF);

    //# 0x84 set frequency
    Asx126x_write_cmd(0x86, buff, 4);
}

void fun_sx126x_setModulation(u8 sf, u8 bw, u8 cr, u8 lowDataRateOpt) {
    // valid spreading factor is between 5 and 12
    if (sf > 12) sf = 12;
    else if (sf < 5) sf = 5;
    
    u8 buf[4] = {
        sf, bw, cr, lowDataRateOpt & 0x01
    };

    //# 0x8B set modulation
    Asx126x_write_cmd(0x8B, buf, 4);
}


//! ####################################
//! INIT FUNCTIONS
//! ####################################

#define SX126X_MODE_STDBY_RC        0x20        // current chip mode: STDBY_RC
#define SX126X_MODE_STDBY_XOSC      0x30        //                    STDBY_XOSC
#define SX126X_STATUS_MODE_FS       0x40        //                    FS
#define SX126X_STATUS_MODE_RX       0x50        //                    RX
#define SX126X_STATUS_MODE_TX       0x60        //                    TX

// SetTxParams
#define SX126X_PA_RAMP_10U          0x00        // ramp time: 10 us
#define SX126X_PA_RAMP_20U          0x01        //            20 us
#define SX126X_PA_RAMP_40U          0x02        //            40 us
#define SX126X_PA_RAMP_80U          0x03        //            80 us
#define SX126X_PA_RAMP_200U         0x04        //            200 us
#define SX126X_PA_RAMP_800U         0x05        //            800 us
#define SX126X_PA_RAMP_1700U        0x06        //            1700 us
#define SX126X_PA_RAMP_3400U        0x07        //            3400 us

u8 LORA_OK2 = 0;

void fun_sx126x_init(uint32_t frequency) {
    //! sanity check
    u8 check = Asx126x_read_reg(0x0740);
    printf("check: 0x%02X\n", check);   // Expect 0x14

    //! read version
    u8 ver[16];
    Asx126x_read_regs(0x0320, ver, 16);
    printf("version: %s\n", ver);

    //# 0x80 set standby
    u8 mode = 0x00;     // 0x00 = RC (low power), 0x01 = XOSC (performant)
    Asx126x_write_cmd8(0x80, mode);

    //# 0x9D set DIO2 as RF Switch Control
    Asx126x_write_cmd8(0x9D, 0x01);

    //# check status mode. Expect 0x20
    if (fun_sx126x_getMode() == SX126X_MODE_STDBY_RC) LORA_OK2 = 1;
    printf("Status1: 0x%02X\n", fun_sx126x_getMode());

    //# 0x8A set packet type
    mode = 0x01;        // 0x00 = GFSK, 0x01 = LoRa
    Asx126x_write_cmd8(0x8A, 0x01);

    //# 0x11 get packet type
    u8 buf[2];
    Asx126x_read_cmd(0x11, buf, 2);
    u8 packet_type = buf[1];
    printf("Packet type: %d\n", packet_type);

    //# set frequency
    fun_sx126x_setFreq(frequency);

    // //# SX1262: 0x08D8 get TX clamp config
    // u8 value = Asx126x_read_reg(0x08D8);
    // printf("TX clamp config: 0x%02X\n", value);
    // value |= 0x1E;
    // Asx126x_write_regs(0x08D8, &value, 1);

    //# 0x95 set PA and TX power setting
    // for SX1261
    // pa_power == 5    : { 0x06, 0x00, 0x01, 0x01 }
    // else             : { 0x04, 0x00, 0x00, 0x01 }
    // config for SX1262
    u8 buff2[4] = {
        0x04,       // PA Duty Cycle
        0x07,       // HP Max
        0x00,       // Device Select: 0x00 = SX1262, 0x01 = SX1261
        0x01        // PowerLUT
    };
    Asx126x_write_cmd(0x95, buff2, 4);

    //# 0x8E set TX power
    // RampTime 0x00 = 10 us, 0x01 = 20 us, 0x02 = 40 us, 0x03 = 80 us,
    // 0x04 = 200 us, 0x05 = 800 us, 0x06 = 1700 us, 0x07 = 3400 us
    u8 rampTime = 0x04;                 // 200 us
    // Low Power Mode 0xEF(-17dBm) to 0x0E(+14dBm)
    // High Power Mode 0xF7(-9dBm) to 0x16(+22dBm) 
    s8 power = 0x16;

    power = (power < -3) ? -3 : power;
    power = (power > 22) ? 22 : power;
    buf[0] = power;
    buf[1] = rampTime;
    Asx126x_write_cmd(0x8E, buf, 2);

    //# set modulation
    fun_sx126x_setModulation(7, SX126X_BW_125000, SX126X_CR_4_5, 0);

    printf("\n-- LORA RECEIVER --\n");
}

static u8 payloadTxRx;
static u8 buffIndex;
static u16 irqStatus;

u8 fun_sx126x_readByte() {
    u8 data;
    sx126x_readBuffer(buffIndex, &data, 1);
    buffIndex++;
    if (payloadTxRx > 0) payloadTxRx--;
    return data;
}

u16 fun_sx126x_getIRQStatus() {
    u8 buff3[3];
    Asx126x_read_cmd(0x12, buff3, 3);        //# 0x12 get IRQ status
    return (buff3[1] << 8) | buff3[2];
}


// SetDioIrqParams
#define SX126X_IRQ_NONE                         0x0000      // no interrupts
#define SX126X_IRQ_TX_DONE                      0x0001      // packet transmission completed
#define SX126X_IRQ_RX_DONE                      0x0002      // packet received
#define SX126X_IRQ_PREAMBLE_DETECTED            0x0004      // preamble detected
#define SX126X_IRQ_SYNC_WORD_VALID              0x0008      // valid sync word detected
#define SX126X_IRQ_HEADER_VALID                 0x0010      // valid LoRa header received
#define SX126X_IRQ_HEADER_ERR                   0x0020      // LoRa header CRC error
#define SX126X_IRQ_CRC_ERR                      0x0040      // wrong CRC received
#define SX126X_IRQ_CAD_DONE                     0x0080      // channel activity detection finished
#define SX126X_IRQ_CAD_DETECTED                 0x0100      // channel activity detected
#define SX126X_IRQ_TIMEOUT                      0x0200      // Rx or Tx timeout
#define SX126X_IRQ_ALL                          0x03FF      // all interrupts

void fun_sx126x_setDioIrqParams(
    uint16_t irqMask, uint16_t dio1Mask, uint16_t dio2Mask, uint16_t dio3Mask
) {
    u8 buff[8] = {
        (u8)((irqMask >> 8) & 0xFF),
        (u8)(irqMask & 0xFF),
        (u8)((dio1Mask >> 8) & 0xFF),
        (u8)(dio1Mask & 0xFF),
        (u8)((dio2Mask >> 8) & 0xFF),
        (u8)(dio2Mask & 0xFF),
        (u8)((dio3Mask >> 8) & 0xFF), 
        (u8)(dio3Mask & 0xFF)
    };

    Asx126x_write_cmd(0x08, buff, 8);
}


//! ####################################
//! RECEIVE FUNCTION
//! ####################################

u8 fun_sx126x_parsePacket(u32 timeoutMs) {
    if (fun_sx126x_getMode() == SX126X_STATUS_MODE_RX) return 0;

    //# clear IRQ status
    u8 buf[2];
    buf[0] = 0x03FF >> 8;
    buf[1] = 0x03FF;
    Asx126x_write_cmd(0x02, buf, 2);

    //# set DIO IRQ
    u16 iqrDio1_mask = SX126X_IRQ_RX_DONE | SX126X_IRQ_TIMEOUT | SX126X_IRQ_CRC_ERR;
    fun_sx126x_setDioIrqParams(
        iqrDio1_mask, iqrDio1_mask, SX126X_IRQ_NONE, SX126X_IRQ_NONE
    );

    //# set RX timeout
    u8 timeoutBuff[3] = {
        (u8)((timeoutMs >> 16) & 0xFF),
        (u8)((timeoutMs >> 8) & 0xFF),
        (u8)(timeoutMs & 0xFF) 
    };
    Asx126x_write_cmd(0x82, timeoutBuff, 3);

    fun_sx126x_getIRQStatus();

    //# 0x13 get buffer status
    u8 buff3[3];
    Asx126x_read_cmd(0x13, buff3, 3);
    payloadTxRx = buff3[1];
    buffIndex = buff3[2];

    const u8 msgLen = payloadTxRx - 1;
    char message[msgLen];
    u8 counter;

    u8 i = 0;
    while (payloadTxRx > 1){
        message[i++] = fun_sx126x_readByte();
    }
    counter = fun_sx126x_readByte();

    // Print received message and counter in serial
    printf("counter: %ld, message: %s\n", counter, message);

    //# 0x14 get packet status
    uint8_t buff[4];
    Asx126x_read_cmd(0x14, buff, 4);
    u16 rssi = buff[0] / -2;
    u16 snr = buff[1] / 4;
    printf("RSSI: %d dbm, SNR = %d dB\n", rssi, snr);
    return 1;
}


//! ####################################
//! SEND FUNCTION
//! ####################################

void fun_sx126x_send(char* message, u8 len, u32 timeoutMs) {
    //# packet configuration
    // (preambleLen, headerType, payloadLen, crcOn, invertIQ)
    fun_sx126x_setPacketParams(12, 0, len, 1, 0);

    //# set DIO IRQ
    u8 iqrDio1_mask = SX126X_IRQ_TX_DONE | SX126X_IRQ_TIMEOUT;
    fun_sx126x_setDioIrqParams(
        iqrDio1_mask, iqrDio1_mask, SX126X_IRQ_NONE, SX126X_IRQ_NONE
    );

    //# 0x8F set buffer base address
    u8 buff[2];
    buff[0] = 0x00;
    buff[1] = 0x00;
    Asx126x_write_cmd(0x8F, buff, 2);

    //# set payload
    Asx126x_write_buffer(0x00, (u8*)message, len);

    //# 0x83 set Tx with timeout
    u8 timeoutBuff[3] = {
        (u8)((timeoutMs >> 16) & 0xFF),
        (u8)((timeoutMs >> 8) & 0xFF),
        (u8)(timeoutMs & 0xFF)
    };
    Asx126x_write_cmd(0x83, timeoutBuff, 3);

    //# clear IRQ status
    // clear status command = 0x43FF
    u16 status = 0x43FF;
    // u16 status = fun_sx126x_getIRQStatus();
    buff[0] = (u8)((status >> 8) & 0xFF);
    buff[1] = (u8)(status & 0xFF);
    Asx126x_write_cmd(0x02, buff, 2);
}