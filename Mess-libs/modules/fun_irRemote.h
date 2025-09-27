#include "ch32fun.h"
#include <stdint.h>
#include <stdio.h>

#define IRREMOTE_TIMEOUT_US 50000

int IR_REMOTE_PIN = -1;

typedef enum {
    IRRemote_Code_Begin,
    IRRemote_Code_Idle,
    IRRemote_Code_Parsing
} IRRemote_Code_State_t;

int IRRemote_Code_State = IRRemote_Code_Idle;
int lastState = 0;
int currentState = 0;
u16 pulseCount = 0;
u32 startTime;

u32 durations[200]; // Store pulse durations
u8 states [200];

//! IR Remote pin are Pulled up
void fun_irRemote_init(int pin) {
    IR_REMOTE_PIN = pin;
    funPinMode(IR_REMOTE_PIN, GPIO_CFGLR_IN_PUPD);
    startTime = micros();
}

void fun_irRemote_task() {
    if (IR_REMOTE_PIN == -1) return;
    currentState = funDigitalRead(IR_REMOTE_PIN);

    if ((IRRemote_Code_State == IRRemote_Code_Idle) && !currentState) {
        IRRemote_Code_State = IRRemote_Code_Begin;

        pulseCount = 0;
        startTime = micros();
        lastState = currentState;
    }
    else if (
        (IRRemote_Code_State == IRRemote_Code_Begin) &&
        (currentState != lastState)    
    ) {
        IRRemote_Code_State = IRRemote_Code_Parsing;

        // State changed - record duration
        durations[pulseCount] = micros() - startTime;
        states[pulseCount] = currentState;

        pulseCount++;
        startTime = micros();
        lastState = currentState;
    }
    else if (
        (IRRemote_Code_State == IRRemote_Code_Parsing) &&
        (currentState != lastState)
    ) {
        // State changed - record duration
        durations[pulseCount] = micros() - startTime;
        states[pulseCount] = currentState;

        pulseCount++;
        startTime = micros();
        lastState = currentState;        
    }
    else if (
        ((IRRemote_Code_State == IRRemote_Code_Parsing) || 
            (IRRemote_Code_State == IRRemote_Code_Begin))
        && (micros() - startTime > 100000)
    ) {
        IRRemote_Code_State = IRRemote_Code_Idle;
        printf("\nPulses: %d\n", pulseCount);
    
        for (int i = 0; i < pulseCount - 1; i++) {
            u16 rounded = 100 * (durations[i] / 100);
            printf(states[i] ? "%ld " : "-%ld ", rounded);
            if (i%16 == 0 && i != 0) printf("\n");
        }
        printf("\n");

        pulseCount = 0;
        startTime = micros();
        lastState = currentState;
    }
}


// void fun_irRemote_task() {
//     if (IR_REMOTE_PIN == -1) return;

//     u32 startTime = micros();
//     int pulseCount = 0;
//     int lastState = 0;
//     int currentState;
    
//     // Capture signal for up to 100ms
//     while (micros() - startTime < 100000 && pulseCount < 100) {
//         currentState = funDigitalRead(IR_REMOTE_PIN);
        
//         if (currentState != lastState) {
//             // State changed - record duration
//             u32 duration = micros() - startTime;
            
//             if (pulseCount > 0) {
//                 durations[pulseCount - 1] = duration;
//                 states[pulseCount - 1] = currentState;
//             }
            
//             pulseCount++;
//             startTime = micros();
//             lastState = currentState;
//         }
//     }
    
//     // Print the raw data
//     if (pulseCount > 1) {
//         printf("Pulses: %d\n", pulseCount);
    
//         for (int i = 0; i < pulseCount - 1; i++) {
//             printf("%d ", durations[i]);
//         }

//         printf("\nStates: ");
//         for (int i = 0; i < pulseCount - 1; i++) {
//             printf("%d ", states[i]);
//         }

//         printf("\n---");
//     }

    
//     Delay_Ms(50); // Debounce
// }