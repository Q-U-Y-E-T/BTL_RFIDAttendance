/* RC522 driver adapted from the supplied demo for the shared STM32 HAL project. */
#include "rfid.h"
#include "rfid_delay.h"
#include "main.h"
#include "spi.h"
#include <string.h>

/* Temporary diagnostics consumed by the UART test main.c. */
const char * volatile rfid_debug_step = "NOT_STARTED";
volatile uint32_t rfid_debug_hal_status;
volatile uint32_t rfid_debug_irq;
volatile uint32_t rfid_debug_error;
volatile uint32_t rfid_debug_count;
volatile uint32_t rfid_debug_version;

enum {
    REG_COMMAND = 0x01,
    REG_COM_IRQ = 0x04,
    REG_ERROR = 0x06,
    REG_FIFO_DATA = 0x09,
    REG_FIFO_LEVEL = 0x0A,
    REG_CONTROL = 0x0C,
    REG_BIT_FRAMING = 0x0D,
    REG_COLLISION = 0x0E,
    REG_MODE = 0x11,
    REG_TX_MODE = 0x12,
    REG_RX_MODE = 0x13,
    REG_TX_CONTROL = 0x14,
    REG_TX_ASK = 0x15,
    REG_RF_CONFIG = 0x26,
    REG_TIMER_MODE = 0x2A,
    REG_TIMER_PRESCALER = 0x2B,
    REG_TIMER_RELOAD_HIGH = 0x2C,
    REG_TIMER_RELOAD_LOW = 0x2D,
    REG_VERSION = 0x37
};

static int transfer_register(uint8_t address, uint8_t value, uint8_t *received)
{
    uint8_t tx[2] = {address, value};
    uint8_t rx[2];
    HAL_StatusTypeDef status;

    HAL_GPIO_WritePin(RFID_CS_GPIO_Port, RFID_CS_Pin, GPIO_PIN_RESET);
    status = HAL_SPI_TransmitReceive(&hspi1, tx, rx, 2U, 10U);
    HAL_GPIO_WritePin(RFID_CS_GPIO_Port, RFID_CS_Pin, GPIO_PIN_SET);

    rfid_debug_hal_status = (uint32_t)status;
    if (status != HAL_OK) {
        rfid_debug_step = "SPI_TRANSFER";
        return 0;
    }
    if (received != NULL) {
        *received = rx[1];
    }
    return 1;
}

static int write_register(uint8_t reg, uint8_t value)
{
    return transfer_register((uint8_t)((reg << 1) & 0x7EU), value, NULL);
}

static int read_register(uint8_t reg, uint8_t *value)
{
    return transfer_register((uint8_t)(((reg << 1) & 0x7EU) | 0x80U), 0U, value);
}

static void calculate_crc_a(const uint8_t *data, unsigned length, uint8_t crc_out[2])
{
    uint16_t crc = 0x6363U;

    for (unsigned i = 0; i < length; ++i) {
        uint8_t value = (uint8_t)(data[i] ^ (uint8_t)crc);
        value ^= (uint8_t)(value << 4);
        crc = (uint16_t)((crc >> 8) ^ ((uint16_t)value << 8)
                        ^ ((uint16_t)value << 3) ^ (value >> 4));
    }
    crc_out[0] = (uint8_t)crc;
    crc_out[1] = (uint8_t)(crc >> 8);
}

