#include "ch32fun.h"
#include <stdio.h>

#include "../lib_i2c.h"
#include "../ssd1306/fun_ssd1306.h"
#include "../../modules/systick_irq.h"

#ifndef PACKED
#define PACKED __attribute__((packed))
#endif

i2c_device_t dev_ssd1306 = {
	.clkr = I2C_CLK_100KHZ,
	.type = I2C_ADDR_7BIT,
	.addr = 0x3C,				// Default address for SSD1306
	.regb = 1,
};

/* send OLED command byte */
uint8_t ssd1306_cmd(uint8_t cmd) {
	uint8_t pkt[2];
	pkt[0] = 0;
	pkt[1] = cmd;
	return i2c_write_raw(&dev_ssd1306, pkt, 2);
}

/* send OLED data packet (up to 32 bytes) */
uint8_t ssd1306_data(uint8_t *data, int sz) {
	uint8_t pkt[33];
	pkt[0] = 0x40;
	memcpy(&pkt[1], data, sz);
	return i2c_write_raw(&dev_ssd1306, pkt, sz+1);
}


//! BH1750
#define BH17_CONT_HI1 0x10      // 1 lux resolution 120ms
#define BH17_CONT_HI2 0x11      // .5 lux resolution 120ms
#define BH17_CONT_LOW 0x13      // 4 lux resolution 16ms
#define BH17_ONCE_HI1 0x20      // 1 lux resolution 120ms
#define BH17_ONCE_HI2 0x21      // .5 lux resolution 120ms
#define BH17_ONCE_LOW 0x23      // 4 lux resolution 16ms
#define BH17_RESET 0x07
#define BH17_POWER_ON 0x01


//! SHT30
uint8_t SHT_RESET_CMD[2]        = { 0x30, 0xA2 };
uint8_t SHT_HIGHREP_HOLD_CMD[2] = { 0x2C, 0x06 };
uint8_t SHT_MEDREP_HOLD_CMD[2]  = { 0x2C, 0x0D };
uint8_t SHT_LOWREP_HOLD_CMD[2]  = { 0x2C, 0x10 };
uint8_t SHT_HIGHREP_FREE_CMD[2] = { 0x24, 0x00 };
uint8_t SHT_MEDREP_FREE_CMD[2]  = { 0x24, 0x0B };
uint8_t SHT_LOWREP_FREE_CMD[2]  = { 0x24, 0x16 };
uint8_t SHT_HEATER_DISABLE[2]   = { 0x30, 0x66 };
uint8_t SHT_HEATER_ENABLE[2]    = { 0x30, 0x6D };

i2c_device_t dev_sensor = {
	.clkr = I2C_CLK_100KHZ,
	.type = I2C_ADDR_7BIT,
	.addr = 0x3C,
	.regb = 1,
};

uint16_t hexToDecimal(const uint8_t* hexArray, size_t length) {
	uint16_t result = 0;
	
	for (size_t i = 0; i < length; i++) {
		result = (result << 8) | hexArray[i];
	}
	
	return result;
}

void i2c_getReadings(struct SensorData* data) {
   // uint16_t lux = 0;
	// uint16_t tempC = 0, tempF = 0, hum = 0;
	// uint16_t busVoltage = 0, mA = 0;

	//! BH1750 Reading
	// int check = I2CTest(BH17_ADDR);

	// if (check) { 
	// 	data->lux = BH17_Read(); 
	// }

	// char strOut[32];
	// mini_snprintf(strOut, sizeof(strOut), "\nlux=%lu", lux);
	// printf(strOut);
	// ssd1306_drawstr_sz(0, 16, strOut, 1, fontsize_8x8);

	// //! SHT31 Reading
	// int check2 = I2CTest(SHT3_ADDR);

	// if (check2) {
	// 	I2CWrite(SHT3_ADDR, SHT_LOWREP_FREE_CMD, sizeof(SHT_LOWREP_FREE_CMD));
	// 	Delay_Ms(5);
	// 	uint8_t buff[6];
	// 	I2CRead(SHT3_ADDR, buff, 6);

	// 	uint16_t rawTemp = 0, rawHum = 0;
	// 	rawTemp = (buff[0]<<8) + buff[1];
	// 	rawHum = (buff[3]<<8) + buff[4];

	// 	// tempC = rawTemp*175/65535 - 45;
	// 	data->tempF = rawTemp*63/13107 - 49;
	// 	data->hum = rawHum*100/65535;
	// }

	// mini_snprintf(strOut, sizeof(strOut), "\nt=%lu, h=%lu", tempF, hum);
	// printf(strOut);

	// //! INA219 Reading
	// int check3 = I2CTest(INA219_ADDR);

	// if (check3) {
	// 	data->voltage = INA219_Read();
	// }

   // printf("\n\rtemp=%lu, hum=%lu, lux=%lu, v=%lu, mA=%lu", tempC, hum, lux, busVoltage, mA);
   // mini_snprintf(strOut, sizeof(strOut), "\nV=%lu, mA=%lu", busVoltage, mA);
   // printf(strOut);		

   // uint8_t allValues[] = { 0xB1, 0xCA, 0xFF, 0xFF, 0xFF, 0xFF, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x00, 0x0D, 0x0A };
   // uint16_t allValues[] = { 0xCAB1, 0xFFFF, 0xFFFF, tempF, hum, lux, busVoltage, 0x0A0D };
}


