#include "ch32fun.h"

#include "../Mess-libs/i2c/lib/lib_i2c.h"

i2c_device_t dev_sensor = {
	.clkr = I2C_CLK_100KHZ,
	.type = I2C_ADDR_7BIT,
	.addr = 0x00,				// Placeholder, set before use
	.regb = 1,
};

void i2c_bh1750_reading(uint16_t *lux) {
    dev_sensor.addr = 0x23;

	if (i2c_ping(dev_sensor.addr) != I2C_OK) {
		printf("BH1750 not found\n");
		return;
	}

	i2c_err_t ret;
	uint8_t buff[8];

	// CONTINOUS_HI_RES_MODE = 0x10		- 1 lux resolution 120ms
	// CONTINOUS_HI_RES_MODE2 = 0x11	- .5 lux resolution 120ms
	// CONTINOUS_LOW_RES_MODE = 0x13	- 4 lux resolution 16ms
	// ONE_TIME_HI_RES_MODE = 0x20		- 1 lux resolution 120ms
	// ONE_TIME_HI_RES_MODE2 = 0x21		- .5 lux resolution 120ms
	// ONE_TIME_LOW_RES_MODE = 0x23		- 4 lux resolution 16ms
	ret = i2c_read_reg(&dev_sensor, 0x13, buff, 2);

	uint32_t lux_raw = (buff[0] << 8) | buff[1];
	*lux = (lux_raw / 1.2);
	printf("BH1750: %lu lx\n\n", *lux);
}

uint32_t i2c_sht3x_reading(uint16_t *temperature, uint16_t *humidity) {
	dev_sensor.addr = 0x44;

	if (i2c_ping(dev_sensor.addr) != I2C_OK) {
		printf("SHT3X not found\n");
		return;
	}

	// Soft reset
	i2c_err_t ret;
	uint8_t buff[8];

	// Soft Reset
	ret = i2c_write_raw(&dev_sensor, (uint8_t[]){0x30, 0xA2}, 2);
	Delay_Ms(1);	//! REQUIRED
	
	//# SINGLE_SHOT = MSB: 0x24 
	// LSB: 0x00 (High Repeatability) | 0x0B (Med Rep) | 0x16 (Low Rep)
	//# SINGLE_SHOT_CLOCK_STRETCH = MSB: 0x2C
	// LSB: 0x06 (High Repeatability) | 0x0D (Med Rep) | 0x10 (Low Rep)

	//# PERIODIC_MEASUREMENT (0.5 meas/sec) = MSB: 0x20
	// 0x32 (High Repeatability) | 0x24 (Med Rep) | 0x2F (Low Rep)
	//# PERIODIC_MEASUREMENT (1 meas/sec) = MSB: 0x21
	// 0x30 (High Repeatability) | 0x26 (Med Rep) | 0x2D (Low Rep)
	//# PERIODIC_MEASUREMENT (2 meas/sec) = MSB: 0x22
	// 0x36 (High Repeatability) | 0x20 (Med Rep) | 0x2B (Low Rep)
	//# PERIODIC_MEASUREMENT (4 meas/sec) = MSB: 0x23
	// 0x34 (High Repeatability) | 0x22 (Med Rep) | 0x29 (Low Rep)
	//# PERIODIC_MEASUREMENT (10 meas/sec) = MSB: 0x27
	// 0x37 (High Repeatability) | 0x21 (Med Rep) | 0x2A (Low Rep)

	ret = i2c_write_raw(&dev_sensor, (uint8_t[]){0x24, 0x00}, 2);
	Delay_Ms(12);	//! REQUIRED

	ret = i2c_read_raw(&dev_sensor, buff, 6);
	uint16_t temp_raw = (buff[0] << 8) | buff[1];
	uint16_t hum_raw = (buff[3] << 8) | buff[4];
	*temperature = ((175 * temp_raw) >> 16) - 45;				// >> 16 is equivalent to / 65536
	*humidity = (100 * hum_raw) >> 16;							// >> 16 is equivalent to / 65536
	printf("SHT3X temp: %d, hum: %d\n\n", *temperature, *humidity);
}

uint16_t ina219_current_divider_mA;
uint16_t ina219_power_multiplier_uW;