static RFID_Status_t transceive(const uint8_t *tx, unsigned length,
                                uint8_t last_tx_bits, uint8_t *rx,
                                unsigned capacity, unsigned *received)
{
    uint8_t irq, errors, count, last_rx_bits;
    uint32_t start;

    rfid_debug_step = "START_COMMAND";
    rfid_debug_irq = 0U;
    rfid_debug_error = 0U;
    rfid_debug_count = 0U;
    *received = 0U;
    if (!write_register(REG_COMMAND, 0U)
        || !write_register(REG_COM_IRQ, 0x7FU)
        || !write_register(REG_FIFO_LEVEL, 0x80U)) {
        return RFID_ERROR;
    }
    for (unsigned i = 0; i < length; ++i) {
        if (!write_register(REG_FIFO_DATA, tx[i])) {
            return RFID_ERROR;
        }
    }
    if (!write_register(REG_BIT_FRAMING, last_tx_bits)
        || !write_register(REG_COMMAND, 0x0CU)
        || !write_register(REG_BIT_FRAMING, (uint8_t)(last_tx_bits | 0x80U))) {
        return RFID_ERROR;
    }

    rfid_debug_step = "WAIT_IRQ";
    start = HAL_GetTick();
    for (;;) {
        if (!read_register(REG_COM_IRQ, &irq)) {
            return RFID_ERROR;
        }
        rfid_debug_irq = irq;
        /* Received data takes priority. Timer expiry precedes an idle-only event. */
        if ((irq & 0x20U) != 0U) {
            break;
        }
        if ((irq & 0x01U) != 0U) {
            if (!write_register(REG_COMMAND, 0U)) {
                return RFID_ERROR;
            }
            rfid_debug_step = "NO_RESPONSE";
            return RFID_NO_CARD;
        }
        if ((irq & 0x10U) != 0U) {
            break;
        }
        if ((uint32_t)(HAL_GetTick() - start) >= 40U) {
            (void)write_register(REG_COMMAND, 0U);
            rfid_debug_step = "WAIT_IRQ_TIMEOUT";
            return RFID_ERROR;
        }
        RFID_DelayMs(1U);
    }

    if (!write_register(REG_BIT_FRAMING, last_tx_bits)
        || !read_register(REG_ERROR, &errors)) {
        return RFID_ERROR;
    }
    rfid_debug_error = errors;
    rfid_debug_step = "RX_ERROR";
    /* Reject collision, buffer overflow, parity and protocol errors. */
    if ((errors & 0x1BU) != 0U) {
        return RFID_ERROR;
    }
    if (!read_register(REG_FIFO_LEVEL, &count)
        || !read_register(REG_CONTROL, &last_rx_bits)) {
        return RFID_ERROR;
    }
    rfid_debug_count = count;
    rfid_debug_step = "RX_FORMAT";
    if (count > capacity || (last_rx_bits & 7U) != 0U) {
        return RFID_ERROR;
    }
    for (unsigned i = 0; i < count; ++i) {
        if (!read_register(REG_FIFO_DATA, &rx[i])) {
            return RFID_ERROR;
        }
    }
    rfid_debug_step = "RESPONSE_OK";
    *received = count;
    return RFID_OK;
}

RFID_Status_t RFID_Init(void)
{
    static const uint8_t setup[][2] = {
        {REG_TX_MODE, 0U}, {REG_RX_MODE, 0x08U},
        {REG_TIMER_MODE, 0x8DU}, {REG_TIMER_PRESCALER, 0x3EU},
        {REG_TIMER_RELOAD_HIGH, 0U}, {REG_TIMER_RELOAD_LOW, 30U},
        {REG_TX_ASK, 0x40U}, {REG_MODE, 0x3DU},
        {REG_RF_CONFIG, 0x50U}, {REG_COLLISION, 0x80U}
    };
    uint8_t value;
    uint32_t start;

    rfid_debug_step = "INIT_RESET";
    rfid_debug_version = 0U;
    HAL_GPIO_WritePin(RFID_RST_GPIO_Port, RFID_RST_Pin, GPIO_PIN_RESET);
    RFID_DelayMs(2U);
    HAL_GPIO_WritePin(RFID_RST_GPIO_Port, RFID_RST_Pin, GPIO_PIN_SET);
    RFID_DelayMs(50U);
    if (!write_register(REG_COMMAND, 0x0FU)) {
        return RFID_ERROR;
    }
    RFID_DelayMs(50U);

    start = HAL_GetTick();
    for (;;) {
        if (!read_register(REG_COMMAND, &value)) {
            return RFID_ERROR;
        }
        if ((value & 0x10U) == 0U) {
            break;
        }
        if ((uint32_t)(HAL_GetTick() - start) >= 100U) {
            return RFID_ERROR;
        }
        RFID_DelayMs(1U);
    }
    rfid_debug_step = "INIT_VERSION";
    if (!read_register(REG_VERSION, &value)) {
        return RFID_ERROR;
    }
    rfid_debug_version = value;
    if (value == 0U || value == 0xFFU) {
        return RFID_ERROR;
    }
    rfid_debug_step = "INIT_CONFIG";
    for (unsigned i = 0; i < sizeof(setup) / sizeof(setup[0]); ++i) {
        if (!write_register(setup[i][0], setup[i][1])) {
            return RFID_ERROR;
        }
    }
    if (!read_register(REG_TX_CONTROL, &value)
        || !write_register(REG_TX_CONTROL, (uint8_t)(value | 3U))) {
        return RFID_ERROR;
    }
    rfid_debug_step = "INIT_OK";
    return RFID_OK;
}