i2c_err_t ret;
uint8_t buff[8];

void test_htu21() {
    dev_sensor.addr = 0x40;

    if (i2c_ping(dev_sensor.addr) != I2C_OK) {
		printf("HTU21 not found\n");
		return;
	}

	Delay_Ms(100);
	ret = i2c_write_raw(&dev_sensor, (uint8_t[]){0xFE}, 1);		// Soft reset
	printf("Error0: %d\n", ret);

	// Delay_Ms(100);
	// ret = i2c_read_reg(&dev_sensor, 0xE7, buff, 3);		// Read User register
	// printf("Error1: %d\n", ret);
	// printf("HTU21 User Register: %02X, %02X, %02X\n", buff[0], buff[1], buff[2]);


	Delay_Ms(200);
	// ret = i2c_read_reg(&dev_sensor, 0xF3, buff, 3);				// Read Temperature
	ret = i2c_write_raw(&dev_sensor, (uint8_t[]){0xF3}, 1);
	printf("Error3: %d\n", ret);

	Delay_Ms(200);
	ret = i2c_read_raw(&dev_sensor, buff, 3);				// Read Temperature
	printf("Error4: %d\n", ret);
	uint16_t temp_raw = (buff[0] << 8) | buff[1];
	uint16_t temp = (temp_raw * 17572) >> 16;					// >> 16 is equivalent to / 65536

	Delay_Ms(200);
	// ret = i2c_read_reg(&dev_sensor, 0xF5, buff, 3);				// Read Humidity
	ret = i2c_write_raw(&dev_sensor, (uint8_t[]){0xF5}, 1);
	printf("Error4: %d\n", ret);

	Delay_Ms(200);
	ret = i2c_read_raw(&dev_sensor, buff, 3);				// Read Humidity
	printf("Error5: %d\n", ret);
	uint16_t hum_raw = (buff[0] << 8) | buff[1];
	uint16_t hum = (hum_raw * 125) >> 16;							// >> 16 is equivalent to / 65536

	printf("HTU21 Temp*100: %d, Hum: %d\n", temp, hum);
}

void test_aht21() {
    dev_sensor.addr = 0x38;

    if (i2c_ping(dev_sensor.addr) != I2C_OK) {
		printf("AHT21 not found\n");
		return;
	}

	ret = i2c_read_reg(&dev_sensor, 0x71, buff, 1);
	buff[0] = buff[0] & 0x18;
	printf("Error0: %d\n", ret);
	printf("AHT21 Read reg 0x71: %02X\n", buff[0]);		// expect 0x18

	buff[0] = 0xAC;
	buff[1] = 0X33;
	buff[2] = 0x00;
	ret = i2c_write_raw(&dev_sensor, buff, 3);
	printf("Error1: %d\n", ret);

	ret = i2c_read_raw(&dev_sensor, buff, 6);		// Read sensor
	printf("Error2: %d\n", ret);

	// calculate humidity
	uint32_t hum = (buff[1] << 12) | (buff[2] << 4) | (buff[3] >> 4);
	hum = (hum * 100) / 0x100000;
	printf("Humidity: %lu\n", hum);

	// calculate temperature
	uint32_t temp = ((buff[3] & 0xF) << 16) | (buff[4] << 8) | (buff[5]);
	temp = (temp * 200) / 0x100000;
	printf("Temperature: %lu\n", temp);
}

void test_bmp280() {
    dev_sensor.addr = 0x76;

    if (i2c_ping(dev_sensor.addr) != I2C_OK) {
		printf("BMP280 not found\n");
		return;
	}

	struct {
		int16_t dig_T1;
		int16_t dig_T2;
		int16_t dig_T3;
		// Pressure calibration would be added similarly
	} bmp280_calib;

	ret = i2c_read_reg(&dev_sensor, 0xD0, buff, 1);
	printf("Error0: %d\n", ret);
	printf("BMP280 Read reg 0xD0: %02X\n", buff[0]);		// expect 0x58

	ret = i2c_read_reg(&dev_sensor, 0x88, buff, 2);
	bmp280_calib.dig_T1 =  (int16_t)(buff[1] << 8) | buff[0];

	ret = i2c_read_reg(&dev_sensor, 0x8A, buff, 2);
	bmp280_calib.dig_T2 =  (int16_t)(buff[1] << 8) | buff[0];

	ret = i2c_read_reg(&dev_sensor, 0x8C, buff, 2);
	bmp280_calib.dig_T3 =  (int16_t)(buff[1] << 8) | buff[0];

	printf("dig_T1: %lu\n", bmp280_calib.dig_T1);
	printf("dig_T2: %lu\n", bmp280_calib.dig_T2);
	printf("dig_T3: %ld\n", bmp280_calib.dig_T3);

	ret = i2c_write_reg(&dev_sensor, 0xF4, (uint8_t[]){0x5D}, 1);
	printf("Error1: %d\n", ret);
	ret = i2c_read_reg(&dev_sensor, 0xF7, buff, 6);			// read sensors

	uint32_t raw_pressure = (uint32_t)(buff[0]*4096 + buff[1]*16 + (buff[2]/16));
	uint32_t raw_temp = (uint32_t)(buff[3]*4096 + buff[4]*16 + (buff[5]/16));

	uint32_t var1 = (raw_temp/16384 - bmp280_calib.dig_T1/1024) * bmp280_calib.dig_T2;
	uint32_t var2 = ((raw_temp/131072 - bmp280_calib.dig_T1/8192) * (raw_temp/131072 - bmp280_calib.dig_T1/8192)) * bmp280_calib.dig_T3;
	uint32_t t_fine = var1 + var2;		// t_fine in 0.01 degrees Celsius

	uint32_t temp = t_fine/5120;
	printf("Temperature: %lu\n", temp);

}

