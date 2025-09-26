#include "ch32fun.h"
#include <stdint.h>
#include <stdio.h>

#include "fonts/font_5x7.h"
#include "../../spi/lib/lib_spi.h"

int HC595_LATCH_PIN = -1;
u16 current_value = 0;

void print_bits(u32 val, u8 bit_count, u8 print_space) {
    for(int i = bit_count-1; i >= 0; i--) {
        printf("%d", (val >> i) & 1);
        if (print_space && i % 4 == 0 && i != 0) printf(" "); // Optional: add space every 4 bits
    }
    printf("\n");
}

void print_bits8(u8 val)    { print_bits(val, 8, 0); }
void print_bits16(u16 val)  { print_bits(val, 16, 0); }


u16 reverse_bit16(u16 val) {
    u16 result = 0;
    for (int i = 0; i < 16; i++) {
        result <<= 1;           // Shift result left
        result |= (val & 1);    // Add LSB of val
        val >>= 1;              // Shift val right
    }
    return result;
}


//! ####################################
//! SPI FUNCTIONS
//! ####################################


void HC595_RELEASE_LATCH() {
    if (HC595_LATCH_PIN != -1) funDigitalWrite(HC595_LATCH_PIN, 0);
}

void HC595_SET_LATCH() {
    if (HC595_LATCH_PIN != -1) funDigitalWrite(HC595_LATCH_PIN, 1);
}

void hc595_send8(u8 data) {
    HC595_RELEASE_LATCH();      // latch OFF
    current_value = data;
    SPI_transfer_8(data);
    HC595_SET_LATCH();          // latch ON
}

void hc595_send16(u16 data) {
    HC595_RELEASE_LATCH();      // latch OFF
    u8 msb = (data >> 8) & 0xFF;
    u8 lsb = data & 0xFF;
    SPI_transfer_8(msb);
    SPI_transfer_8(lsb);
    HC595_SET_LATCH();          // latch ON
}

// 74HC595 Datasheet: https://www.ti.com/lit/ds/symlink/sn74hc595.pdf

//# 74HC595 Pinout
// Q1  - 1     |    16 - Vcc
// Q2  - 2     |    15 - Q0
// Q3  - 3     |    14 - DATA
// Q4  - 4     |    13 - CE
// Q5  - 5     |    12 - CLK
// Q6  - 6     |    11 - LATCH
// Q7  - 7     |    10 - RST
// GND - 8     |    9  - Q7'

//! Assuming Data goes to Shift Register 1
//! and Shift Register 1 Serial Data output goes to Shift Register 2 Data input
//! Column 0 starts from the right side of the diagram bellow pin #1-#16

// 1088AS Top view
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% tab %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//! REG PINS:   Q0       Q1       (Q2)      Q3       (Q4)       (Q5)     Q6        Q7
//#  ROW/COL:   ROW2     ROW5     (COL4)    ROW7     (COL2)     (COL1)   ROW6      ROW4
//      PINS:   #8       #7       #6        #5       #4         #3       #2        #1
// ---------------------------------------------------------------------------------------
//      PINS:   #9       #10      #11       #12      #13        #14      #15       #16
//#  ROW/COL:   ROW0     (COL3)   (COL5)    ROW3     (COL0)     ROW1     (COL6)    (COL7)
//! REG PINS:   Q15      (Q14)    (Q13)     Q12      (Q11)      Q10      (Q9)      (Q8)
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

//! BIT7 of Shift Register 1 is PIN #1 Q7 (MSB)
//! BIT7 of Shift Register 2 is PIN #9 Q15 (MSB)
//! bit order: Q15-Q0 = REG2 PINS #9-#16 - REG1 PINs #1-#8

#define HC595_COL0_MASK         (1 << 11)   // 0b0000 1000 0000 0000
#define HC595_COL1_MASK         (1 << 5)    // 0b0000 0000 0010 0000
#define HC595_COL2_MASK         (1 << 4)    // 0b0000 0000 0001 0000
#define HC595_COL3_MASK         (1 << 14)   // 0b0100 0000 0000 0000
#define HC595_COL4_MASK         (1 << 2)    // 0b0000 0000 0000 0100
#define HC595_COL5_MASK         (1 << 13)   // 0b0010 0000 0000 0000
#define HC595_COL6_MASK         (1 << 9)    // 0b0000 0010 0000 0000
#define HC595_COL7_MASK         (1 << 8)    // 0b0000 0001 0000 0000

// Columns mask represents the bits of all of the columns
#define HC595_ALL_COLS_MASK     (HC595_COL0_MASK | HC595_COL1_MASK | \
                                HC595_COL2_MASK | HC595_COL3_MASK | \
                                HC595_COL4_MASK | HC595_COL5_MASK | \
                                HC595_COL6_MASK | HC595_COL7_MASK)

#define HC595_ROW0_MASK         (1 << 15)
#define HC595_ROW1_MASK         (1 << 10)
#define HC595_ROW2_MASK         (1 << 0)
#define HC595_ROW3_MASK         (1 << 12)
#define HC595_ROW4_MASK         (1 << 7)
#define HC595_ROW5_MASK         (1 << 1)
#define HC595_ROW6_MASK         (1 << 6)
#define HC595_ROW7_MASK         (1 << 3)

