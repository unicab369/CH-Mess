#include "ch32fun.h"
#include <stdio.h>

#include "../fun_timPWM.h"
#include "../../modules/systick_irq.h"

int main() {
    SystemInit();
    systick_init();			//! required for millis()

    funGpioInitAll();
	Delay_Ms(100);

    TIM_PWM_t pwm_CH1 = {
		.pin = PD4,
        .TIM = TIM2,
		.CCER = TIM_CC1E
	};

	TIM_PWM_t pwm_CH2 = {
		.pin = PD3,
        .TIM = TIM2,
		.CCER = TIM_CC2E
	};
	
	TIM_PWM_t pwm_CH3 = {
		.pin = PC0,
        .TIM = TIM2,
		.CCER = TIM_CC3E
	};


	TIM_PWM_t pwm_CH4 = {
		.pin = PD7,
        .TIM = TIM2,
		.CCER = TIM_CC4E
	};

	fun_timPWM_init(&pwm_CH1);
	fun_timPWM_reload(&pwm_CH1);
	fun_timPWM_reload(&pwm_CH2);
	fun_timPWM_reload(&pwm_CH3);
	fun_timPWM_reload(&pwm_CH4);

    uint32_t sec_time = 0;

    while(1) {
        uint32_t now = millis();

		fun_timPWM_task(now, &pwm_CH1);
		fun_timPWM_task(now, &pwm_CH2);
		fun_timPWM_task(now, &pwm_CH3);
		fun_timPWM_task(now, &pwm_CH4);

        if (now - sec_time > 1000) {
			sec_time = now;
            printf(".");
        }
	}
}