void test_hdc1080() {
    dev_sensor.addr = 0x40;

    if (i2c_ping(dev_sensor.addr) != I2C_OK) {
		printf("HDC1080 not found\n");
		return;
	}

	ret = i2c_write_reg(&dev_sensor, 0x02, (uint8_t[]){0x10}, 1);	// Set configuration register
	printf("Error0: %d\n", ret);

	ret = i2c_write_raw(&dev_sensor, (uint8_t[]){0x00}, 1);		// Trigger measurement
	printf("Error1: %d\n", ret);

	Delay_Ms(100);											//! Wait for measurement to complete
	ret = i2c_read_raw(&dev_sensor, buff, 4);				// Read data register
	printf("Error2: %d\n", ret);
	printf("HDC1080 Read: %02X %02X %02X %02X\n", buff[0], buff[1], buff[2], buff[3]);

	uint16_t raw_temp = (buff[0] << 8) | buff[1];
	uint16_t raw_hum = (buff[2] << 8) | buff[3];

	uint32_t temp = (raw_temp * 165 / 65536) - 40;
	uint32_t hum = (raw_hum * 100 / 65536);

	printf("Temperature: %lu\n", temp);
	printf("Humidity: %lu\n", hum);
}

void test_si7021() {
    dev_sensor.addr = 0x40;

    if (i2c_ping(dev_sensor.addr) != I2C_OK) {
		printf("SI7021 not found\n");
		return;
	}

	ret = i2c_write_raw(&dev_sensor, (uint8_t[]){0xE3}, 1);
	printf("Error0: %d\n", ret);

	ret = i2c_read_raw(&dev_sensor, buff, 4);				// Read Temperature and Humidity
	printf("Error0: %d\n", ret);

	uint16_t raw_temp = (buff[0] << 8) | buff[1];
	uint32_t temp = ((raw_temp * 17572) >> 16) - 4685;		// >> 16 is equivalent to / 65536
	printf("Temp*100: %lu\n", temp);

	uint16_t raw_hum = (buff[2] << 8) | buff[3];
	uint32_t hum = ((raw_hum * 12500) >> 16) - 600;
	printf("Hum*100: %lu\n", hum);
}

void test_max44009() {
    dev_sensor.addr = 0x4A;

    if (i2c_ping(dev_sensor.addr) != I2C_OK) {
		printf("Max44009 not found\n");
		return;
	}

	ret = i2c_read_reg(&dev_sensor, 0x03, buff, 2);		// Read lux registers
	printf("Error0: %d\n", ret);

	int exponent = (buff[0] & 0xF0) >> 4;		// Get exponent from first byte
	int mantissa = ((buff[0] & 0x0F)) << 4 | (buff[1] & 0x0F);	// Get mantissa from both bytes

	uint16_t lux = (1 << exponent) * mantissa * 45;
	printf("Max44009: %02X %02X\n", buff[0], buff[1]);
	printf("lux*1000: %lu\n", lux);
}

void test_ap3216() {
    dev_sensor.addr = 0x1E;

    if (i2c_ping(dev_sensor.addr) != I2C_OK) {
		printf("AP3216 not found\n");
		return;
	}

	ret = i2c_write_raw(&dev_sensor, (uint8_t[]){0x00, 0x03}, 2);		// Trigger measurement
	printf("Error0: %d\n", ret);

	ret = i2c_read_reg(&dev_sensor, 0x0C, buff, 4);		// Read data registers
	printf("Error1: %d\n", ret);

	uint16_t lux = (buff[1] << 8) + buff[0];

	uint8_t low_byte = buff[2] & 0b00001111;		// Get low byte of lux
	uint8_t high_byte = buff[3] & 0x00111111;		// Get high byte of lux
	uint16_t proximity = (high_byte << 4) + low_byte;
	printf("lux: %d, proximity: %d\n", lux, proximity);
}

