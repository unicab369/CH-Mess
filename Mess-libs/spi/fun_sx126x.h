
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

void sx126x_transfer(
    u8 opCode, u8* data, u8 len
) {
    sx126x_transfer_read(opCode, NULL, 0, data, len);
}

void sx126x_write(u16 addr, u8* data, u8 len) {
    u8 buff[2] = {addr >> 8, addr };
    sx126x_transfer_read(0x0D, buff, 2 , data, len);
}

void sx126x_read(u16 addr, u8* data, u8 len) {
    u8 buff[3] = {addr >> 8, addr, 0x00 };
    sx126x_transfer_read(0x1D, buff, 3, data, len);    
}

void sx126x_writeBuffer(u8 offset, u8* data, u8 len) {
    u8 buff[1] = { offset };
    sx126x_transfer_read(0x0E, buff, 1, data, len);
}

void sx126x_readBuffer(u8 offset, u8 *data, u8 len) {
    u8 buff[2] = { offset, 0x00 };
    sx126x_transfer_read(0x1E, buff, 2, data, len);
}


//! ####################################
//! SET FREQUENCY
//! ####################################

// CalibrateImage
#define SX126X_CAL_IMG_430              0x6B        // ISM band: 430-440 Mhz Freq1
#define SX126X_CAL_IMG_440              0x6F        //           430-440 Mhz Freq2
#define SX126X_CAL_IMG_470              0x75        //           470-510 Mhz Freq1
#define SX126X_CAL_IMG_510              0x81        //           470-510 Mhz Freq2
#define SX126X_CAL_IMG_779              0xC1        //           779-787 Mhz Freq1
#define SX126X_CAL_IMG_787              0xC5        //           779-787 Mhz Freq2
#define SX126X_CAL_IMG_863              0xD7        //           863-870 Mhz Freq1
#define SX126X_CAL_IMG_870              0xDB        //           863-870 Mhz Freq2
#define SX126X_CAL_IMG_902              0xE1        //           902-928 Mhz Freq1
#define SX126X_CAL_IMG_928              0xE9        //           902-928 M   

void fun_sx126x_setFreq(uint32_t frequency) {
    u8 buff[2];
    if (frequency < 446000000) {        // 430 - 440 Mhz
        buff[0] = SX126X_CAL_IMG_430;
        buff[1] = SX126X_CAL_IMG_440;
    }
    else if (frequency < 734000000) {   // 470 - 510 Mhz
        buff[0] = SX126X_CAL_IMG_470;
        buff[1] = SX126X_CAL_IMG_510;
    }
    else if (frequency < 828000000) {   // 779 - 787 Mhz
        buff[0] = SX126X_CAL_IMG_779;
        buff[1] = SX126X_CAL_IMG_787;
    }
    else if (frequency < 877000000) {   // 863 - 870 Mhz
        buff[0] = SX126X_CAL_IMG_863;
        buff[1] = SX126X_CAL_IMG_870;
    }
    else if (frequency < 1100000000) {  // 902 - 928 Mhz
        buff[0] = SX126X_CAL_IMG_902;
        buff[1] = SX126X_CAL_IMG_928;
    }

    // calculate frequency for setting configuration
    uint32_t rfFreq = ((uint64_t) frequency << 25) / 32000000;
    sx126x_transfer(0x98, buff, 2);         // set calibration image

    u8 buff2[4];
    buff2[0] = (u8)(rfFreq >> 24);
    buff2[1] = (u8)(rfFreq >> 16);
    buff2[2] = (u8)(rfFreq >> 8);
    buff2[3] = (u8)(rfFreq >> 0);
    sx126x_transfer(0x86, buff2, 4);        // set frequency
}


//! ####################################
//! SET MODULATION
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
#define SX126X_LDRO_OFF         0x00        // LoRa low data rate optimization: disabled
#define SX126X_LDRO_ON          0x00        //                                  enabled

void fun_sx126x_setModulation(u8 sf, u8 bw, u8 cr) {
    // valid spreading factor is between 5 and 12
    if (sf > 12) sf = 12;
    else if (sf < 5) sf = 5;
    
    u8 buf[8] = {
        sf, (u8)bw, cr, 0x00,
        0x00, 0x00, 0x00, 0x00
    };
    sx126x_transfer(0x8B, buf, 8);
}

