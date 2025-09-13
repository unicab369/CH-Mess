#include "ch32fun.h"

#include "../Mess-libs/i2c/lib/lib_i2c.h"
#include "../Mess-libs/i2c/ssd1306/fun_ssd1306.h"

#ifndef PACKED
#define PACKED __attribute__((packed))
#endif

#define PRINT_BUFF_SIZE 5

char str_output[SSD1306_STR_SIZE];

i2c_device_t dev_ssd1306 = {
	.clkr = I2C_CLK_100KHZ,
	.type = I2C_ADDR_7BIT,
	.addr = 0x3C,				// Default address for SSD1306
	.regb = 1,
};

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

	// Configure INA219 32V 1A Range
	i2c_err_t ret = i2c_write_reg(&dev_sensor, 0x00, config, 2);
}

void i2c_ina219_reading(int16_t *shunt, int16_t *bus, int16_t *power, int16_t *current) {
	dev_sensor.addr = 0x40;

	if (i2c_ping(dev_sensor.addr) != I2C_OK) {
		printf("INA219 not found\n");
		return;
	}

	i2c_err_t ret;
	uint8_t buff[2];
	uint16_t raw_value;

	uint16_t powerLSB = 2;    	// 2uW per bit

	// 2. Set calibration for 1A range (assuming 0.1Ω shunt)
	uint16_t cal = 4096;  // 0.04096 / (0.0001 * 0.1)
	uint8_t cal_bytes[2] = {cal >> 8, cal & 0xFF};
	i2c_write_reg(&dev_sensor, 0x05, cal_bytes, 2);

	// Read shunt voltage in uV
	ret = i2c_read_reg(&dev_sensor, 0x01, buff, 2);		
	raw_value = (buff[0] << 8) | buff[1];
	*shunt = raw_value * 10;

	// Read bus voltage in mV
	ret = i2c_read_reg(&dev_sensor, 0x02, buff, 2);
	raw_value = (buff[0] << 8) | buff[1];
	*bus = (raw_value >> 3) * 4;

	// Read power in uW
	ret = i2c_read_reg(&dev_sensor, 0x03, buff, 2);		
	raw_value = (buff[0] << 8) | buff[1];
	*power = raw_value * powerLSB;

	// Read current in mA
	ret = i2c_read_reg(&dev_sensor, 0x04, buff, 2);
	raw_value = (buff[0] << 8) | buff[1];
	*current = raw_value;

	printf("shunt: %ld uV, bus: %ld mV, P: %ld mW, I: %ld mA\n",
			*shunt, *bus, *power,*current);
}

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


void modI2C_display(const char *str, uint8_t line) {
	//! validate device before print
	if (i2c_ping(0x3C) != I2C_OK) return;
	ssd1306_print_str_at(str, line, 0);
}

void i2c_scan_callback(const uint8_t addr) {
	if (addr == 0x00 || addr == 0x7F) return; // Skip reserved addresses
	
	static int line = 1;
	sprintf(str_output, "I2C: 0x%02X", addr);
	printf("%s\n", str_output);
	modI2C_display(str_output, line++);
}

void modI2C_setup(uint16_t counter) {
	if(i2c_init(&dev_ssd1306) != I2C_OK) {
		printf("Failed to init I2C\n");
	} 
	else {
		if (i2c_ping(0x3C) == I2C_OK) {
			ssd1306_setup();

			sprintf(str_output, "Hello Bee %ld", counter);
			ssd1306_print_str_at(str_output, 0, 0);
		}

		// Scan the I2C Bus, prints any devices that respond
		printf("----Scanning I2C Bus for Devices---\n");
		i2c_scan(i2c_scan_callback);
		printf("----Done Scanning----\n\n");
	}
}


typedef struct PACKED {
	char str[SSD1306_STR_SIZE];
	uint8_t line_num;		// line_num = 0 means empty
} PrintBuff_t;


uint32_t lock_printTime = 0;
uint8_t printBuff_idx = 0;
PrintBuff_t printBuff[PRINT_BUFF_SIZE] = { 0 };
uint8_t flag_printBuff_data = 1;					// set to 1 to allow initial print

void mngI2c_load_printBuff(const char *str, uint8_t line_idx) {
	flag_printBuff_data = 1;
	PrintBuff_t *buff = &printBuff[printBuff_idx];
	buff->line_num = line_idx + 1;
	strncpy(buff->str, str, SSD1306_STR_SIZE);
	// buff->str[24] = '\0';
	printBuff_idx = (printBuff_idx + 1) % PRINT_BUFF_SIZE;
}

uint32_t line6_preserve_time = 0;

void mngI2c_load_joystick(uint32_t time, uint16_t x, uint16_t y) {
	// keep line6_preserve_time shown for at least 1 second
	if (time - line6_preserve_time < 1000) return;

	sprintf(str_output, "jx: %d, jy: %d", x, y);
	mngI2c_load_printBuff(str_output, 6);
}

void mngI2c_load_buttonState(uint32_t time, uint8_t state) {
	line6_preserve_time = time;

	sprintf(str_output, "button: %d", state);
	mngI2c_load_printBuff(str_output, 6);
}

void mngI2c_load_encoder(uint32_t time, uint8_t pos, uint8_t dir) {
	line6_preserve_time = time;

	sprintf(str_output, "pos: %d, dir: %s", pos, dir ? "CW" : "CCW");
	mngI2c_load_printBuff(str_output, 6);
}

char loading_char[4] = "\\";

void cycle_loading_char() {
    // Since we're only dealing with single characters, we can just check the first byte.
    // This is faster and safer than strcmp/strcpy.
    switch(loading_char[0]) {
        case '\\':	strcpy(loading_char, "|"); break;
        case '|':	strcpy(loading_char, "/"); break;
        case '/':	strcpy(loading_char, "-"); break;
        case '-':	strcpy(loading_char, "\\"); break;
    }
}

void mngI2c_loadCounter(uint32_t counter, uint32_t runTime) {
	// uint16_t lux;
	// i2c_bh1750_reading(&lux);

	// uint16_t temp, hum;
	// i2c_sht3x_reading(&temp, &hum);

	uint16_t shunt, bus, power, current;
	i2c_ina219_reading(&shunt, &bus, &power, &current);


	sprintf(str_output, "%s cyc/s %lu ~ %lums", loading_char, counter, runTime);
	mngI2c_load_printBuff(str_output, 7);
	cycle_loading_char();
}

void mngI2c_printBuff_task() {
	if (!flag_printBuff_data) return;

	for (int i = 0; i < PRINT_BUFF_SIZE; i++) {
		if (printBuff[i].line_num == 0) continue;
		modI2C_display(printBuff[i].str, printBuff[i].line_num-1);
	}

	//! Clear printBuff
	memset(printBuff, 0, sizeof(printBuff));
	flag_printBuff_data = 0;
}