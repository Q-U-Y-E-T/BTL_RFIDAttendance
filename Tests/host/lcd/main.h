/* Mock main.h cho test LCD tren may tinh: giong Tests/host/main.h cua Quyet,
 * them kieu va ham HAL I2C ma lcd.c / lcd_display.c dung. */
#ifndef HOST_LCD_MAIN_H
#define HOST_LCD_MAIN_H
#include <stdint.h>
typedef enum { GPIO_PIN_RESET = 0, GPIO_PIN_SET = 1 } GPIO_PinState;
#define LED_GPIO_Port ((void *)1)
#define BUZZER_GPIO_Port ((void *)2)
#define LED_Pin 13U
#define BUZZER_Pin 8U
uint32_t HAL_GetTick(void);
void HAL_GPIO_WritePin(void *port, uint16_t pin, GPIO_PinState state);

typedef enum { HAL_OK = 0, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef struct { int dummy; } I2C_HandleTypeDef;
HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *h, uint16_t addr,
                                          uint8_t *p, uint16_t n, uint32_t to);
HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *h, uint16_t addr,
                                        uint32_t trials, uint32_t to);
HAL_StatusTypeDef HAL_I2C_Init(I2C_HandleTypeDef *h);
HAL_StatusTypeDef HAL_I2C_DeInit(I2C_HandleTypeDef *h);
void HAL_Delay(uint32_t ms);
#endif