//! ####################################
//! OTHER SETUP FUNCTIONS
//! ####################################

// RxGain
#define SX126X_RX_GAIN_POWER_SAVING     0x00        // gain used in Rx mode: power saving gain (default)
#define SX126X_RX_GAIN_BOOSTED          0x01        //                       boosted gain
#define SX126X_POWER_SAVING_GAIN        0x94        // power saving gain register value
#define SX126X_BOOSTED_GAIN             0x96        // boosted gain register value

#define SX126X_REG_RX_GAIN              0x08AC
#define SX126X_TX_CLAMP_CONFIG          0x08D8
#define SX126X_SYNC_WORD_MSB            0x0740

void fun_sx126x_setRxGain(u8 boost) {
    u8 gain = boost ? SX126X_BOOSTED_GAIN : SX126X_POWER_SAVING_GAIN;
    sx126x_write(SX126X_REG_RX_GAIN, &gain, 1);
}

void fun_sx126x_setLoraPacket(
    u8 headerType, u16 preambleLen, u8 payloadLen, u8 crcOn
) {
    // header type 0 = explicit, 1 = implicit
    headerType = (headerType == 0) ? 0 : 1;
    u8 buff[9] = {
        preambleLen >> 8, preambleLen, headerType, payloadLen, crcOn,
        0x00, 0x00, 0x00, 0x00
    };
    sx126x_transfer(0x8C, buff, 9);
}

void fun_sx126x_setSyncWord(u16 syncWord) {
    u8 buf[2];
    buf[0] = syncWord >> 8;
    buf[1] = syncWord & 0xFF;
    if (syncWord <= 0xFF) {
        buf[0] = (syncWord & 0xF0) | 0x04;
        buf[1] = (syncWord << 4) | 0x04;
    }
    sx126x_write(SX126X_SYNC_WORD_MSB, buf, 2);
}

u8 fun_sx126x_getMode() {
    u8 mode;
    sx126x_transfer(0xC0, &mode, 1);
    printf("Status0: 0x%02X\n", mode);
    return mode & 0x70;
}


//! ####################################
//! INIT FUNCTIONS
//! ####################################

// SetStandby
#define SX126X_STANDBY_RC               0x00        // standby mode: using 13 MHz RC oscillator
#define SX126X_STANDBY_XOSC             0x01        //               using 32 MHz crystal oscillator

#define SX126X_MODE_STDBY_RC            0x20        // current chip mode: STDBY_RC
#define SX126X_MODE_STDBY_XOSC          0x30        //                    STDBY_XOSC
#define SX126X_STATUS_MODE_FS           0x40        //                    FS
#define SX126X_STATUS_MODE_RX           0x50        //                    RX
#define SX126X_STATUS_MODE_TX           0x60        //                    TX

// SetPacketType    
#define SX126X_FSK_MODEM                0x00        // GFSK packet type
#define SX126X_LORA_MODEM               0x01        // LoRa packet type

// SetTxParams
#define SX126X_PA_RAMP_10U                      0x00        // ramp time: 10 us
#define SX126X_PA_RAMP_20U                      0x01        //            20 us
#define SX126X_PA_RAMP_40U                      0x02        //            40 us
#define SX126X_PA_RAMP_80U                      0x03        //            80 us
#define SX126X_PA_RAMP_200U                     0x04        //            200 us
#define SX126X_PA_RAMP_800U                     0x05        //            800 us
#define SX126X_PA_RAMP_1700U                    0x06        //            1700 us
#define SX126X_PA_RAMP_3400U                    0x07        //            3400 us

u8 LORA_OK2 = 0;

