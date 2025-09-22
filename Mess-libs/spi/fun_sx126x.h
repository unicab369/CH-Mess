
#include "ch32fun.h"
#include <stdint.h>

//! ####################################
//! SPI FUNCTIONS
//! ####################################

u8 LORA_CS_PIN2 = -1;

void SX126X_CS_HI() {
    if (LORA_CS_PIN2 != -1) funDigitalWrite(LORA_CS_PIN2, 1);
}

//# Write/Read Commands

void sx126x_write_cmd(u8 opCode, u8* data, u8 len) {
    if (LORA_CS_PIN2 != -1) funDigitalWrite(LORA_CS_PIN2, 0);
    SPI_transfer_8(opCode);
    for (int i=0; i<len; i++) SPI_transfer_8(data[i]);
    if (LORA_CS_PIN2 != -1) funDigitalWrite(LORA_CS_PIN2, 1);
}

void sx126x_read_cmd(u8 opCode, u8* data, u8 len) {
    if (LORA_CS_PIN2 != -1) funDigitalWrite(LORA_CS_PIN2, 0);
    SPI_transfer_8(opCode);
    for (int i=0; i<len; i++) data[i] = SPI_transfer_8(data[i]);
    if (LORA_CS_PIN2 != -1) funDigitalWrite(LORA_CS_PIN2, 1);
}

//# Write/Read Registers

void sx126x_write_regs(u16 addr, u8 *data, u8 len) {
    if (LORA_CS_PIN2 != -1) funDigitalWrite(LORA_CS_PIN2, 0);
    SPI_transfer_8(0x0D);                       //# 0x0D Write Register
    SPI_transfer_8((u8)((addr >> 8) & 0xFF));   // trasfer MSB
    SPI_transfer_8((u8)(addr & 0xFF));          // transfer LSB

    for (int i=0; i<len; i++) SPI_transfer_8(data[i]);
    if (LORA_CS_PIN2 != -1) funDigitalWrite(LORA_CS_PIN2, 1);
}

void sx126x_read_regs(u16 addr, u8 *data, u8 len) {
    if (LORA_CS_PIN2 != -1) funDigitalWrite(LORA_CS_PIN2, 0);
    SPI_transfer_8(0x1D);                       //# 0x1D Read Register
    SPI_transfer_8((u8)((addr >> 8) & 0xFF));   // trasfer MSB
    SPI_transfer_8((u8)(addr & 0xFF));          // transfer LSB
    SPI_transfer_8(0x00);                       // Dummy Byte

    // read data
    for (int i=0; i<len; i++) {
        data[i] = SPI_transfer_8(data[i]);
    }

    if (LORA_CS_PIN2 != -1) funDigitalWrite(LORA_CS_PIN2, 1);
}

//# Write/Read Buffer

void sx126x_write_buffer(u8 offset, u8* data, u8 len) {
    if (LORA_CS_PIN2 != -1) funDigitalWrite(LORA_CS_PIN2, 0);
    SPI_transfer_8(0x0E);                   //# 0x0E Write Buffer
    SPI_transfer_8(offset);

    for (int i=0; i<len; i++) SPI_transfer_8(data[i]);
    if (LORA_CS_PIN2 != -1) funDigitalWrite(LORA_CS_PIN2, 1);
}

void sx126x_read_buffer(u8 offset, u8 *data, u8 len) {
    if (LORA_CS_PIN2 != -1) funDigitalWrite(LORA_CS_PIN2, 0);
    SPI_transfer_8(0x1E);                   //# 0x1E Read Buffer
    SPI_transfer_8(offset);
    SPI_transfer_8(0x00);                   // Dummy Byte

    // read data
    for (int i=0; i<len; i++) {
        data[i] = SPI_transfer_8(data[i]);
    }

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
    u8 gain[1] = { 0x94 };          // default
    if (boost) gain[0] = 0x96;
    sx126x_write_regs(0x08AC, gain, 1);
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
    sx126x_write_cmd(0x8C, buff, 6);
}

void fun_sx126x_setSyncWord(u16 syncWord) {
    u8 buf[2];
    buf[0] = (u8)((syncWord >> 8) & 0xFF),
    buf[1] = (u8)(syncWord & 0xFF),

    //# 0x0740 set sync word
    sx126x_write_regs(0x0740, buf, 2);
}