void test_vl53l0x() {
    dev_sensor.addr = 0x29;

    if (i2c_ping(dev_sensor.addr) != I2C_OK) {
		printf("VL53L0X not found\n");
		return;
	}

	// ret = i2c_write_raw(&dev_sensor, (uint8_t[]){0x00, 0x01}, 2);		// Trigger measurement

	uint8_t status;
	ret = i2c_read_reg(&dev_sensor, 0xC0, &status, 1);		// Read system status register
	printf("Error0: %d\n", ret);
	printf("status: %02X\n", status);						// expect 0xEE
	
	// ret = i2c_write_reg(&dev_sensor, 0x88, (uint8_t[]){0x00}, 1);		// Start initialization

	// ret = i2c_write_reg(&dev_sensor, 0x80, (uint8_t[]){0x01}, 1);
	// ret = i2c_write_reg(&dev_sensor, 0xFF, (uint8_t[]){0x01}, 1);
	// ret = i2c_write_reg(&dev_sensor, 0x00, (uint8_t[]){0x00}, 1);

	// // ret = i2c_read_reg(&dev_sensor, 0x91, buff, 1);		// Read I2C mode register
	// // printf("Error1: %d\n", ret);
	// // printf("VL53L0X Read reg 0x91: %02X\n", buff[0]);		// expect 0x3C
	// ret = i2c_write_reg(&dev_sensor, 0x91, (uint8_t[]){0x3C}, 1);		// Set I2C mode

	// ret = i2c_write_reg(&dev_sensor, 0x00, (uint8_t[]){0x01}, 1);
	// ret = i2c_write_reg(&dev_sensor, 0xFF, (uint8_t[]){0x00}, 1);
	// ret = i2c_write_reg(&dev_sensor, 0x80, (uint8_t[]){0x00}, 1);

	// start measurement
	ret = i2c_write_reg(&dev_sensor, 0x00, (uint8_t[]){0x00}, 1);

	Delay_Ms(100);											// Wait for measurement to complete

	ret = i2c_read_reg(&dev_sensor, 0x13, buff, 1);
	printf("Error0: %d\n", ret);
	printf("VL53L0X Read reg 0x13: %02X\n", buff[0]);

	Delay_Ms(100);	
	ret = i2c_read_reg(&dev_sensor, 0x14 + 10, buff, 2);				// Read distance register
	printf("Error1: %d\n", ret);

	printf("VL53L0X Read: %02X %02X\n", buff[0], buff[1]);
	uint16_t distance = (buff[1] << 8) + buff[0];
	printf("distance: %d\n", distance);
}

void test_ens160() {
    dev_sensor.addr = 0x53;

    if (i2c_ping(dev_sensor.addr) != I2C_OK) {
		printf("ENS160 not found\n");
		return;
	}

	ret = i2c_read_reg(&dev_sensor, 0x00, buff, 2);		// Read ENS160 ID
	uint16_t ens160_id = (buff[1] << 8) | buff[0];
	printf("ENS160 ID: %04X\n", ens160_id);
	ret = i2c_write_reg(&dev_sensor, 0x10, (uint8_t[]){0x02}, 1);		// set Mode

	// get Air Quality Index (AQI)
	// value: 1 - excelent, 2 - good, 3 - fair, 4 - poor, 5 - very poor
	ret = i2c_read_reg(&dev_sensor, 0x21, buff, 5);		
	printf("ENS160 AQI: %d\n", buff[0]);

	// get Total Volatile Organic Compounds (TVOC)
	// value range: 0 - 65000 ppb	
	uint16_t tvoc = (buff[2] << 8) | buff[1];
	printf("ENS160 TVOC: %d\n", tvoc);

	// get Equivalent CO2 (eCO2)
	// value range: 400 - 65535 ppm
	// level Excelent: 400 - 600 ppm, Good: 600 - 800 ppm, Fair: 800 - 1000 ppm, Poor: 1000 - 1500 ppm, Unhealthy: >1500	
	uint16_t eco2 = (buff[4] << 8) | buff[3];
	printf("ENS160 eCO2: %d\n", eco2);
}

void test_sgp30() {
    dev_sensor.addr = 0x58;

    if (i2c_ping(dev_sensor.addr) != I2C_OK) {
		printf("SGP30 not found\n");
		return;
	}

	ret = i2c_write_raw(&dev_sensor, (uint8_t[]){0x36, 0x82}, 2);
	printf("Error0: %d\n", ret);					// Initialize SGP30
	ret = i2c_read_raw(&dev_sensor, buff, 3);		// Read ID
	printf("Error1: %d\n", ret);
	uint32_t id = (buff[0] << 16) | (buff[1] << 8) | buff[2];		// Get ID	
	printf("SGP30 ID: %04X\n", id);

	ret = i2c_write_raw(&dev_sensor, (uint8_t[]){0x20, 0x03}, 2);
	printf("Error2: %d\n", ret);		// Initialize SGP30	

	ret = i2c_write_raw(&dev_sensor, (uint8_t[]){0x20, 0x08}, 2);
	printf("Error0: %d\n", ret);		// Start measurement

	Delay_Ms(100);		// Wait for measurement
	ret = i2c_read_raw(&dev_sensor, buff, 6);		// Read sensor data
	printf("Error1: %d\n", ret);
	uint16_t co2 = (buff[0] << 8) | buff[1];		// CO2 in ppm
	uint16_t tvoc = (buff[3] << 8) | buff[4];		// TVOC in ppb

	ret = i2c_write_raw(&dev_sensor, (uint8_t[]){0x20, 0x50}, 2);
	printf("Error2: %d\n", ret);		// Set baseline

	Delay_Ms(100);		// Wait for baseline to be
	ret = i2c_read_raw(&dev_sensor, buff, 6);		// Read baseline data
	printf("Error3: %d\n", ret);
	uint16_t h2 = (buff[0] << 8) | buff[1];			// H2 baseline
	uint16_t ethanol = (buff[3] << 8) | buff[4];	// Ethanol baseline

	printf("SGP30 CO2: %d ppm, TVOC: %d ppb\n", co2, tvoc);
	printf("SGP30 H2: %d, Ethanol: %d\n", h2, ethanol);
}