// Rows mask represents the bits of all of the rows
// Rows mask is reversed of the columns bits
#define HC595_ALL_ROWS_MASK     (HC595_ROW0_MASK | HC595_ROW1_MASK | \
                                HC595_ROW2_MASK | HC595_ROW3_MASK | \
                                HC595_ROW4_MASK | HC595_ROW5_MASK | \
                                HC595_ROW6_MASK | HC595_ROW7_MASK)

void hc595_setAll_ON() {
    // all rows OFF, all columns ON - simply send the columns mask
    hc595_send16(HC595_ALL_COLS_MASK);
}

void hc595_setAll_OFF() {
    hc595_send16(0);
}

const u16 HC595_COLS[] = { 
    HC595_COL0_MASK, HC595_COL1_MASK,
    HC595_COL2_MASK, HC595_COL3_MASK,
    HC595_COL4_MASK, HC595_COL5_MASK,
    HC595_COL6_MASK, HC595_COL7_MASK 
};

void hc595_setColumn_ON(u8 colIdx) {
    // All rows OFF, Only the selected column ON 
    // - simply send the selected column mask
    u16 col_value = HC595_COLS[colIdx];
    hc595_send16(col_value);
}

const u16 HC595_ROWS[] = { 
    HC595_ROW0_MASK, HC595_ROW1_MASK,
    HC595_ROW2_MASK, HC595_ROW3_MASK,
    HC595_ROW4_MASK, HC595_ROW5_MASK,
    HC595_ROW6_MASK, HC595_ROW7_MASK
};

void hc595_setRow_ON(u8 rowIdx) {
    // All columns ON, Only the selected row OFF
    u16 rows_value = HC595_ALL_ROWS_MASK & ~HC595_ROWS[rowIdx];
    hc595_send16(HC595_ALL_COLS_MASK | rows_value);
}

void hc595_setPosition_ON(u8 colIdx, u8 rowIdx) {
    // Only the selected row OFF, Only the selected column ON
    u16 rows_value = HC595_ALL_ROWS_MASK & ~HC595_ROWS[rowIdx];
    hc595_send16(HC595_COLS[colIdx] | rows_value);
}

void hc595_init(u8 latchPin) {
    if (latchPin != -1) {
        HC595_LATCH_PIN = latchPin;
        funPinMode(HC595_LATCH_PIN, GPIO_CFGLR_OUT_50Mhz_PP);
    }
}

void hc595_mask_column(u8 colIdx, u8 value) {
    // translate the value to associated rows
    u16 row_value = 0;

    for (int i = 7; i >= 0; i--) {
        u8 val = (value >> i) & 1;
        if (val) row_value |= HC595_ROWS[i];
    }

    // apply the rows mask
    row_value = HC595_ALL_ROWS_MASK & ~row_value;
    hc595_send16(HC595_COLS[colIdx] | row_value);
}

//! ####################################
//! OTHER FUNCTIONS
//! ####################################

u8 font_width = 5;
u8 font_height = 7;

void hc595_printChar(char c) {
    const char* glyph = &FONT_5x7[(c-32) * font_width];

    for (u8 x=0; x < font_width; x++) {
        u8 value = glyph[x];
        hc595_mask_column(x, value);
    }
}

void hc595_printStr(const char *str) {
    while (*str) {
        char c = *str++;
        hc595_printChar(c);
    }
}

//! ####################################
//! SHIFTOUT FUNCTIONS
//! ####################################

u8 HC595_CLOCK_PIN = -1;
u8 HC595_DATA_PIN = -1;

//! methods for shiftout
void hc595_setup_shiftOut(u8 clockPin, u8 dataPin) {
    if (clockPin != -1) {
        HC595_CLOCK_PIN = clockPin;
        funPinMode(HC595_CLOCK_PIN, GPIO_CFGLR_OUT_50Mhz_PP);
    }
    if (dataPin != -1) {
        HC595_DATA_PIN = dataPin;
        funPinMode(HC595_DATA_PIN, GPIO_CFGLR_OUT_50Mhz_PP);
    }
}

//! REQUIRES: clock and data pins
void hc595_shiftOut8(u8 val, u8 msbFirst) {
    HC595_RELEASE_LATCH();

	for (int i = 0; i < 8; i++) {
		if (msbFirst == 1) {
            funDigitalWrite(HC595_DATA_PIN, (val & 0x80) != 0); // Send MSB first
			val <<= 1; // Shift left
		} else {
			funDigitalWrite(HC595_DATA_PIN, val & 1); // Send LSB first
			val >>= 1; // Shift right
		}
		funDigitalWrite(HC595_CLOCK_PIN, 1); // Clock pulse
		funDigitalWrite(HC595_CLOCK_PIN, 0);  // Clock off
	}

    HC595_SET_LATCH();
}

//! REQUIRES: clock and data pins
void hc595_shiftOut16(u16 val, u8 msbFirst) {
    HC595_RELEASE_LATCH();

	for (int i = 0; i < 16; i++) {
		if (msbFirst == 1) {
            funDigitalWrite(HC595_DATA_PIN, (val & 0x8000) != 0); // Send MSB first
			val <<= 1; // Shift left
		} else {
			funDigitalWrite(HC595_DATA_PIN, val & 1); // Send LSB first
			val >>= 1; // Shift right
		}
		funDigitalWrite(HC595_CLOCK_PIN, 1); // Clock pulse
		funDigitalWrite(HC595_CLOCK_PIN, 0);  // Clock off
	}

    HC595_SET_LATCH();
}