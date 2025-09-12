#include "ch32fun.h"

// Set UART baud rate here
#define UART_BR 115200


// DMA transfer completion interrupt. It will fire when the DMA transfer is
// complete. We use it just to blink the LED
__attribute__((interrupt)) __attribute__((section(".srodata")))
void DMA1_Channel4_IRQHandler(void) {
	// Clear flag
	DMA1->INTFCR |= DMA_CTCIF4;

	// Blink LED
	// GPIOD->OUTDR ^= 1<<LED_PIN;
}

static void uart_setup(void) {
	// Enable UART and GPIOD
	RCC->APB2PCENR |= RCC_APB2Periph_GPIOD | RCC_APB2Periph_USART1;

	// Push-Pull, 10MHz Output on D5, with AutoFunction
	funPinMode( PD5, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP_AF); ;

	// Setup UART for Tx 8n1
	USART1->CTLR1 = USART_WordLength_8b | USART_Parity_No | USART_Mode_Tx;
	USART1->CTLR2 = USART_StopBits_1;
	// Enable Tx DMA event
	USART1->CTLR3 = USART_DMAReq_Tx;

	// Set baud rate and enable UART
	USART1->BRR = ((FUNCONF_SYSTEM_CORE_CLOCK) + (UART_BR)/2) / (UART_BR);
	USART1->CTLR1 |= CTLR1_UE_Set;
}

static void dma_uart_setup(void) {
	// Enable DMA peripheral
	RCC->AHBPCENR = RCC_AHBPeriph_SRAM | RCC_AHBPeriph_DMA1;

	// Disable channel just in case there is a transfer in progress
	DMA1_Channel4->CFGR &= ~DMA_CFGR1_EN;

	// USART1 TX uses DMA channel 4
	DMA1_Channel4->PADDR = (uint32_t)&USART1->DATAR;
	// MEM2MEM: 0 (memory to peripheral)
	// PL: 0 (low priority since UART is a relatively slow peripheral)
	// MSIZE/PSIZE: 0 (8-bit)
	// MINC: 1 (increase memory address)
	// CIRC: 0 (one shot)
	// DIR: 1 (read from memory)
	// TEIE: 0 (no tx error interrupt)
	// HTIE: 0 (no half tx interrupt)
	// TCIE: 1 (transmission complete interrupt enable)
	// EN: 0 (do not enable DMA yet)
	DMA1_Channel4->CFGR = DMA_CFGR1_MINC | DMA_CFGR1_DIR | DMA_CFGR1_TCIE;

	// Enable channel 4 interrupts
	NVIC_EnableIRQ(DMA1_Channel4_IRQn);
}

static void dma_uart_tx(const void *data, uint32_t len)
{
	// Disable DMA channel (just in case a transfer is pending)
	// DMA1_Channel4->CFGR &= ~DMA_CFGR1_EN;
	// Set transfer length and source address
	DMA1_Channel4->CNTR = len;
	DMA1_Channel4->MADDR = (uint32_t)data;
	// Enable DMA channel to start the transfer
	DMA1_Channel4->CFGR |= DMA_CFGR1_EN;
}




//# UART RECEIVE
void process_cmd(u8* buf) {
	if (strncmp((char*)buf, "toggle\r\n", 9) == 0) {
		GPIOC->OUTDR ^= (1<<7);
		printf("Horay!\r\n");
	} else {
		printf("Invalid cmd: %s\r\n", buf);
	}	
}

#define RX_BUF_LEN 16 // size of receive circular buffer

u8 rx_buf[RX_BUF_LEN] = {0}; // DMA receive buffer for incoming data
u8 cmd_buf[RX_BUF_LEN] = {0}; // buffer for complete command strings

void uart_rx_setup() {
	// enable rx pin
	USART1->CTLR1 |= USART_CTLR1_RE;

	// enable usart's dma rx requests
	USART1->CTLR3 |= USART_CTLR3_DMAR;

	// enable dma clock
	RCC->AHBPCENR |= RCC_DMA1EN;

	// configure dma for UART reception, it should fire on RXNE
	DMA1_Channel5->MADDR = (u32)&rx_buf;
	DMA1_Channel5->PADDR = (u32)&USART1->DATAR;
	DMA1_Channel5->CNTR = RX_BUF_LEN;

	// MEM2MEM: 0 (memory to peripheral)
	// PL: 0 (low priority since UART is a relatively slow peripheral)
	// MSIZE/PSIZE: 0 (8-bit)
	// MINC: 1 (increase memory address)
	// PINC: 0 (peripheral address remains unchanged)
	// CIRC: 1 (circular)
	// DIR: 0 (read from peripheral)
	// TEIE: 0 (no tx error interrupt)
	// HTIE: 0 (no half tx interrupt)
	// TCIE: 0 (no transmission complete interrupt)
	// EN: 1 (enable DMA)
	DMA1_Channel5->CFGR = DMA_CFGR1_CIRC | DMA_CFGR1_MINC | DMA_CFGR1_EN;
}


void uart_rx_task() {
	static u32 tail = 0; // current read position in rx_buf
	static u32 cmd_end = 0; // end index of current command in rx_buf
	static u32 cmd_st = 0; // start index of current command in rx_buf

	// calculate head position based on DMA counter (modulo when DMA1_Channel5->CNTR = 0)
	u32 head = (RX_BUF_LEN - DMA1_Channel5->CNTR) % RX_BUF_LEN; // current write position in rx_buf
	
	// process new bytes in rx_buf. when a newline character is detected, the command is copied to cmd_buf
	while (tail != head)
	{
		if ( rx_buf[tail] == '\n' ) 
		{
			cmd_end = tail;
			u32 cmd_i = 0; // carret position in cmd_buf
			if (cmd_end > cmd_st)
			{
				for (u32 rx_i = cmd_st; rx_i < cmd_end + 1; rx_i++, cmd_i++) {
					cmd_buf[cmd_i] = rx_buf[rx_i];
				}
			} else if (cmd_st > cmd_end) { // handle wrap around
				for (u32 rx_i = cmd_st; rx_i < RX_BUF_LEN; rx_i++, cmd_i++) {
					cmd_buf[cmd_i] = rx_buf[rx_i];
				}
				for (u32 rx_i = 0; rx_i < cmd_end + 1; rx_i++, cmd_i++) {
					cmd_buf[cmd_i] = rx_buf[rx_i];
				}
			}

			// null terminate
			cmd_buf[cmd_i] = '\0';

			process_cmd(cmd_buf);

			// update start position for next command
			cmd_st = (cmd_end + 1) % RX_BUF_LEN;
		}

		// move to next position 
		tail = (tail+1) % RX_BUF_LEN;
	}
}