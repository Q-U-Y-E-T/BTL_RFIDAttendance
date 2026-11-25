#include "lcd.h"
#include "i2c.h"

#include <stddef.h>

#ifdef LCD_I2C_ADDR
static const uint8_t k_lcd_addr_list[] = { LCD_I2C_ADDR };
#else
static const uint8_t k_lcd_addr_list[] = { 0x27U, 0x3FU };
#endif

#define LCD_I2C_TIMEOUT_MS      20U
#define LCD_PROBE_TRIALS        2U

#define LCD_COLS                16U

#define LCD_PIN_RS              0x01U
#define LCD_PIN_RW              0x02U
#define LCD_PIN_EN              0x04U
#define LCD_PIN_BACKLIGHT       0x08U

#define LCD_CMD_CLEAR           0x01U
#define LCD_CMD_ENTRY_INC       0x06U
#define LCD_CMD_DISPLAY_OFF     0x08U
#define LCD_CMD_DISPLAY_ON      0x0CU
#define LCD_CMD_FUNC_4BIT_2LINE 0x28U
#define LCD_CMD_SET_DDRAM       0x80U
#define LCD_LINE1_ADDR          0x00U
#define LCD_LINE2_ADDR          0x40U

static uint16_t s_dev_addr;
static uint8_t  s_ready;

static HAL_StatusTypeDef lcd_transmit(uint8_t *buf, uint16_t len)
{
    return HAL_I2C_Master_Transmit(&hi2c1, s_dev_addr, buf, len,
                                   LCD_I2C_TIMEOUT_MS);
}

static void lcd_pack_byte(uint8_t *out, uint8_t value, uint8_t rs)
{
    const uint8_t ctrl = LCD_PIN_BACKLIGHT | (rs ? LCD_PIN_RS : 0U);
    const uint8_t hi = (uint8_t)(value & 0xF0U);
    const uint8_t lo = (uint8_t)((value << 4) & 0xF0U);

    out[0] = (uint8_t)(hi | ctrl | LCD_PIN_EN);
    out[1] = (uint8_t)(hi | ctrl);
    out[2] = (uint8_t)(lo | ctrl | LCD_PIN_EN);
    out[3] = (uint8_t)(lo | ctrl);
}

static HAL_StatusTypeDef lcd_write_nibble(uint8_t high_nibble)
{
    uint8_t buf[2];
    buf[0] = (uint8_t)((high_nibble & 0xF0U) | LCD_PIN_BACKLIGHT | LCD_PIN_EN);
    buf[1] = (uint8_t)((high_nibble & 0xF0U) | LCD_PIN_BACKLIGHT);
    return lcd_transmit(buf, sizeof(buf));
}

static HAL_StatusTypeDef lcd_write_cmd(uint8_t cmd)
{
    uint8_t buf[4];
    lcd_pack_byte(buf, cmd, 0U);
    return lcd_transmit(buf, sizeof(buf));
}

static char lcd_sanitize(char c)
{
    const unsigned char u = (unsigned char)c;
    return (u >= 0x20U && u <= 0x7DU) ? c : '?';
}

static void lcd_fill_line(char line[LCD_COLS], const char *src, size_t max_len)
{
    size_t i = 0U;

    if (src != NULL) {
        for (; i < LCD_COLS && i < max_len && src[i] != '\0'; i++) {
            line[i] = lcd_sanitize(src[i]);
        }
    }
    for (; i < LCD_COLS; i++) {
        line[i] = ' ';
    }
}

static void lcd_fill_text(char line[LCD_COLS], const char *text)
{
    lcd_fill_line(line, text, LCD_COLS);
}

static HAL_StatusTypeDef lcd_write_line(uint8_t ddram_addr,
                                        const char line[LCD_COLS])
{
    uint8_t buf[4U * (1U + LCD_COLS)];
    size_t i;

    lcd_pack_byte(&buf[0], (uint8_t)(LCD_CMD_SET_DDRAM | ddram_addr), 0U);
    for (i = 0U; i < LCD_COLS; i++) {
        lcd_pack_byte(&buf[4U * (i + 1U)], (uint8_t)line[i], 1U);
    }
    return lcd_transmit(buf, (uint16_t)sizeof(buf));
}

