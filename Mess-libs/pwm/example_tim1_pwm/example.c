#include "ch32fun.h"
#include <stdio.h>

#include "../tim1_pwm.h"
#include "../../modules/systick_irq.h"

int main() {
    SystemInit();
    systick_init();			//! required for millis()

    funGpioInitAll();
	Delay_Ms(100);


    PWM_GPIO_t pwm_CH1 = {
		.pin = PD2,
		.CCER = TIM_CC1E
	};

	PWM_GPIO_t pwm_CH1c = {
		.pin = PD0,
		.CCER = TIM_CC1NE
	};

	PWM_GPIO_t pwm_CH2 = {
		.pin = PA1,
		.CCER = TIM_CC2E
	};
	
	PWM_GPIO_t pwm_CH2c = {
		.pin = PA2,
		.CCER = TIM_CC2NE
	};

	PWM_GPIO_t pwm_CH3 = {
		.pin = PC3,
		.CCER = TIM_CC3E
	};


	PWM_GPIO_t pwm_CH4 = {
		.pin = PC4,
		.CCER = TIM_CC4E
	};

	fun_t1pwm_init();
	fun_t1pwm_reload(&pwm_CH1);
	fun_t1pwm_reload(&pwm_CH1c);
	fun_t1pwm_reload(&pwm_CH2);
	fun_t1pwm_reload(&pwm_CH2c);
	fun_t1pwm_reload(&pwm_CH3);
	fun_t1pwm_reload(&pwm_CH4);

    uint32_t sec_time = 0;

    while(1) {
        uint32_t now = millis();

		fun_t1pwm_task(now, &pwm_CH1);
		fun_t1pwm_task(now, &pwm_CH1c);
		fun_t1pwm_task(now, &pwm_CH2);
		fun_t1pwm_task(now, &pwm_CH2c);
		fun_t1pwm_task(now, &pwm_CH3);
		fun_t1pwm_task(now, &pwm_CH4);

        if (now - sec_time > 1000) {
			sec_time = now;
            printf(".");
        }
	}
}