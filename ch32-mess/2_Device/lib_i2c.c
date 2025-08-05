/******************************************************************************
* Lightweight and simple CH32V003 I2C Library.
*
* This library provides functions to init, read and write to the hardware I2C
* Bus - in Default, and Alternative Pinout Modes.
* Default:	SCL = PC2		SDA = PC1
* Alt 1:	SCL = PD1		SDA = PD0
* Alt 2:	SCL = PC5		SDA = PC6
*
* See GitHub Repo for more information: 
* https://github.com/ADBeta/CH32V003_lib_i2c
* Released under the MIT Licence
* Copyright ADBeta (c) 2024 - 2025
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to
* deal in the Software without restriction, including without limitation the 
* rights to use, copy, modify, merge, publish, distribute, sublicense, and/or 
* sell copies of the Software, and to permit persons to whom the Software is 
* furnished to do so, subject to the following conditions:
* The above copyright notice and this permission notice shall be included in 
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
* DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR 
* OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE 
* USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************/
#include "lib_i2c.h"
#include <stddef.h>

/*** Static Variables ********************************************************/
/// @brief Timeout variable. Set and decrimented in functions
static int32_t _i2c_timeout = 0;


/*** Macro Functions *********************************************************/
#define I2C_TIMEOUT_WAIT_FOR(condition, err_var) \
do { \
	_i2c_timeout = I2C_TIMEOUT; \
	while((condition)) \
		if(--_i2c_timeout <= 0) {(err_var) = i2c_get_busy_error(); break;} \
} while(0)


/*** Static Functions ********************************************************/
/// @brief Checks the I2C Status against a mask value, returns 1 if it matches
/// @param Status To match to
/// @return uint32_t masked status value: 1 if mask and status match
__attribute__((always_inline))
static inline uint32_t i2c_status(const uint32_t status_mask)
{
	uint32_t status = (uint32_t)I2C1->STAR1 | (uint32_t)(I2C1->STAR2 << 16);
	return (status & status_mask) == status_mask; 
}


/// @brief Gets and returns any error state on the I2C Interface, and resets
/// the bit flags
/// @param none
/// @return i2c_err_t error value
static inline i2c_err_t i2c_error(void)
{
	if(I2C1->STAR1 & I2C_STAR1_BERR)  {I2C1->STAR1 &= ~I2C_STAR1_BERR;  return I2C_ERR_BERR;}
	if(I2C1->STAR1 & I2C_STAR1_AF)    {I2C1->STAR1 &= ~I2C_STAR1_AF;    return I2C_ERR_NACK;}
	if(I2C1->STAR1 & I2C_STAR1_ARLO)  {I2C1->STAR1 &= ~I2C_STAR1_ARLO;  return I2C_ERR_ARLO;}
	if(I2C1->STAR1 & I2C_STAR1_OVR)   {I2C1->STAR1 &= ~I2C_STAR1_OVR;   return I2C_ERR_OVR;}

	return I2C_OK;
}


/// @brief Called when the I2C Bus Timesout - Returns any known error code
/// if applicable - returns generic I2C_ERR_BUSY if not
/// @param None
/// @return i2c_err_t error value
__attribute__((always_inline))
static inline uint32_t i2c_get_busy_error(void)
{

	i2c_err_t i2c_err = i2c_error();
	if(i2c_err == I2C_OK) i2c_err = I2C_ERR_BUSY;
	return i2c_err;
}


/// @brief Waits for the I2C Bus to be ready
/// @param None
/// @return i2c_err_t, I2C_OK if the bus is ready
__attribute__((always_inline))
static inline i2c_err_t i2c_wait()
{
	i2c_err_t i2c_ret = I2C_OK;
	I2C_TIMEOUT_WAIT_FOR((I2C1->STAR2 & I2C_STAR2_BUSY), i2c_ret);

	return i2c_ret;
}


/// @brief Starts the I2C Bus for communications
/// @param None
/// @return None
__attribute__((always_inline))
static inline void i2c_start()
{
	// Send a START Signal and wait for it to assert
	I2C1->CTLR1 |= I2C_CTLR1_START;
	while(!i2c_status(I2C_EVENT_MASTER_MODE_SELECT));
}