void test_scd40() {
    dev_sensor.addr = 0x62;

    if (i2c_ping(dev_sensor.addr) != I2C_OK) {
		printf("SCD40 not found\n");
		return;
	}

	ret = i2c_write_raw(&dev_sensor, (uint8_t[]){0x36, 0xF6}, 2); 	// wake up
	printf("Error0: %d\n", ret);

	ret = i2c_write_raw(&dev_sensor, (uint8_t[]){0x21, 0xB1}, 2);	// periodic measurement
	printf("Error0: %d\n", ret);

	ret = i2c_write_raw(&dev_sensor, (uint8_t[]){0xEC, 0x05}, 2);	// read measurement
	printf("Error1: %d\n", ret);
	
	ret = i2c_read_raw(&dev_sensor, buff, 8);		// Read sensor data
	printf("Error2: %d\n", ret);

	uint16_t co2 = (buff[0] << 8) | buff[1];		// CO2 in ppm
	uint16_t temp_raw = (buff[3] << 8) | buff[4];	
	uint16_t temp = (temp_raw * 175) >> 16;			// >> 16 is equivalent to / 65536
	uint16_t hum_raw = (buff[6] << 8) | buff[6];
	uint16_t hum = (hum_raw * 100) >> 16;			// >> 16 is equivalent to / 65536

	printf("SCD40 CO2: %d ppm, Temp: %d, Hum: %d\n", co2, temp, hum);
}

void test_mpu6050() {
    dev_sensor.addr = 0x68;

    if (i2c_ping(dev_sensor.addr) != I2C_OK) {
		printf("MPU6050 not found\n");
		return;
	}
    
	ret = i2c_write_reg(&dev_sensor, 0x6B, (uint8_t[]){0x00}, 1);	// Reset MPU6050

	ret = i2c_read_reg(&dev_sensor, 0x3B, buff, 6);
	uint16_t acc_x = (buff[0] << 8) | buff[1];
	uint16_t acc_y = (buff[2] << 8) | buff[3];
	uint16_t acc_z = (buff[4] << 8) | buff[5];
	printf("MPU6050 acc_x: %d, acc_y: %d, acc_z: %d\n", acc_x, acc_y, acc_z);

	ret = i2c_read_reg(&dev_sensor, 0x43, buff, 6);
	uint16_t gyro_x = (buff[0] << 8) | buff[1];
	uint16_t gyro_y = (buff[2] << 8) | buff[3];
	uint16_t gyro_z = (buff[4] << 8) | buff[5];
	printf("MPU6050 gyro_x: %d, gyro_y: %d, gyro_z: %d\n", gyro_x, gyro_y, gyro_z);
}

void test_adxl345() {
    dev_sensor.addr = 0x53;

    if (i2c_ping(dev_sensor.addr) != I2C_OK) {
		printf("ADXL345 not found\n");
		return;
	}

	ret = i2c_write_reg(&dev_sensor, 0x2D, (uint8_t[]){0x08}, 1);	// Set power mode to measurement
	printf("Error0: %d\n", ret);

	ret = i2c_read_reg(&dev_sensor, 0x32, buff, 6);
	uint16_t x = (buff[1] << 8) | buff[0];
	uint16_t y = (buff[3] << 8) | buff[2];
	uint16_t z = (buff[5] << 8) | buff[4];
	printf("ADXL345 x: %d, y: %d, z: %d\n", x, y, z);
}