static HAL_StatusTypeDef lcd_probe(void)
{
    size_t i;

    for (i = 0U; i < sizeof(k_lcd_addr_list); i++) {
        const uint16_t addr = (uint16_t)(k_lcd_addr_list[i] << 1);
        if (HAL_I2C_IsDeviceReady(&hi2c1, addr, LCD_PROBE_TRIALS,
                                  LCD_I2C_TIMEOUT_MS) == HAL_OK) {
            s_dev_addr = addr;
            return HAL_OK;
        }
    }
    return HAL_ERROR;
}

LCD_Status_t LCD_Init(void)
{
    uint8_t idle = LCD_PIN_BACKLIGHT;

    s_ready = 0U;

    HAL_Delay(50U);

    if (lcd_probe() != HAL_OK) {
        return LCD_ERROR;
    }

    if (lcd_transmit(&idle, 1U) != HAL_OK) {
        return LCD_ERROR;
    }

    if (lcd_write_nibble(0x30U) != HAL_OK) { return LCD_ERROR; }
    HAL_Delay(5U);
    if (lcd_write_nibble(0x30U) != HAL_OK) { return LCD_ERROR; }
    HAL_Delay(1U);
    if (lcd_write_nibble(0x30U) != HAL_OK) { return LCD_ERROR; }
    HAL_Delay(1U);
    if (lcd_write_nibble(0x20U) != HAL_OK) { return LCD_ERROR; }
    HAL_Delay(1U);

    if (lcd_write_cmd(LCD_CMD_FUNC_4BIT_2LINE) != HAL_OK) { return LCD_ERROR; }
    if (lcd_write_cmd(LCD_CMD_DISPLAY_OFF) != HAL_OK)     { return LCD_ERROR; }
    if (lcd_write_cmd(LCD_CMD_CLEAR) != HAL_OK)           { return LCD_ERROR; }
    HAL_Delay(2U);
    if (lcd_write_cmd(LCD_CMD_ENTRY_INC) != HAL_OK)       { return LCD_ERROR; }
    if (lcd_write_cmd(LCD_CMD_DISPLAY_ON) != HAL_OK)      { return LCD_ERROR; }

    s_ready = 1U;
    return LCD_OK;
}

LCD_Status_t LCD_ShowEvent(const DisplayEvent_t *event)
{
    char line1[LCD_COLS];
    char line2[LCD_COLS];

    if (event == NULL || s_ready == 0U) {
        return LCD_ERROR;
    }

    switch (event->status) {
    case APP_READY:
        lcd_fill_text(line1, "MOI QUET THE");
        lcd_fill_text(line2, NULL);
        break;

    case APP_CARD_ACCEPTED:

        lcd_fill_line(line1, event->student_id, sizeof(event->student_id));
        lcd_fill_line(line2, event->student_name, sizeof(event->student_name));
        break;

    case APP_CARD_UNKNOWN:
        lcd_fill_text(line1, "THE KHONG HOP LE");
        lcd_fill_text(line2, "VUI LONG THU LAI");
        break;

    case APP_READER_ERROR:
        lcd_fill_text(line1, "LOI DAU DOC RFID");
        lcd_fill_text(line2, "KIEM TRA RC522");
        break;

    case APP_STORAGE_ERROR:
        lcd_fill_text(line1, "LOI LUU DU LIEU");
        lcd_fill_text(line2, "KIEM TRA THE SD");
        break;

    case APP_LIST_ERROR:
        lcd_fill_text(line1, "LOI DANH SACH");
        lcd_fill_text(line2, "KIEM TRA FILE");
        break;

    default:

        return LCD_ERROR;
    }

    if (lcd_write_line(LCD_LINE1_ADDR, line1) != HAL_OK) {
        return LCD_ERROR;
    }
    if (lcd_write_line(LCD_LINE2_ADDR, line2) != HAL_OK) {
        return LCD_ERROR;
    }
    return LCD_OK;
}
