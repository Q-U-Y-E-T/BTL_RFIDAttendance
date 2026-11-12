#ifndef HOST_MAIN_H
#define HOST_MAIN_H
#include <stdint.h>
typedef enum { GPIO_PIN_RESET = 0, GPIO_PIN_SET = 1 } GPIO_PinState;
#define LED_GPIO_Port ((void *)1)
#define BUZZER_GPIO_Port ((void *)2)
#define LED_Pin 13U
#define BUZZER_Pin 8U
uint32_t HAL_GetTick(void);
void HAL_GPIO_WritePin(void *port, uint16_t pin, GPIO_PinState state);
#endif