void test_ina3221() {
    dev_sensor.addr = 0x42;

    if (i2c_ping(dev_sensor.addr) != I2C_OK) {
		printf("INA3221 not found\n");
		return;
	}

	ret = i2c_read_reg(&dev_sensor, 0x00, buff, 2);
	printf("Error0: %d\n", ret);
	printf("INA3221: %02X %02X\n", buff[0], buff[1]);

	// ret = i2c_write_reg(&dev_sensor, 0x00, (uint8_t[]){0xF1, 0x27}, 2); // Reset + 128x avg
	// printf("Config Error: %d\n", ret);
	// Delay_Ms(100);

	ret = i2c_write_raw(&dev_sensor, (uint8_t[]){0x00, 0xF1, 0x26}, 3); // Reset + 128x avg
	printf("Error: %d\n", ret);

	Delay_Ms(100);
	ret = i2c_read_reg(&dev_sensor, 0x00, buff, 2);
	printf("Error0: %d\n", ret);
	printf("INA3221: %02X %02X\n", buff[0], buff[1]);

	// // Read all 6 registers (shunt1, bus1, shunt2, bus2, shunt3, bus3)
	// ret = i2c_read_reg(&dev_sensor, 0x01, buff, 12); // Reads 0x01-0x0C
	// printf("Read Error: %d\n", ret);

	// // Extract raw values (signed!)
	// int16_t ch0_shunt_raw = (buff[0] << 8) | buff[1];   // 0x01-0x02
	// int16_t ch0_bus_raw   = (buff[2] << 8) | buff[3];   // 0x03-0x04 (NOTE: 0x02 is skipped!)
	// int16_t ch1_shunt_raw = (buff[4] << 8) | buff[5];   // 0x05-0x06
	// int16_t ch1_bus_raw   = (buff[6] << 8) | buff[7];   // 0x07-0x08
	// int16_t ch2_shunt_raw = (buff[8] << 8) | buff[9];   // 0x09-0x0A
	// int16_t ch2_bus_raw   = (buff[10] << 8) | buff[11]; // 0x0B-0x0C

	// // Convert to real units
	// int32_t ch0_shunt = ch0_shunt_raw * 40;      		// µV (40µV/LSB)
	// int32_t ch0_bus   = (ch0_bus_raw >> 3) * 8000; 		// µV (8mV/LSB)
	// int32_t ch1_shunt = ch1_shunt_raw * 40;     		// µV
	// int32_t ch1_bus   = (ch1_bus_raw >> 3) * 8000; 		// µV
	// int32_t ch2_shunt = ch2_shunt_raw * 40;      		// µV
	// int32_t ch2_bus   = (ch2_bus_raw >> 3) * 8000; 		// µV

	// // Print results
	// printf("Ch0: Bus=%ld uV, Shunt=%ld uV\n", ch0_bus, ch0_shunt);
	// printf("Ch1: Bus=%ld uV, Shunt=%ld uV\n", ch1_bus, ch1_shunt);
	// printf("Ch2: Bus=%ld uV, Shunt=%ld uV\n", ch2_bus, ch2_shunt);
}

void test_as5600() {
    dev_sensor.addr = 0x36;

    if (i2c_ping(dev_sensor.addr) != I2C_OK) {
		printf("AS5600 not found\n");
		return;
	}

	ret = i2c_read_reg(&dev_sensor, 0x0C, buff, 4);
	printf("Error0: %d\n", ret);

	uint16_t angle_raw = (buff[0] << 8) | buff[1];
	uint16_t angle_value = (buff[2] << 8) | buff[3];
	printf("AS5600 angle_raw: %d, angle_value: %d\n", angle_raw, angle_value);
}

void test_max30102() {
    dev_sensor.addr = 0x57;

    if (i2c_ping(dev_sensor.addr) != I2C_OK) {
		printf("MAX30102 not found\n");
		return;
	}

	uint8_t part_id;
	ret = i2c_read_reg(&dev_sensor, 0xFF, &part_id, 1);
	printf("MAX30102: %02X\n", part_id);		// expect 0x15

	// heart rate + SP02 mode
	ret = i2c_write_reg(&dev_sensor, 0x09, (uint8_t[]){0x03}, 1);

	// set RED LED current to 36mA
	ret = i2c_write_reg(&dev_sensor, 0x0C, (uint8_t[]){0x24}, 1);

	// set IR LED current to 36mA
	ret = i2c_write_reg(&dev_sensor, 0x0D, (uint8_t[]){0x24}, 1);

	// set sample average to 4
	ret = i2c_write_reg(&dev_sensor, 0x08, (uint8_t[]){0x40}, 1);

	Delay_Ms(100);
	ret = i2c_read_reg(&dev_sensor, 0x07, buff, 6);
	uint32_t red_led = (buff[0] << 16) | (buff[1] << 8) | buff[2];
	uint32_t ir_led = (buff[3] << 16) | (buff[4] << 8) | buff[5];
	printf("MAX30102: %lu, %lu\n", red_led, ir_led);
}

void test_amg88xx() {
    dev_sensor.addr = 0x69;

    if (i2c_ping(dev_sensor.addr) != I2C_OK) {
		printf("AMG88xx not found\n");
		return;
	}

	uint16_t pixels[64];
	ret = i2c_read_reg(&dev_sensor, 0x80, buff, 128);
	printf("Error0: %d\n", ret);

	for (int i=0; i<64; i++) {
		uint16_t raw = (buff[i*2] << 8) | buff[i*2+1];
		pixels[i] = raw * .25;
	}

	// print out each line
	for (int i=0; i<8; i++) {
		printf("%lu, %lu, %lu, %lu, %lu, %lu, %lu, %lu\n", 
		pixels[i*8], pixels[i*8+1], pixels[i*8+2], pixels[i*8+3], pixels[i*8+4], pixels[i*8+5], pixels[i*8+6], pixels[i*8+7]);
	}
}