/// @brief Stops the I2C Bus
/// @param None
/// @return None
__attribute__((always_inline))
static inline void i2c_stop()
{
	I2C1->CTLR1 |= I2C_CTLR1_STOP;
}


/// @brief Sends the Address Byte(s) to the I2C Device in Write mode
/// @param i2c_dev_t device to address
/// @return i2c_err_r error status. I2C_OK on success
static inline i2c_err_t i2c_send_addr_write(const i2c_device_t *dev)
{
	i2c_err_t i2c_ret = I2C_OK;
	
	if(dev->type == I2C_ADDR_7BIT)
	{
		// Send the Address and wait for it to finish transmitting
		I2C1->DATAR = (dev->addr << 1) & 0xFE;
		I2C_TIMEOUT_WAIT_FOR(!i2c_status(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED), i2c_ret);
	}

/*
	if(dev->type == I2C_ADDR_10BIT)
	{
		uint8_t upper = 0xF0 | ((dev->addr & 0x0300) >> 7);
		uint8_t lower = dev->addr & 0xFF;
	}
*/
	return i2c_ret;
}


/// @brief Sends the Address Byte(s) to the I2C Device in Read mode
/// @param i2c_dev_t device to address
/// @return i2c_err_r error status. I2C_OK on success
static inline i2c_err_t i2c_send_addr_read(const i2c_device_t *dev)
{
	i2c_err_t i2c_ret = I2C_OK;

	if(dev->type == I2C_ADDR_7BIT)
	{
		// Send the Address and wait for it to finish transmitting
		I2C1->DATAR = (dev->addr << 1) | 0x01;
		I2C_TIMEOUT_WAIT_FOR(!i2c_status(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED), i2c_ret);
	}
	return i2c_ret;
}


/*** API Functions ***********************************************************/
i2c_err_t i2c_init(i2c_device_t *dev)
{
	// Limit the input regb to between 1-4
	if(dev->regb == 0) dev->regb = 1;
	if(dev->regb  > 4) dev->regb = 4;

	// Toggle the I2C Reset bit to init Registers
	RCC->APB1PRSTR |=  RCC_APB1Periph_I2C1;
	RCC->APB1PRSTR &= ~RCC_APB1Periph_I2C1;

	// Enable the I2C Peripheral Clock
	RCC->APB1PCENR |= RCC_APB1Periph_I2C1;

	// Enable the selected I2C Port, and the Alternate Function enable bit
	RCC->APB2PCENR |= I2C_PORT_RCC | RCC_APB2Periph_AFIO;

	// Reset the AFIO_PCFR1 register, then set it up
	AFIO->PCFR1 &= ~(0x04400002);
	AFIO->PCFR1 |= I2C_AFIO_REG;

	// Clear, then set the GPIO Settings for SCL and SDA, on the selected port
	I2C_PORT->CFGLR &= ~(0x0F << (4 * I2C_PIN_SDA));
	I2C_PORT->CFGLR |= (GPIO_Speed_10MHz | GPIO_CNF_OUT_OD_AF) << (4 * I2C_PIN_SDA);	
	I2C_PORT->CFGLR &= ~(0x0F << (4 * I2C_PIN_SCL));
	I2C_PORT->CFGLR |= (GPIO_Speed_10MHz | GPIO_CNF_OUT_OD_AF) << (4 * I2C_PIN_SCL);

	// Set the Prerate frequency
	uint16_t i2c_conf = I2C1->CTLR2 & ~I2C_CTLR2_FREQ;
	i2c_conf |= (FUNCONF_SYSTEM_CORE_CLOCK / I2C_PRERATE) & I2C_CTLR2_FREQ;
	I2C1->CTLR2 = i2c_conf;

	// Set I2C Clock
	if(dev->clkr <= 100000)
	{
		i2c_conf = (FUNCONF_SYSTEM_CORE_CLOCK / (2 * dev->clkr)) & I2C_CKCFGR_CCR;
	} else {
		// Fast mode. Default to 33% Duty Cycle
		i2c_conf = (FUNCONF_SYSTEM_CORE_CLOCK / (3 * dev->clkr)) & I2C_CKCFGR_CCR;
		i2c_conf |= I2C_CKCFGR_FS;
	}
	I2C1->CKCFGR = i2c_conf;

	// Enable the I2C Peripheral
	I2C1->CTLR1 |= I2C_CTLR1_PE;

	// Check error states
	return i2c_error();
}


