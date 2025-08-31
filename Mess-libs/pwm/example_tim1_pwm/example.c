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
		.pin = PD2,
        .TIM = TIM1,
		.CCER = TIM_CC1E
	};

	TIM_PWM_t pwm_CH1c = {
		.pin = PD0,
        .TIM = TIM1,
		.CCER = TIM_CC1NE
	};

	TIM_PWM_t pwm_CH2 = {
		.pin = PA1,
        .TIM = TIM1,
		.CCER = TIM_CC2E
	};
	
	TIM_PWM_t pwm_CH2c = {
		.pin = PA2,
        .TIM = TIM1,
		.CCER = TIM_CC2NE
	};

	TIM_PWM_t pwm_CH3 = {
		.pin = PC3,
        .TIM = TIM1,
		.CCER = TIM_CC3E
	};


	TIM_PWM_t pwm_CH4 = {
		.pin = PC4,
        .TIM = TIM1,
		.CCER = TIM_CC4E
	};

	fun_timPWM_init(&pwm_CH1);
	fun_timPWM_reload(&pwm_CH1);
	fun_timPWM_reload(&pwm_CH1c);
	fun_timPWM_reload(&pwm_CH2);
	fun_timPWM_reload(&pwm_CH2c);
	fun_timPWM_reload(&pwm_CH3);
	fun_timPWM_reload(&pwm_CH4);

    uint32_t sec_time = 0;

    while(1) {
        uint32_t now = millis();

		fun_timPWM_task(now, &pwm_CH1);
		fun_timPWM_task(now, &pwm_CH1c);
		fun_timPWM_task(now, &pwm_CH2);
		fun_timPWM_task(now, &pwm_CH2c);
		fun_timPWM_task(now, &pwm_CH3);
		fun_timPWM_task(now, &pwm_CH4);

        if (now - sec_time > 1000) {
			sec_time = now;
            printf(".");
        }
	}
}