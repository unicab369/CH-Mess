#include "ch32fun.h"
#include <stdint.h>
#include <stdio.h>

#define IRREMOTE_DEBUG_PRINT

#define IRREMOTE_TIMEOUT_US 50000
#define IRREMOTE_MAX_PULSES 200

// low state: average 500us-600us
// high state: average 1500us-1700us
// estimated threshold: greater than 1000 for high state
#define IRREMOTE_PULSE_THRESHOLD_US 1000
#define IRREMOTE_BUFFER_SIZE 4

int IR_REMOTE_PIN = -1;

typedef enum {
    IRRemote_Code_Begin,
    IRRemote_Code_Idle,
    IRRemote_Code_Parsing
} IRRemote_Code_State_t;

int IRRemote_Code_State = IRRemote_Code_Idle;
int IR_lastState = 0;
int IR_currentState = 0;
u16 IR_pulseCount = 0;
u32 IR_refTime;
u32 IR_durations[IRREMOTE_MAX_PULSES];  // Store pulse durations in microseconds

//! IR Remote pin are Pulled up
void fun_irRemote_init(int pin) {
    IR_REMOTE_PIN = pin;
    funPinMode(IR_REMOTE_PIN, GPIO_CFGLR_IN_PUPD);
    IR_refTime = micros();
}

//! ####################################
//! HANDLER FUNCTIONS
//! ####################################

void irRemote_decode(void (*handler)(u16, u16)) {
    if (IR_pulseCount < 1) return;

    //# the first 2 pulses are start pulses
    #ifdef IRREMOTE_DEBUG_PRINT
        // start pulses: 9ms HIGH, 4.5ms LOW
        printf("\nPulses: %d\n", IR_pulseCount);
        printf("start pulses (us): %ld %ld\n", IR_durations[0], IR_durations[1]);

        u8 len_count = 0;
        for (int i = 2; i < IR_pulseCount; i++) {
            u16 rounded = 10 * (IR_durations[i] / 10);
            printf((i%2 == 0) ? "\n%ld " : "-%ld ", rounded);
            len_count++;
            if (len_count%16 == 0) printf("\n"); // line seperator
        }
    #endif

    // u16 data_lsb[IRREMOTE_BUFFER_SIZE] = {0};
    u16 data_msb[IRREMOTE_BUFFER_SIZE] = {0};
    int bits_processed = 0;

    for (int i = 3; i < IR_pulseCount; i += 2) {
        if (bits_processed >= 16*IRREMOTE_BUFFER_SIZE) break;
        
        int word_idx = bits_processed / 16;
        int bit_pos = bits_processed % 16;
        
        if (IR_durations[i] > IRREMOTE_PULSE_THRESHOLD_US) {
            // LSB first (original)
            // data_lsb[word_idx] |= (1 << bit_pos);
            // MSB first (reversed)
            data_msb[word_idx] |= (1 << (15 - bit_pos));
        }
        
        bits_processed++;
    }

    handler(data_msb[0], data_msb[1]);

    #ifdef IRREMOTE_DEBUG_PRINT
        // printf("\nLSB First: ");
        // for (int i = 0; i < 4; i++) printf("0x%04X ", data_lsb[i]);
        // printf("\n");

        printf("MSB First: ");
        for (int i = 0; i < 4; i++) printf("0x%04X ", data_msb[i]);
        printf("\n");
    #endif
}

void fun_irRemote_task(void (*handler)(u16, u16)) {
    if (IR_REMOTE_PIN == -1) return;
    IR_currentState = funDigitalRead(IR_REMOTE_PIN);

    if ((IRRemote_Code_State == IRRemote_Code_Idle) && !IR_currentState) {
        IRRemote_Code_State = IRRemote_Code_Begin;

        IR_pulseCount = 0;
        IR_refTime = micros();
        IR_lastState = IR_currentState;
    }
    else if (
        (IRRemote_Code_State == IRRemote_Code_Begin) &&
        (IR_currentState != IR_lastState)    
    ) {
        IRRemote_Code_State = IRRemote_Code_Parsing;
        // State changed - record duration
        IR_durations[IR_pulseCount] = micros() - IR_refTime;

        IR_pulseCount++;
        IR_refTime = micros();
        IR_lastState = IR_currentState;
    }
    else if (
        (IRRemote_Code_State == IRRemote_Code_Parsing) &&
        (IR_currentState != IR_lastState)
    ) {
        // State changed - record duration
        IR_durations[IR_pulseCount] = micros() - IR_refTime;

        IR_pulseCount++;
        IR_refTime = micros();
        IR_lastState = IR_currentState;        
    }
    else if (
        ((IRRemote_Code_State == IRRemote_Code_Parsing) || 
            (IRRemote_Code_State == IRRemote_Code_Begin))
        && (micros() - IR_refTime > 100000)
    ) {
        IRRemote_Code_State = IRRemote_Code_Idle;
        irRemote_decode(handler);

        IR_pulseCount = 0;
        IR_refTime = micros();
        IR_lastState = IR_currentState;
    }
}