i2c_err_t i2c_ping(const uint8_t addr)
{
	// Create a temporary i2c device using passed addr
	i2c_device_t tmp_dev = {.type = I2C_ADDR_7BIT, .addr = addr};
	
	// Wait for the bus to become free
	i2c_err_t i2c_ret = i2c_wait();

	// Send the address and get the status
	i2c_start();
	if(i2c_ret == I2C_OK) i2c_ret = i2c_send_addr_write(&tmp_dev);

	// Signal a STOP
	i2c_stop();

	return i2c_ret;
}


void i2c_scan(void (*callback)(const uint8_t))
{
	// If the callback function is null, exit
	if(callback == NULL) return;

	// Scan through every address, getting a ping() response
	for(uint8_t addr = 0x00; addr < 0x7F; addr++)
		// If the address responds, call the callback function
		if(i2c_ping(addr) == I2C_OK) callback(addr);
}


i2c_err_t i2c_read_raw(const i2c_device_t *dev,     uint8_t *buf,
                                                    const size_t len)
{
	// Wait for the I2C Bus to be Available
	i2c_err_t i2c_ret = i2c_wait();
	
	// Start the I2C Bus and send the Write Address byte
	if(i2c_ret == I2C_OK) { i2c_start(); i2c_ret = i2c_send_addr_write(dev); }

	// Enter Read Mode
	if(i2c_ret == I2C_OK)
	{
		// If the message is long enough, enable ACK messages
		if(len > 1) I2C1->CTLR1 |= I2C_CTLR1_ACK;

		// Send a Repeated START and send the Read Address
		i2c_start();
		i2c_ret = i2c_send_addr_read(dev);
	}

	// Read the data from the bus
	if(i2c_ret == I2C_OK)
	{
		uint8_t cbyte = 0;
		while(cbyte < len)
		{
			// If this is the last byte, send the NACK Bit
			if(cbyte == len - 1) I2C1->CTLR1 &= ~I2C_CTLR1_ACK;

			// Wait until there is data (Read Register Not Empty)
			I2C_TIMEOUT_WAIT_FOR(!(I2C1->STAR1 & I2C_STAR1_RXNE), i2c_ret);
			//while(!(I2C1->STAR1 & I2C_STAR1_RXNE));
			buf[cbyte] = I2C1->DATAR;
			++cbyte;

			// Make sure no errors occured for this byte
			if(i2c_ret != I2C_OK || (i2c_ret = i2c_error()) != I2C_OK) break;
		}
	}

	// Signal a STOP
	i2c_stop();

	return i2c_ret;
}


i2c_err_t i2c_write_raw(const i2c_device_t *dev,    const uint8_t *buf,
                                                    const size_t len)
{
	// Wait for the I2C Bus the be Available
	i2c_err_t i2c_ret = i2c_wait();

	// Start the I2C Bus and send the Write Address byte
	if(i2c_ret == I2C_OK) { i2c_start(); i2c_ret = i2c_send_addr_write(dev); }

	// Write the data
	if(i2c_ret == I2C_OK)
	{
		uint8_t cbyte = 0;
		while(cbyte < len)
		{
			// Write the byte and wait for it to finish transmitting
			I2C_TIMEOUT_WAIT_FOR(!(I2C1->STAR1 & I2C_STAR1_TXE), i2c_ret);
			//while(!(I2C1->STAR1 & I2C_STAR1_TXE));
			I2C1->DATAR = buf[cbyte];
			++cbyte;

			// Make sure no errors occured for this byte
			if(i2c_ret != I2C_OK || (i2c_ret = i2c_error()) != I2C_OK) break;
		}
	}

	// Wait for the bus to finish transmitting
	I2C_TIMEOUT_WAIT_FOR(!i2c_status(I2C_EVENT_MASTER_BYTE_TRANSMITTED), i2c_ret);
	// Signal a STOP
	i2c_stop();

	return i2c_ret;



}


