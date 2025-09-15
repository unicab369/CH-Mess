#include "ch32fun.h"

#define SPI_SCLK 5  // PC5
#define SPI_MOSI 6  // PC6

static void SPI_init(void) {
    // reset control register
	SPI1->CTLR1 = 0;

    // Enable GPIO Port C and SPI peripheral
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOC | RCC_APB2Periph_SPI1;

    // PC5 - SCLK
    GPIOC->CFGLR &= ~(0xf << (SPI_SCLK << 2));
    GPIOC->CFGLR |= (GPIO_CNF_OUT_PP_AF | GPIO_Speed_50MHz) << (SPI_SCLK << 2);

    // PC6 - MOSI
    GPIOC->CFGLR &= ~(0xf << (SPI_MOSI << 2));
    GPIOC->CFGLR |= (GPIO_CNF_OUT_PP_AF | GPIO_Speed_50MHz) << (SPI_MOSI << 2);

    // PC7 - MISO
    GPIOC->CFGLR &= ~(0xf << (4 * 7));
    GPIOC->CFGLR |= GPIO_CNF_IN_FLOATING << (4 * 7);

    // Configure SPI
    SPI1->CTLR1 = SPI_CPHA_2Edge             // Bit 0     - Clock PHAse
                  | SPI_CPOL_Low             // Bit 1     - Clock POLarity - idles at the logical low voltage
                  | SPI_Mode_Master          // Bit 2     - Master device
                  | SPI_BaudRatePrescaler_8  // Bit 3-5   - F_HCLK / 2
                  | SPI_FirstBit_MSB         // Bit 7     - MSB transmitted first
                  | SPI_NSS_Soft             // Bit 9     - Software slave management
                  | SPI_DataSize_8b;         // Bit 11    - 8-bit data
    
    // SPI_Direction_1Line_Tx | SPI_Direction_2Lines_FullDuplex
    SPI1->CTLR1 |= SPI_Direction_2Lines_FullDuplex; 

    SPI1->CRCR = 7;                          // CRC
    SPI1->CTLR2 |= SPI_I2S_DMAReq_Tx;        // Configure SPI DMA Transfer
    SPI1->CTLR1 |= CTLR1_SPE_Set;            // Bit 6     - Enable SPI

    // Enable DMA peripheral
    RCC->AHBPCENR |= RCC_AHBPeriph_DMA1;

    // Config DMA for SPI TX
    DMA1_Channel3->CFGR = DMA_DIR_PeripheralDST          // Bit 4     - Read from memory
                          | DMA_Mode_Circular            // Bit 5     - Circulation mode
                          | DMA_PeripheralInc_Disable    // Bit 6     - Peripheral address no change
                          | DMA_MemoryInc_Enable         // Bit 7     - Increase memory address
                          | DMA_PeripheralDataSize_Byte  // Bit 8-9   - 8-bit data
                          | DMA_MemoryDataSize_Byte      // Bit 10-11 - 8-bit data
                          | DMA_Priority_VeryHigh        // Bit 12-13 - Very high priority
                          | DMA_M2M_Disable;             // Bit 14    - Disable memory to memory mode
    DMA1_Channel3->PADDR = (uint32_t)&SPI1->DATAR;
}


static void SPI_send_DMA(const uint8_t* buffer, uint16_t size, uint16_t repeat) {
    DMA1_Channel3->MADDR = (uint32_t)buffer;
    DMA1_Channel3->CNTR  = size;
    DMA1_Channel3->CFGR |= DMA_CFGR1_EN;  // Turn on channel

    // Circulate the buffer
    while (repeat--) {
        // Clear flag, start sending?
        DMA1->INTFCR = DMA1_FLAG_TC3;

        // Waiting for channel 3 transmission complete
        while (!(DMA1->INTFR & DMA1_FLAG_TC3))
            ;
    }

    DMA1_Channel3->CFGR &= ~DMA_CFGR1_EN;  // Turn off channel
}

static void SPI_send(uint8_t data) {
    // Send byte
    SPI1->DATAR = data;

    // Waiting for transmission complete
    while (!(SPI1->STATR & SPI_STATR_TXE)) ;
}

//! INTERFACES
void FN_SPI_DC_LOW();
void FN_SPI_DC_HIGH();

static void write_cmd_8(uint8_t cmd) {
    FN_SPI_DC_LOW();      // Command Mode
    SPI_send(cmd);
}

static void write_data_8(uint8_t data) {
    FN_SPI_DC_HIGH();     // Data Mode
    SPI_send(data);
}

static void write_data_16(uint16_t data) {
    FN_SPI_DC_HIGH();     // Data Mode
    SPI_send(data >> 8);
    SPI_send(data);
}


static inline void SPI_wait_TX_complete() {
    while (!(SPI1->STATR & SPI_STATR_TXE)) { }
}

static inline uint8_t SPI_is_RX_empty() {
    return SPI1->STATR & SPI_STATR_RXNE;
}

static inline void SPI_wait_RX_available() {
    while (!(SPI1->STATR & SPI_STATR_RXNE)) { }
}

static inline void SPI_wait_not_busy() {
    while ((SPI1->STATR & SPI_STATR_BSY) != 0) { }
}

static inline void SPI_wait_transmit_finished() {
    SPI_wait_TX_complete();
    SPI_wait_not_busy();
}


void SPI_end() {
    SPI1->CTLR1 &= ~(SPI_CTLR1_SPE);
}

static inline uint8_t SPI_read_8() {
    return SPI1->DATAR;
}

static inline void SPI_write_8(uint8_t data) {
    SPI1->DATAR = data;
}

uint8_t SPI_transfer_8(uint8_t data) {
    SPI_write_8(data);
    SPI_wait_TX_complete();
    asm volatile("nop");
    SPI_wait_RX_available();
    return SPI_read_8();
}