u8 fun_sx126x_getMode() {
    u8 mode;
    sx126x_read_cmd(0xC0, &mode, 1);
    return mode;
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

    //# 0x98 set calibration image
    sx126x_write_cmd(0x98, buff, 2);
    Delay_Ms(10);

    uint32_t rfFreq = ((uint64_t) frequency << 25) / 32000000;
    buff[0] = (u8)((rfFreq >> 24) & 0xFF);
    buff[1] = (u8)((rfFreq >> 16) & 0xFF);
    buff[2] = (u8)((rfFreq >> 8) & 0xFF);
    buff[3] = (u8)(rfFreq & 0xFF);

    //# 0x84 set frequency
    sx126x_write_cmd(0x86, buff, 4);
}

void fun_sx126x_setModulation(u8 sf, u8 bw, u8 cr, u8 lowDataRateOpt) {
    // valid spreading factor is between 5 and 12
    if (sf > 12) sf = 12;
    else if (sf < 5) sf = 5;
    
    u8 buf[4] = {
        sf, bw, cr, lowDataRateOpt & 0x01
    };

    //# 0x8B set modulation
    sx126x_write_cmd(0x8B, buf, 4);
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


u8 LORA_OK2 = 0;

#define DIO_PIN         PD4

void fun_sx126x_init(uint32_t frequency, u8 cs_pin) {
    funPinMode(DIO_PIN, GPIO_CFGLR_IN_PUPD);
    u8 buf[9];

    //! configure CS Pin
    if (cs_pin != -1) {
        LORA_CS_PIN2 = cs_pin;
        funPinMode(cs_pin, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP);
        funDigitalWrite(cs_pin, 1);
    }

    //# Reset Spi Devices
    funDigitalWrite(PC3, 0);
    Delay_Ms(100);
    funDigitalWrite(PC3, 1);
    Delay_Ms(100);

    //! default sync_word check
    u8 default_syncWord[2];
    sx126x_read_regs(0x0740, default_syncWord, 2);
    printf("Default SyncWord: 0x%02X 0x%02X\n", default_syncWord[0], default_syncWord[1]);
    //! Expect 0x2414
    LORA_OK2 = default_syncWord[0] == 0x14;
    printf("LoRa OK2: %d\n", LORA_OK2);

    // # 0x8A set modem
    u8 value[1] = {0x01};    // 0x00 = GFSK, 0x01 = LoRa
    sx126x_write_cmd(0x8A, value, 1);
    Delay_Ms(1);

    // # 0x11 Get modem
    sx126x_read_cmd(0x11, buf, 2);
    printf("packetType: 0x%02X 0x%02X\n", buf[0], buf[1]);
    //! Expect 0x01
    LORA_OK2 = buf[1] == 0x01;
    printf("LoRa OK2: %d\n", LORA_OK2);

    // # 0x80 set standby (Not needed for minimal init? it works without this command)
    buf[0] = 0x00;      // 0x00 = RC (low power), 0x01 = XOSC (performant)
    sx126x_write_buffer(0x80, buf, 1);
    Delay_Ms(1);

    //# set frequency
    fun_sx126x_setFreq(frequency);
    Delay_Ms(10);

    // # set modulation
    u8 cr = 0x01;     // 0x01 = 4/5, 0x02 = 4/6, 0x03 = 4/7, 0x04 = 4/8
    fun_sx126x_setModulation(7, SX126X_BW_125000, cr, 0);
    Delay_Ms(10);

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
    sx126x_write_cmd(0x95, buff2, 4);
    Delay_Ms(10);

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
    sx126x_write_cmd(0x8E, buf, 2);
    Delay_Ms(10);

    //# configure IRQ interrups
    u16 iqrDio1_mask = SX126X_IRQ_RX_DONE;
    fun_sx126x_setDioIrqParams(
        0x0002,     // IRQ mask for receiving
        0xFFFF,     // DIO1 mask turn ON
        0x0000,     // DIO2 mask
        0x0000      // DIO3 mask
    );


    // //# 0x8F set buffer base address
    // u8 buff[2];
    // buff[0] = 0x00;
    // buff[1] = 0x00;
    // sx126x_write_cmd(0x8F, buff, 2);
    // Delay_Ms(10);

    // //# 0xC0 get Status
    // if (LORA_CS_PIN2 != -1) funDigitalWrite(LORA_CS_PIN2, 0);
    // SPI_transfer_8(0xC0);
    // b0 = SPI_transfer_8(0xFF);
    // printf("status: 0x%02X, chipMode: 0x%02X, cmdStatus: 0x%02X\n", 
    //         b0, (b0 >> 4) & 0x7, (b0 >> 1) & 0x7);
    // printf("status Mode: 0x%02X\n", b0 & 0x70);
    // if (LORA_CS_PIN2 != -1) funDigitalWrite(LORA_CS_PIN2, 1);
    // Delay_Ms(100);

    // //# 0x12 get IRQ status
    // sx126x_read_cmd(0x12, buf, 2);
    // printf("\nIRQ status: 0x%02X 0x%02X\n", buf[0], buf[1]);
    // for (int i = 7; i >= 0; i--) printf("%d", (buf[0] >> i) & 1);
    // for (int i = 7; i >= 0; i--) printf("%d", (buf[1] >> i) & 1);
    // printf("\n");
}

static u8 payloadTxRx;
static u8 buffIndex;
static u16 irqStatus;

u16 fun_sx126x_getIRQStatus() {
    u8 buff3[3];
    sx126x_read_cmd(0x12, buff3, 3);        //# 0x12 get IRQ status
    return (buff3[1] << 8) | buff3[2];
}


void fun_sx126x_setDioIrqParams(
    u16 irqMask, u16 dio1Mask, u16 dio2Mask, u16 dio3Mask
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

    sx126x_write_cmd(0x08, buff, 8);
}

#define SX126X_PREAMBLE_LEN     12
#define SX126X_HEADER_IMPLICIT  0x00    // 0x00: implicit, 0x01: explicit

//! ####################################
//! RECEIVE FUNCTION
//! ####################################
u8 fun_sx126x_getReceivedMessage(
    u8 *message, u8 len, u8 memoryIndex,
    s16 *rssi, s16 *snr
) {
    sx126x_read_buffer(memoryIndex, message, len);
    printf("\nmessage: %s\n", message);
    u8 buf[4];

    //# 0x14: get packet status
    sx126x_read_cmd(0x14, buf, 4);
    u8 status = buf[0];
    *rssi = - buf[1] / 2;
    *snr = buf[2] / 4;
}

u8 fun_sx126x_parsePacket(u32 timeoutMs, u8 *memoryIndex) {
    // return;
    u8 buf[4];

    //# packet configuration 
    // (preambleLen, headerType, payloadLen, crcOn, invertIQ)
    fun_sx126x_setPacketParams(
        SX126X_PREAMBLE_LEN, SX126X_HEADER_IMPLICIT,
        255, 0, 0
    );
    Delay_Ms(1);

    //# 0x82: start RX with timeout 0xFFFFFF to listen continously
    u8 timeoutBuff[3] = {
        (u8)((timeoutMs >> 16) & 0xFF),
        (u8)((timeoutMs >> 8) & 0xFF),
        (u8)((timeoutMs >> 0) & 0xFF) 
    };
    sx126x_write_cmd(0x82, timeoutBuff, 3);
    Delay_Ms(1);

    if (funDigitalRead(DIO_PIN) == 0) return 0;

    //# 0x13: get buffer status
    sx126x_read_cmd(0x13, buf, 3);
    u8 payloadLen = buf[1];
    *memoryIndex = buf[2];

    //# 0x02: clear all IRQ status
    u16 clearCode = 0xFFFF;
    buf[0] = (u8)((clearCode >> 8) & 0xFF);
    buf[1] = (u8)(clearCode & 0xFF);
    sx126x_write_cmd(0x02, buf, 2);

    return payloadLen;
}


u32 send_time = 0;

//! ####################################
//! SEND FUNCTION
//! ####################################

void fun_sx126x_send(char* message, u8 len, u32 timeoutMs) {
    return;

    if (millis() - send_time < 2000) return;
    send_time = millis();

    //# packet configuration
    // (preambleLen, headerType, payloadLen, crcOn, invertIQ)
    fun_sx126x_setPacketParams(
        SX126X_PREAMBLE_LEN, SX126X_HEADER_IMPLICIT,
        len, 1, 0
    );
    Delay_Ms(10);

    // //# 0x8F set buffer base address
    // u8 buff[2];
    // buff[0] = 0x00;
    // buff[1] = 0x00;
    // sx126x_write_cmd(0x8F, buff, 2);
    // Delay_Ms(10);

    //# set payload
    sx126x_write_buffer(0x00, (u8*)message, len);

    //# 0x83 start Tx with timeout
    u8 timeoutBuff[3] = {
        (u8)((timeoutMs >> 16) & 0xFF),
        (u8)((timeoutMs >> 8) & 0xFF),
        (u8)(timeoutMs & 0xFF)
    };
    sx126x_write_cmd(0x83, timeoutBuff, 3);


    // //# clear IRQ status
    // // clear status command = 0x43FF
    // u16 status = 0x43FF;
    // // u16 status = fun_sx126x_getIRQStatus();
    // buff[0] = (u8)((status >> 8) & 0xFF);
    // buff[1] = (u8)(status & 0xFF);
    // sx126x_write_cmd(0x02, buff, 2);
}