i2c_err_t i2c_read_reg(const i2c_device_t *dev,     const uint32_t reg,
                                                    uint8_t *buf,
                                                    const size_t len)
{
	// Wait for the I2C Bus to be Available
	i2c_err_t i2c_ret = i2c_wait();
	
	// Start the I2C Bus and send the Write Address byte
	if(i2c_ret == I2C_OK) { i2c_start(); i2c_ret = i2c_send_addr_write(dev); }

	// Send the register byte/s - MSBFirst
	if(i2c_ret == I2C_OK)
	{
		for(int8_t b = dev->regb - 1; b >= 0; b--)
		{
			uint8_t reg_byte = (reg >> (8 * b)) & 0xFF;
			I2C1->DATAR = reg_byte;

			I2C_TIMEOUT_WAIT_FOR(!(I2C1->STAR1 & I2C_STAR1_TXE), i2c_ret);
			if(i2c_ret != I2C_OK || (i2c_ret = i2c_error()) != I2C_OK) break;
		}
	}

	// Enter Read Mode
	if(i2c_ret == I2C_OK)
	{
		// If the message is long enough, enable ACK messages
		if(len > 1) I2C1->CTLR1 |= I2C_CTLR1_ACK;

		// Send a Repeated START and send the Read Address
		i2c_start();
		i2c_ret = i2c_send_addr_read(dev);
	}

	// Read the data from the bus
	if(i2c_ret == I2C_OK)
	{
		uint8_t cbyte = 0;
		while(cbyte < len)
		{
			// If this is the last byte, send the NACK Bit
			if(cbyte == len - 1) I2C1->CTLR1 &= ~I2C_CTLR1_ACK;

			// Wait until there is data (Read Register Not Empty)
			I2C_TIMEOUT_WAIT_FOR(!(I2C1->STAR1 & I2C_STAR1_RXNE), i2c_ret);
			//while(!(I2C1->STAR1 & I2C_STAR1_RXNE));
			buf[cbyte] = I2C1->DATAR;
			++cbyte;

			// Make sure no errors occured for this byte
			if(i2c_ret != I2C_OK || (i2c_ret = i2c_error()) != I2C_OK) break;
		}
	}

	// Signal a STOP
	i2c_stop();

	return i2c_ret;
}


i2c_err_t i2c_write_reg(const i2c_device_t *dev,    const uint32_t reg,
                                                    const uint8_t *buf,
                                                    const size_t len)
{
	// Wait for the I2C Bus the be Available
	i2c_err_t i2c_ret = i2c_wait();

	// Start the I2C Bus and send the Write Address byte
	if(i2c_ret == I2C_OK) { i2c_start(); i2c_ret = i2c_send_addr_write(dev); }

	// Send the register byte/s - MSBFirst
	if(i2c_ret == I2C_OK)
	{
		for(int8_t b = dev->regb - 1; b >= 0; b--)
		{
			uint8_t reg_byte = (reg >> (8 * b)) & 0xFF;
			I2C1->DATAR = reg_byte;

			I2C_TIMEOUT_WAIT_FOR(!(I2C1->STAR1 & I2C_STAR1_TXE), i2c_ret);
			if(i2c_ret != I2C_OK || (i2c_ret = i2c_error()) != I2C_OK) break;
		}
	}

	// Write the data
	if(i2c_ret == I2C_OK)
	{
		uint8_t cbyte = 0;
		while(cbyte < len)
		{
			// Write the byte and wait for it to finish transmitting
			I2C_TIMEOUT_WAIT_FOR(!(I2C1->STAR1 & I2C_STAR1_TXE), i2c_ret);
			//while(!(I2C1->STAR1 & I2C_STAR1_TXE));
			I2C1->DATAR = buf[cbyte];
			++cbyte;

			// Make sure no errors occured for this byte
			if(i2c_ret != I2C_OK || (i2c_ret = i2c_error()) != I2C_OK) break;
		}
	}

	// Wait for the bus to finish transmitting
	I2C_TIMEOUT_WAIT_FOR(!i2c_status(I2C_EVENT_MASTER_BYTE_TRANSMITTED), i2c_ret);
	// Signal a STOP
	i2c_stop();

	return i2c_ret;
}