RFID_Status_t RFID_ReadCard(CardEvent_t *card)
{
    uint8_t request = 0x52U; /* WUPA also wakes a card halted by the previous read. */
    uint8_t rx[10], frame[9], crc[2], part[5];
    unsigned received, position = 0U;
    RFID_Status_t status;
    CardEvent_t result = {0};

    if (card == NULL) {
        return RFID_ERROR;
    }
    status = transceive(&request, 1U, 7U, rx, sizeof(rx), &received);
    if (status != RFID_OK) {
        return status;
    }
    rfid_debug_step = "WUPA_LENGTH";
    if (received != 2U) {
        return RFID_ERROR;
    }

    for (unsigned level = 0; level < 3U; ++level) {
        frame[0] = (uint8_t)(0x93U + 2U * level);
        frame[1] = 0x20U;
        status = transceive(frame, 2U, 0U, rx, sizeof(rx), &received);
        if (status != RFID_OK) {
            return RFID_ERROR;
        }
        rfid_debug_step = "ANTICOLLISION_BCC";
        if (received != 5U
            || (uint8_t)(rx[0] ^ rx[1] ^ rx[2] ^ rx[3]) != rx[4]) {
            return RFID_ERROR;
        }
        memcpy(part, rx, sizeof(part));
        frame[1] = 0x70U;
        memcpy(frame + 2, part, sizeof(part));
        calculate_crc_a(frame, 7U, frame + 7);
        status = transceive(frame, sizeof(frame), 0U, rx, sizeof(rx), &received);
        if (status != RFID_OK) {
            return RFID_ERROR;
        }
        rfid_debug_step = "SELECT_LENGTH";
        if (received != 3U) {
            return RFID_ERROR;
        }
        rfid_debug_step = "SELECT_CRC";
        calculate_crc_a(rx, 1U, crc);
        if (crc[0] != rx[1] || crc[1] != rx[2]) {
            return RFID_ERROR;
        }
        rfid_debug_step = "UID_CASCADE";
        if (part[0] == 0x88U) {
            if (level == 2U || (rx[0] & 4U) == 0U) {
                return RFID_ERROR;
            }
            for (unsigned i = 1U; i < 4U; ++i) {
                result.uid[position++] = part[i];
            }
        } else {
            if ((rx[0] & 4U) != 0U) {
                return RFID_ERROR;
            }
            for (unsigned i = 0; i < 4U; ++i) {
                result.uid[position++] = part[i];
            }
            result.uid_len = (uint8_t)position;

            frame[0] = 0x50U;
            frame[1] = 0U;
            calculate_crc_a(frame, 2U, frame + 2);
            /* A correctly halted card sends no response. */
            status = transceive(frame, 4U, 0U, rx, sizeof(rx), &received);
            if (status != RFID_NO_CARD) {
                if (status == RFID_OK) {
                    rfid_debug_step = "HALT_UNEXPECTED_REPLY";
                }
                return RFID_ERROR;
            }
            result.uptime_ms = HAL_GetTick();
            rfid_debug_step = "CARD_OK";
            *card = result;
            return RFID_OK;
        }
    }
    return RFID_ERROR;
}