void i2c_ina219_setup() {
	dev_sensor.addr = 0x40;

	if (i2c_ping(dev_sensor.addr) != I2C_OK) {
		printf("INA219 not found\n");
		return;
	}

	// 32V = 0x2000, 16V = 0x0000
	uint16_t BUS_VOLTAGE_RANGE = 0x2000;
	
	// Gain/1 40mV 		= 0x0000
	// Gain/2 80mV	 	= 0x0800
	// Gain/4 160mV		= 0x1000
	// Gain/8 320mV		= 0x1800
	uint16_t GAIN_AMPLIFIER_RANGE = 0x1800;

	// 9bits				= 0x0000	84us
	// 10bits 				= 0x0080	148us 
	// 11bits 				= 0x0100	276us 
	// 12bits 				= 0x0180	532us 
	// 12bits 2 samples 	= 0x0480	1.06ms
	// 12bits 4 samples 	= 0x0500	2.13ms
	// 12bits 8 samples 	= 0x0580	4.26ms	
	// 12bits 16 samples 	= 0x0600	8.51ms	
	// 12bits 32 samples 	= 0x0680	17.02ms	
	// 12bits 64 samples 	= 0x0700	34.05ms	 
	// 12bits 128 samples 	= 0x0780	68.10ms
	uint16_t BUS_RESOLUTION_AVERAGE = 0x0180;

	// Shunt Resolution = Bus_Resolution >> 1
	uint16_t SHUNT_RESOLUTION_AVERAGE = 0x0018;

	// 0x00 = Power Down
	// 0x01 = Shunt Voltage, triggered
	// 0x02 = Bus Voltage, triggered
	// 0x03 = Shunt and Bus Voltage, triggered
	// 0x04 = ADC Off
	// 0x05 = Shunt Voltage, continuous
	// 0x06 = Bus Voltage, continuous
	// 0x07 = Shunt and Bus Voltage, continuous
	uint8_t DEVICE_MODE = 0x07;

	uint16_t config = BUS_VOLTAGE_RANGE | GAIN_AMPLIFIER_RANGE |
					BUS_RESOLUTION_AVERAGE | SHUNT_RESOLUTION_AVERAGE |
					DEVICE_MODE;
    printf("INA219 config: 0x%04X\n", config);
    uint8_t config_bytes[2] = {config >> 8, config & 0xFF};

    i2c_err_t ret;
    ret = i2c_write_reg(&dev_sensor, 0x00, config_bytes, 2);

    //# See datasheet for calculations
    // ina219_calibration_value = 0.04096 / (Current_LSB * Rshunt)
    // Current_LSB = Max_Current / 2^15
    // Rshunt = 0.1 ohm

    uint32_t calibration_value;

    switch (GAIN_AMPLIFIER_RANGE) {
        case 0x0000:
            calibration_value = 20480;
            ina219_current_divider_mA = 50;
            ina219_power_multiplier_uW = 400;
            break;
        case 0x0800:
            // current_LSB = 1 / 2^15 = 0.0000305 (30.5uA per bit)
            // current_LSB = roundup to 40uA per bit = 0.00004 (40uA per bit)
            // Calibration_value = 0.04096 / (0.00004 * 0.1) = 10240
            // ina219_calibration_value = 10240;
            calibration_value = 10240;

            // Current_LSB = 40uA per bit (1000mA/40 = 25)
            ina219_current_divider_mA = 25;

            // Power_LSB = 20 * Current_LSB = 0.0008 (0.8mW per bit)
            ina219_power_multiplier_uW = 800;
            break;
        case 0x1000:
            calibration_value = 8192;
            ina219_current_divider_mA = 20;
            ina219_power_multiplier_uW = 1000;
            break;
        case 0x1800:
            calibration_value = 4096;
            ina219_current_divider_mA = 10;
            ina219_power_multiplier_uW = 2000;
            break;
    }

    uint8_t cal_bytes[2] = {calibration_value >> 8, calibration_value & 0xFF};
    ret = i2c_write_reg(&dev_sensor, 0x05, cal_bytes, 2);
}

void i2c_ina219_reading(
    int16_t *shunt_uV, int16_t *bus_mV, int16_t *power_mW, int16_t *current_mA
) {
	dev_sensor.addr = 0x40;

	if (i2c_ping(dev_sensor.addr) != I2C_OK) {
		printf("INA219 not found\n");
		return;
	}

	i2c_err_t ret;
	uint8_t buff[2];
	int16_t raw_value;

	//# Read shunt voltage in uV
	ret = i2c_read_reg(&dev_sensor, 0x01, buff, 2);		
	raw_value = (buff[0] << 8) | buff[1];
	*shunt_uV = raw_value * 10;

	//# Read bus voltage in mV
	ret = i2c_read_reg(&dev_sensor, 0x02, buff, 2);
	raw_value = (buff[0] << 8) | buff[1];
	*bus_mV = (raw_value >> 3) * 4;

	//# Read power in uW
	ret = i2c_read_reg(&dev_sensor, 0x03, buff, 2);		
	raw_value = (buff[0] << 8) | buff[1];
	*power_mW = raw_value * ina219_power_multiplier_uW / 1000;

	//# Read current in mA
	ret = i2c_read_reg(&dev_sensor, 0x04, buff, 2);
	raw_value = (buff[0] << 8) | buff[1];
	*current_mA = raw_value / ina219_current_divider_mA;

	printf("shunt: %d uV, bus: %d mV, P: %d mW, I: %d mA\n",
			*shunt_uV, *bus_mV, *power_mW, *current_mA);
}
