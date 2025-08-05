#include "ch32fun.h"
#include "modTimer.h"
#include "fun_gpio.h"
#include <stdio.h>

#define TICK_DEBOUNCE_DUR     20
#define TICK_CLICK_DUR        160
#define TICK_LONG_PRESS_DUR   4000

enum {
   BTN_DOWN = 0,
   BTN_UP,
   BTN_DOWN2,
   BUTTON_IDLE
};

enum {
   BTN_SINGLECLICK,
   BTN_DOUBLECLICK,
   BTN_LONGPRESS
};

typedef struct {
   uint8_t pin;
   uint8_t btn_state;
   uint32_t debounce_time;
   uint32_t release_time;
   uint32_t press_time;
} M_Button;

void _reset_timers(uint8_t newState, M_Button *model) {
   model->btn_state = newState;
   model->debounce_time = millis();
   model->release_time = millis();
}

void button_setup(M_Button *model) {
   if (model->pin == 0xFF) return; 
   // button_pin = pin;

	pinMode(model->pin, INPUT_PULLUP);
   _reset_timers(BUTTON_IDLE, model);
}

void button_run(M_Button *model, void (*handler)(int, uint32_t)) {
   if (model->pin == 0xFF) return;
   
   uint32_t now = millis();
   uint8_t read = digitalRead(model->pin);

   // Debounce check
   if (now - model->debounce_time < TICK_DEBOUNCE_DUR) return;
   model->debounce_time = now;

   switch (model->btn_state) {
      case BUTTON_IDLE:
         if (read == 0) {
            model->press_time = now;
            _reset_timers(BTN_DOWN, model);      // First Press  
         }
         break;

      case BTN_DOWN:
         if (read == 1) {
            _reset_timers(BTN_UP, model);        // First Release

         } else {
            // Long press detection
            uint32_t press_duration = now - model->press_time;
            if (press_duration > TICK_LONG_PRESS_DUR) {
               handler(BTN_LONGPRESS, press_duration - TICK_LONG_PRESS_DUR);
            }
         }
         break;

      case BTN_UP: {
         uint32_t release_duration = now - model->release_time;

         if (read == 0 && release_duration < TICK_CLICK_DUR) {
            // Second Press in less than TICK_CLICK_DUR
            _reset_timers(BTN_DOWN2, model);

         } else if (release_duration > TICK_CLICK_DUR) {
            handler(BTN_SINGLECLICK, 0);  // Single Click
            _reset_timers(BUTTON_IDLE, model);
         }
         break;
      }

      case BTN_DOWN2:
         // Second release
         if (read == 1) {
            handler(BTN_DOUBLECLICK, 0);  // Double Click
            _reset_timers(BUTTON_IDLE, model);
         }
         break;
   }
}