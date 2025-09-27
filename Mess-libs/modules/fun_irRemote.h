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

void irRemote_decode() {
    #ifdef IRREMOTE_DEBUG_PRINT
        printf("\nPulses: %d\n", IR_pulseCount);
        for (int i = 0; i < IR_pulseCount - 1; i++) {
            u16 rounded = 10 * (IR_durations[i] / 10);
            printf((i%2 == 0) ? "%ld " : "-%ld ", rounded);
            if (i%16 == 0 && i != 0) printf("\n");          // line seperator
        }
        printf("\n");
    #endif
    
    //# the first 2 pulses are start pulses
    printf("\nstart pulse: %ld us %ld us\n", IR_durations[0], IR_durations[1]);

    uint8_t data[8] = {0};
    int bits_processed = 0;

    //# get every other bit after the start pulses
    // the first one starts at index 3, then 5, 7, 9, and so on.
    for (int i = 3; i < IR_pulseCount; i += 2) {
        int byte_idx = bits_processed / 8;
        if (byte_idx >= 8) break;  // Buffer full
        int bit_idx = (bits_processed % 8);

        #ifdef IRREMOTE_DEBUG_PRINT
            printf("%ld ", IR_durations[i]);
            if (bit_idx == 7) printf("\n"); // line seperator
        #endif
        
        if (IR_durations[i] > IRREMOTE_PULSE_THRESHOLD_US) {
            data[byte_idx] |= (1 << bit_idx);
        }
        
        bits_processed++;
    }

    printf("\ndata: ");
    for (int i = 0; i < 8; i++) {
        printf("0x%02X ", data[i]);
    }
    printf("\n");
}

void fun_irRemote_task() {
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
        irRemote_decode();

        IR_pulseCount = 0;
        IR_refTime = micros();
        IR_lastState = IR_currentState;
    }
}