void fun_sx126x_init(uint32_t frequency) {
    //# set standby mode
    u8 command = SX126X_STANDBY_RC;
    sx126x_transfer(0x80, &command, 1);

    //# check status mode. Expect 0x20
    if (fun_sx126x_getMode() == SX126X_MODE_STDBY_RC) LORA_OK2 = 1;
    printf("Status1: 0x%02X\n", fun_sx126x_getMode());

    //# set packet type
    command = SX126X_LORA_MODEM;
    sx126x_transfer(0x8A, &command, 1);

    //# get packet type
    u8 buf[2];
    sx126x_transfer(0x11, buf, 2);
    u8 packet_type = buf[1];
    printf("Packet type: %d\n", packet_type);

    u8 value;
    sx126x_read(SX126X_TX_CLAMP_CONFIG, &value, 1);
    printf("TX clamp config: 0x%02X\n", value);
    value |= 0x1E;
    sx126x_write(SX126X_TX_CLAMP_CONFIG, &value, 1);

    //# set frequency
    fun_sx126x_setFreq(frequency);

    //# PA and TX power setting
    u8 buff2[4] = {
        0x02,       // PA Duty Cycle
        0x03,       // HP Max
        0x00,       // Device Select
        0x01        // PowerLUT
    };
    sx126x_transfer(0x95, buff2, 4);

    buf[0] = 0x16;                  // Power
    buf[1] = SX126X_PA_RAMP_200U;   // RampTime
    sx126x_transfer(0x8E, buf, 2);

    //# set modulation
    fun_sx126x_setModulation(7, SX126X_BW_125000, SX126X_CR_4_5);

    //# set packet parameters
    u16 preambleLen = 12;
    u8 payloadLen = 15;
    fun_sx126x_setLoraPacket(0, preambleLen, payloadLen, 1);

    fun_sx126x_send("Hello World", 11);

    printf("\n-- LORA RECEIVER --\n");
}

// SetDioIrqParams
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
#define SX126X_IRQ_NONE                         0x0000      // no interrupts

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

void fun_sx126x_getPacketStatus(u16 *rssi, u16 *snr, u16* signalRssi) {
    uint8_t buff[4];
    sx126x_transfer(0x14, buff, 4);
    *rssi = buff[0] / -2;
    *snr = buff[1] / 4;
    *signalRssi = buff[2] / -2;
}

u16 fun_sx126x_getIRQStatus() {
    u8 buff3[3];
    sx126x_transfer(0x12, buff3, 3);
    return (buff3[1] << 8) | buff3[2];
}

u8 fun_sx126x_parsePacket() {
    if (fun_sx126x_getMode() == SX126X_STATUS_MODE_RX) return 0;

    //# clear IRQ status
    u8 buf[2];
    buf[0] = 0x03FF >> 8;
    buf[1] = 0x03FF;
    sx126x_transfer(0x02, buf, 2);

    //# clear previous interrupt and set RX done, 
    // RX timeout, header error, and CRC error as interrupt source
    u16 irqMask = SX126X_IRQ_RX_DONE | SX126X_IRQ_TIMEOUT |
                        SX126X_IRQ_HEADER_ERR | SX126X_IRQ_CRC_ERR;
    u8 buf8[8] = {0};
    buf8[0] = irqMask >> 8;
    buf8[1] = irqMask;
    sx126x_transfer(0x08, buf8, 8);

    //# set RX timeout
    u32 rx_timeout = 0;
    u8 buff3[3] = { rx_timeout >> 16, rx_timeout >> 8, rx_timeout };
    sx126x_transfer(0x82, buff3, 3);

    fun_sx126x_getIRQStatus();

    //# sx126x_getRxBufferStatus
    sx126x_transfer(0x13, buff3, 3);
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

    // Print packet/signal status including package RSSI and SNR
    u16 rssi, snr, signalRssi;
    fun_sx126x_getPacketStatus(&rssi, &snr, &signalRssi);
    printf("RSSI: %d dbm, SNR = %d dB\n",rssi, snr);
    return 1;
}

void fun_sx126x_send(char* message, uint8_t len) {
    //# sx126x_setBufferBaseAddress
    u8 buff[2] = {0x00, 0x00};
    sx126x_transfer(0x8F, buff, 2);

    //# write message
    u8* msgUint8 = (u8*) message;
    sx126x_writeBuffer(0x00, msgUint8, len);

    u16 preambleLen = 12;
    u8 payloadLen = 15;
    fun_sx126x_setLoraPacket(0, preambleLen, len, 1);

    //# sx126x_setTx
    u8 timeoutMs = 1000;
    u8 buff3[3] = { timeoutMs >> 16, timeoutMs >> 8, timeoutMs };
    sx126x_transfer(0x83, buff3, 3);

    //# clear IRQ status
    u16 status = fun_sx126x_getIRQStatus();
    buff[0] = status >> 8;
    buff[1] = status;
    sx126x_transfer(0x02, buff, 2);
}