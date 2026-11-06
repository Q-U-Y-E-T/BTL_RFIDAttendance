#ifndef LCD_H
#define LCD_H

#include "app_types.h"

typedef enum {
    LCD_OK = 0,
    LCD_ERROR
} LCD_Status_t;

/*
 * Goi sau MX_GPIO_Init() va MX_I2C1_Init().
 * Khoi tao LCD 16x2 qua I2C1.
 * Tra ve LCD_OK hoac LCD_ERROR.
 */
LCD_Status_t LCD_Init(void);

/*
 * Hien thi ket qua ung dung tren LCD.
 *
 * event phai khac NULL.
 * Ham doc du lieu trong luc goi, khong giu con tro event.
 * Khong sua noi dung *event.
 * Chuoi dau vao phai ket thuc bang '\\0'.
 *
 * Moi dong toi da 16 ky tu.
 * Ten dai duoc cat khi hien thi, khong sua du lieu goc.
 * Phai xoa ky tu thua cua noi dung hien thi truoc.
 *
 * Ham phai co thoi han cho, khong cho vo han.
 * Tra ve LCD_ERROR neu giao tiep LCD that bai.
 */
LCD_Status_t LCD_ShowEvent(const DisplayEvent_t *event);

#endif /* LCD_H */
