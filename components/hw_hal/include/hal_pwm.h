#ifndef HAL_PWM_H
#define HAL_PWM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hal_pwm_channel hal_pwm_channel_t;

struct hal_pwm_channel {
    int (*init)(hal_pwm_channel_t* pwm, int pin, int freq_hz, int resolution_bits);
    int (*set_duty)(hal_pwm_channel_t* pwm, uint32_t duty);
    int (*deinit)(hal_pwm_channel_t* pwm);
    void* _impl;
};

void hal_pwm_init_struct(hal_pwm_channel_t* pwm);

#ifdef __cplusplus
}
#endif

#endif /* HAL_PWM_H */