void test_bh1750() {
    dev_sensor.addr = 0x23;

	if (i2c_ping(dev_sensor.addr) != I2C_OK) {
		printf("BH1750 not found\n");
		return;
	}

	ret = i2c_write_raw(&dev_sensor, (uint8_t[]){0x01}, 1);

	// One-time H-resolution mode
	ret = i2c_read_reg(&dev_sensor, 0x20, buff, 2);

	uint32_t lux_raw = (buff[0] << 8) | buff[1];
	uint32_t lux = (lux_raw / 1.2);
	printf("BH1750: %lu lx\n\n", lux);
}

void test_sht3x() {
    dev_sensor.addr = 0x44;

	if (i2c_ping(dev_sensor.addr) != I2C_OK) {
		printf("SHT3X not found\n");
		return;
	}

	// Soft reset
	ret = i2c_write_reg(&dev_sensor, 0x30, (uint8_t[]){0xA2}, 1);
	// ret = i2c_write_raw(&dev_sensor, (uint8_t[]){0x30, 0xA2}, 2);
	Delay_Ms(20);	//! REQUIRED
	
	// High repeatability, 1 update per second
	ret = i2c_write_raw(&dev_sensor, (uint8_t[]){0x21, 0x30}, 2);
	Delay_Ms(20);	//! REQUIRED

	ret = i2c_read_raw(&dev_sensor, buff, 6);
	uint16_t temp_raw = (buff[0] << 8) | buff[1];
	uint16_t hum_raw = (buff[3] << 8) | buff[4];
	uint16_t temp = (175 * temp_raw) >> 16;			// >> 16 is equivalent to / 65536
	uint16_t hum = (100 * hum_raw) >> 16;			// >> 16 is equivalent to / 65536
	printf("SHT3X temp: %d, hum: %d\n\n", temp, hum);
}

void test_apds9960() {
    dev_sensor.addr = 0x39;

	if (i2c_ping(dev_sensor.addr) != I2C_OK) {
		printf("APDS9960 not found\n");
		return;
	}

	uint8_t config[] = {
		0x80,           // ENABLE
		0x0F,           // Power ON, Proximity enable, ALS enable, Wait enable
		0x90,           // CONFIG2
		0x01,           // Proximity gain control (4x)
		0x8F, 0x20,     // Proximity pulse count (8 pulses)
		0x8E, 0x87      // Proximity pulse length (16us)
	};
	ret = i2c_write_raw(&dev_sensor, config, sizeof(config));
	Delay_Ms(50);  // Wait for sensor to initialize

	uint8_t proximity;
	ret = i2c_read_reg(&dev_sensor, 0x9C, &proximity, 1);		// Read proximity register
	printf("APDS9960 Proximity: %d\n", proximity);

	ret = i2c_read_reg(&dev_sensor, 0x94, buff, 8);			// Read lux registers
	uint16_t clear = (buff[1] << 8) | buff[0];
	uint16_t red = (buff[3] << 8) | buff[2];
	uint16_t green = (buff[5] << 8) | buff[4];
	uint16_t blue = (buff[7] << 8) | buff[6];
	printf("APDS9960 clear: %d, R: %d, G: %d, B: %d\n\n", clear, red, green, blue);
}

void test_ina219() {
    dev_sensor.addr = 0x40;

	if (i2c_ping(dev_sensor.addr) != I2C_OK) {
		printf("INA219 not found\n");
		return;
	}

	uint16_t powerLSB = 2;    	// 2uW per bit
	ret = i2c_write_reg(&dev_sensor, 0x00, (uint8_t[]){0x39, 0x9F}, 2);		// Configure INA219 32V 1A Range

	// 2. Set calibration for 1A range (assuming 0.1Ω shunt)
	uint16_t cal = 4096;  // 0.04096 / (0.0001 * 0.1)
	uint8_t cal_bytes[2] = {cal >> 8, cal & 0xFF};
	i2c_write_reg(&dev_sensor, 0x05, cal_bytes, 2);

	ret = i2c_read_reg(&dev_sensor, 0x01, buff, 2);		// Read shunt voltage
	uint16_t shunt_raw = (buff[1] << 8) | buff[0];
	uint16_t shunt = shunt_raw / 100;					// in mV

	ret = i2c_read_reg(&dev_sensor, 0x02, buff, 2);		// Read bus voltage
	uint16_t bus_raw = (buff[1] << 8) | buff[0];
	uint16_t bus = (shunt_raw >> 3) * 4;				// in mV

	ret = i2c_read_reg(&dev_sensor, 0x03, buff, 2);		// Read power
	uint16_t power_raw = (buff[1] << 8) | buff[0];
	uint16_t power = power_raw * powerLSB;				// in uW

	ret = i2c_read_reg(&dev_sensor, 0x04, buff, 2);		// Read current
	uint16_t current_raw = (buff[1] << 8) | buff[0];
	uint16_t current = current_raw;						// in mA

	printf("INA219 Shunt: %duV, Bus: %duV, Current: %duA\n\n", shunt, bus, current);
}


