/******************************************************************************
* Lightweight and simple CH32V003 I2C Library.
*
* This library provides functions to init, read and write to the hardware I2C
* Bus - in Default, and Alternative Pinout Modes.
* Default:	SCL = PC2		SDA = PC1
* Alt 1:	SCL = PD1		SDA = PD0
* Alt 2:	SCL = PC5		SDA = PC6
*
* Version 5.0    12 July 2025
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
#ifndef CH32_LIB_I2C_H
#define CH32_LIB_I2C_H

#include "ch32fun.h"

#include <stddef.h>

#define I2C_PINOUT_DEFAULT
//#define I2C_PINOUT_ALT_1
//#define I2C_PINOUT_ALT_2

/*** Hardware Definitions ****************************************************/
// Predefined Clock Speeds
#define I2C_CLK_10KHZ     10000
#define I2C_CLK_50KHZ     50000
#define I2C_CLK_100KHZ    100000
#define I2C_CLK_400KHZ    400000
#define I2C_CLK_500KHZ    500000
#define I2C_CLK_600KHZ    600000
#define I2C_CLK_750KHZ    750000
#define I2C_CLK_1MHZ      1000000

// Hardware CLK Prerate and timeout
#define I2C_PRERATE       2000000
#define I2C_TIMEOUT       10000

// Default Pinout
#ifdef I2C_PINOUT_DEFAULT
	#define I2C_AFIO_REG	((uint32_t)0x00000000)
	#define I2C_PORT_RCC	RCC_APB2Periph_GPIOC
	#define I2C_PORT		GPIOC
	#define I2C_PIN_SCL 	2
	#define I2C_PIN_SDA 	1
#endif

// Alternate 1 Pinout
#ifdef I2C_PINOUT_ALT_1
	#define I2C_AFIO_REG	((uint32_t)0x04000002)
	#define I2C_PORT_RCC	RCC_APB2Periph_GPIOD
	#define I2C_PORT		GPIOD
	#define I2C_PIN_SCL 	1
	#define I2C_PIN_SDA 	0
#endif

// Alternate 2 Pinout
#ifdef I2C_PINOUT_ALT_2
	#define I2C_AFIO_REG	((uint32_t)0x00400002)
	#define I2C_PORT_RCC	RCC_APB2Periph_GPIOC
	#define I2C_PORT		GPIOC
	#define I2C_PIN_SCL 	5
	#define I2C_PIN_SDA 	6
#endif

/*** Types and Enums *********************************************************/
/// @brief I2C Error Codes
typedef enum {
	I2C_OK	  = 0,   // No Error. All OK
	I2C_ERR_BERR,	 // Bus Error
	I2C_ERR_NACK,	 // ACK Bit failed
	I2C_ERR_ARLO,	 // Arbitration Lost
	I2C_ERR_OVR,	 // Overun/underrun condition
	I2C_ERR_BUSY,	 // Bus was busy and timed out
} i2c_err_t;


/// @brief I2C Address Mode Types:
/// 7bit is the standard I2C Address mode. ADDR[LSB] is the read/write bit
/// 10bit ADDR is 2 bytes, the ADDR[0][LSB] is the read/write bit.
/// 16bit Mode sends the Address raw as 2 bytes to support some devices
typedef enum {
	I2C_ADDR_7BIT     = 0,
//	I2C_ADDR_10BIT,
//	I2C_ADDR_16BIT
} i2c_addr_t;


typedef struct {
	uint32_t      clkr;  // Clock Rate (in Hz)
	i2c_addr_t    type;  // Address Type - Determines address behaviour
	uint16_t      addr;  // Address Value. Default is WRITE in 7 and 10bit
	uint8_t       regb;  // Register Bytes 1-4 (Capped to sane range in in init())
} i2c_device_t;


/*** Functions ***************************************************************/
/// @brief Initialise the I2C Peripheral on the default pins, in Master Mode
/// @param i2c_dev_t device config - for clock speed, and limits values
/// @return i2c_err_t, I2C_OK On success
i2c_err_t i2c_init(i2c_device_t *dev);


/// @brief Pings a specific I2C Address, and returns a i2c_err_t status
/// @param addr I2C Device Address,                    NOTE: 7BIT ADDRESS ONLY
/// @return i2c_err_t, I2C_OK if the device responds
i2c_err_t i2c_ping(const uint8_t addr);


/// @brief Scans through all 7 Bit addresses, prints any that respond
/// @param callback function - returns void, takes uint8_t
///                                                    NOTE: 7BIT ADDRESS ONLY
/// @return None
void i2c_scan(void (*callback)(const uint8_t));


/// @brief reads [len] bytes from the I2C Device into [buf]
/// @param dev, I2C Device to Read from
/// @param buf, buffer to read to
/// @param len, number of bytes to read
/// @return 12c_err_t. I2C_OK on Success
i2c_err_t i2c_read_raw(const i2c_device_t *dev,     uint8_t *buf,
                                                    const size_t len);


/// @brief writes [len] bytes from [buf], to the I2C Device
/// @param dev, I2C Device to Write to
/// @param buf, Buffer to write from
/// @param len, number of bytes to write
/// @return i2c_err_t. I2C_OK On Success.
i2c_err_t i2c_write_raw(const i2c_device_t *dev,    const uint8_t *buf,
                                                    const size_t len);


/// @brief reads [len] bytes from [addr]s [reg] register into [buf]
/// @param dev, I2C Device to Read from
/// @param reg, register to read from - up to 4 bytes
/// @param buf, buffer to read to
/// @param len, number of bytes to read
/// @return 12c_err_t. I2C_OK on Success
i2c_err_t i2c_read_reg(const i2c_device_t *dev,     const uint32_t reg,
                                                    uint8_t *buf,
                                                    const size_t len);


/// @brief writes [len] bytes from [buf], to the [reg] of [addr]
/// @param dev, I2C Device to Write to
/// @param reg, register to write to
/// @param buf, Buffer to write from
/// @param len, number of bytes to write
/// @return i2c_err_t. I2C_OK On Success.
i2c_err_t i2c_write_reg(const i2c_device_t *dev,    const uint32_t reg,
                                                    const uint8_t *buf,
                                                    const size_t len);

#endif