uint8_t decimal_to_bcd(uint8_t val) { return ((val/10)*16) + (val%10); }
uint8_t bcd_to_decimal(uint8_t val) { return ((val/16)*10) + (val%16); }

void test_ds3231() {
    dev_sensor.addr = 0x68;

    if (i2c_ping(dev_sensor.addr) != I2C_OK) {
		printf("DS3231 not found\n");
		return;
	}

	ret = i2c_write_reg(&dev_sensor, 0x00, (uint8_t[]){
		decimal_to_bcd(9),		// seconds
		decimal_to_bcd(58),		// minutes
		decimal_to_bcd(11),		// hours
		decimal_to_bcd(1),		// day of the week
		decimal_to_bcd(15),		// date
		decimal_to_bcd(7),		// month
		decimal_to_bcd(25),		// year
	}, 7);	// Reset ds3231
	printf("Error0: %d\n", ret);

	ret = i2c_read_reg(&dev_sensor, 0x00, buff, 7);
	printf("Error1: %d\n", ret);

	// print date and time
	printf("%d/%d/%lu %d:%d:%d\n", 
		bcd_to_decimal(buff[4]), bcd_to_decimal(buff[5]), bcd_to_decimal(buff[6]) + 2000,
		bcd_to_decimal(buff[2]), bcd_to_decimal(buff[1]), bcd_to_decimal(buff[0]));
}

void test_at24c() {
    dev_sensor.addr = 0x57;

    if (i2c_ping(dev_sensor.addr) != I2C_OK) {
		printf("AT24C not found\n");
		return;
	}

	ret = i2c_write_raw(&dev_sensor, (uint8_t[]){ 0x00, 0x11, 0xDD }, 3);
	printf("Error0: %d\n", ret);

	Delay_Ms(10);
	ret = i2c_write_raw(&dev_sensor, (uint8_t[]){ 0x00, 0x11 }, 2);
	ret = i2c_read_raw(&dev_sensor, buff, 1);
	printf("Error1: %d\n", ret);
	printf("0x%02X\n", buff[0]);
}

void test_v003Slave() {
    dev_sensor.addr = 0x77;

	if (i2c_ping(dev_sensor.addr) != I2C_OK) {
		printf("v003Slave not found\n");
		return;
	}

	ret = i2c_read_reg(&dev_sensor, 0x00, buff, 3);
	printf("v003Slave: %02X, %02X, %02X\n", buff[0], buff[1], buff[2]);

	ret = i2c_write_reg(&dev_sensor, 0x01, (uint8_t[]){0x11, 0x22}, 2);

	ret = i2c_read_reg(&dev_sensor, 0x00, buff, 3);
	printf("v003Slave: %02X, %02X, %02X\n", buff[0], buff[1], buff[2]);
}

void modI2C_display(const char *str, uint8_t line) {
	//! validate device before print
	if (i2c_ping(0x3C) != I2C_OK) return;
	ssd1306_print_str_at(str, line, 0);
}

char str_output[SSD1306_STR_SIZE];

void i2c_scan_callback(const uint8_t addr) {
	if (addr == 0x00 || addr == 0x7F) return; // Skip reserved addresses
	
	static int line = 1;
	sprintf(str_output, "I2C: 0x%02X", addr);
	printf("%s\n", str_output);
	modI2C_display(str_output, line++);
}

void i2c_device_tests() {
	// test_htu21();				//! NOT WORKING
	// test_aht21();
	// test_bmp280();
	// test_hdc1080();
	// test_si7021();
	// test_max44009();
	// test_ap3216();
	// test_vl53l0x();			    //! NOT WORKING
	// test_ens160();
	// test_sgp30();
	// test_scd40();
	// test_mpu6050();
	// test_adxl345();
	// test_ds3231();
	// test_at24c();
	// test_ina3221();			    //! NOT WORKING
	// test_as5600();
	// test_max30102();
	// test_amg88xx();
	// test_apds9960();
	test_ina219();
    test_bh1750();
	test_sht3x();
}


int main() {
    SystemInit();
    systick_init();			//! required for millis()

    funGpioInitAll();
    Delay_Ms(100);

	if(i2c_init(&dev_ssd1306) != I2C_OK) {
		printf("Failed to init I2C\n");
	} 
	else {
		if (i2c_ping(0x3C) == I2C_OK) {
			ssd1306_setup();

			sprintf(str_output, "Hello Bee2!");
			ssd1306_print_str_at(str_output, 0, 0);
		}

		// Scan the I2C Bus, prints any devices that respond
		printf("----Scanning I2C Bus for Devices---\n");
		i2c_scan(i2c_scan_callback);
		printf("----Done Scanning----\n\n");
	}

    while(1) {
        Delay_Ms(1000);

        i2c_device_tests();
    }
}