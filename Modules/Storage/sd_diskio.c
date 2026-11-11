#include "main.h"
#include "spi.h"
#include "diskio.h"
#include "sd_diskio.h"
#include <stdint.h>
#include <string.h>

#define SD_SPI_TIMEOUT_MS  100U
#define SD_READY_MS        1000U
#define SD_INIT_MS         3000U
#define SD_DATA_MS         500U
#define SD_SECTOR_SIZE     512U

static DSTATUS sd_status = STA_NOINIT;
static uint8_t block_addressed;
static uint8_t spi_error;

/* File nay so huu SPI2 trong luc truy cap SD.
 * Khong goi tu nhieu task cung luc.
 */

static uint8_t exchange(uint8_t value)
{
    uint8_t received = 0xFF;

    if (spi_error) {
        return 0xFF;
    }

    if (HAL_SPI_TransmitReceive(
            &hspi2, &value, &received, 1,
            SD_SPI_TIMEOUT_MS) != HAL_OK) {
        spi_error = 1;
    }

    return received;
}

static void deselect_card(void)
{
    HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);
    (void)exchange(0xFF);
}

static int wait_ready(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();

    do {
        uint8_t value = exchange(0xFF);

        if (spi_error) {
            return 0;
        }
        if (value == 0xFF) {
            return 1;
        }
    } while ((uint32_t)(HAL_GetTick() - start) < timeout_ms);

    return 0;
}

static int select_card(void)
{
    HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_RESET);
    (void)exchange(0xFF);

    if (!wait_ready(SD_READY_MS)) {
        deselect_card();
        return 0;
    }

    return 1;
}

/* Goi xong van giu CS o muc Low de doc phan phan hoi con lai. */
static uint8_t send_command(uint8_t command, uint32_t argument)
{
    uint8_t crc = 0x01;
    uint8_t response = 0xFF;

    deselect_card();

    if (spi_error || !select_card()) {
        return 0xFF;
    }

    if (command == 0) {
        crc = 0x95;
    } else if (command == 8) {
        crc = 0x87;
    }

    (void)exchange((uint8_t)(0x40U | command));
    (void)exchange((uint8_t)(argument >> 24));
    (void)exchange((uint8_t)(argument >> 16));
    (void)exchange((uint8_t)(argument >> 8));
    (void)exchange((uint8_t)argument);
    (void)exchange(crc);

    for (unsigned int i = 0; i < 10U; ++i) {
        response = exchange(0xFF);
        if (spi_error || !(response & 0x80U)) {
            break;
        }
    }

    return spi_error ? 0xFF : response;
}

static int receive_block(uint8_t *buffer, uint16_t length)
{
    uint32_t start = HAL_GetTick();
    uint8_t token;

    do {
        token = exchange(0xFF);
        if (spi_error) {
            return 0;
        }
        if (token != 0xFF) {
            break;
        }
    } while ((uint32_t)(HAL_GetTick() - start) < SD_DATA_MS);

    if (token != 0xFE) {
        return 0;
    }

    /* Truyen 0xFF khi nhan du lieu, moi lan toi da 32 byte. */
    uint8_t dummy[32];
    memset(dummy, 0xFF, sizeof dummy);

    while (length > 0U) {
        uint16_t chunk = length > sizeof dummy
                       ? (uint16_t)sizeof dummy : length;

        if (HAL_SPI_TransmitReceive(
                &hspi2, dummy, buffer, chunk,
                SD_SPI_TIMEOUT_MS) != HAL_OK) {
            spi_error = 1;
            return 0;
        }

        buffer += chunk;
        length -= chunk;
    }

    /* Bo qua hai byte CRC; ban nay chua kiem tra CRC du lieu. */
    (void)exchange(0xFF);
    (void)exchange(0xFF);

    return !spi_error;
}

static int transmit_block(const uint8_t *buffer)
{
    if (!wait_ready(SD_READY_MS)) {
        return 0;
    }

    (void)exchange(0xFE);

    if (spi_error ||
        HAL_SPI_Transmit(
            &hspi2, (uint8_t *)buffer, SD_SECTOR_SIZE,
            SD_SPI_TIMEOUT_MS) != HAL_OK) {
        spi_error = 1;
        return 0;
    }

    (void)exchange(0xFF);
    (void)exchange(0xFF);

    uint8_t response = exchange(0xFF);

    if (spi_error || (response & 0x1FU) != 0x05U) {
        return 0;
    }

    /* Cho the ket thuc ghi truoc khi bao thanh cong. */
    return wait_ready(SD_READY_MS);
}

DSTATUS disk_status(BYTE pdrv)
{
    return pdrv == 0 ? sd_status : STA_NOINIT;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    uint8_t response;
    uint8_t answer[4];
    uint32_t start;
    uint32_t saved_prescaler;

    if (pdrv != 0) {
        return STA_NOINIT;
    }

    sd_status = STA_NOINIT;
    block_addressed = 0;
    spi_error = 0;

    /* Khoi tao cham: PCLK1 / 256.
     * MX_SPI2_Init() phai duoc goi truoc ham nay.
     */
    saved_prescaler = hspi2.Init.BaudRatePrescaler;
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;

    if (HAL_SPI_Init(&hspi2) != HAL_OK) {
        goto finish;
    }

    HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);
    HAL_Delay(10);

    /* It nhat 74 clock khi CS High va MOSI High. */
    for (unsigned int i = 0; i < 10U; ++i) {
        (void)exchange(0xFF);
    }

    start = HAL_GetTick();
    do {
        response = send_command(0, 0);
        if (response == 1 || spi_error) {
            break;
        }
        HAL_Delay(1);
    } while ((uint32_t)(HAL_GetTick() - start) < SD_INIT_MS);

    if (spi_error || response != 1) {
        goto finish;
    }

    /* Ban nay yeu cau SD v2. */
    if (send_command(8, 0x1AA) != 1) {
        goto finish;
    }

    for (unsigned int i = 0; i < 4U; ++i) {
        answer[i] = exchange(0xFF);
    }

    if (spi_error || answer[2] != 0x01 || answer[3] != 0xAA) {
        goto finish;
    }

    start = HAL_GetTick();
    do {
        response = send_command(55, 0);
        if (spi_error || response > 1) {
            goto finish;
        }

        response = send_command(41, 0x40000000UL);
        if (response == 0) {
            break;
        }
        if (spi_error || response != 1) {
            goto finish;
        }

        HAL_Delay(1);
    } while ((uint32_t)(HAL_GetTick() - start) < SD_INIT_MS);

    if (response != 0) {
        goto finish;
    }

    if (send_command(58, 0) != 0) {
        goto finish;
    }

    for (unsigned int i = 0; i < 4U; ++i) {
        answer[i] = exchange(0xFF);
    }

    if (spi_error || !(answer[0] & 0x80U)) {
        goto finish;
    }

    block_addressed = (answer[0] & 0x40U) != 0;

    if (!block_addressed && send_command(16, SD_SECTOR_SIZE) != 0) {
        goto finish;
    }

    sd_status = 0;

finish:
    deselect_card();

    hspi2.Init.BaudRatePrescaler = saved_prescaler;
    if (HAL_SPI_Init(&hspi2) != HAL_OK || spi_error) {
        sd_status = STA_NOINIT;
    }

    return sd_status;
}

static int sector_argument(DWORD sector, uint32_t *argument)
{
    if (block_addressed) {
        *argument = sector;
        return 1;
    }

    if (sector > UINT32_MAX / SD_SECTOR_SIZE) {
        return 0;
    }

    *argument = sector * SD_SECTOR_SIZE;
    return 1;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
    DRESULT result = RES_OK;

    if (pdrv != 0 || buff == NULL || count == 0 ||
        (DWORD)(count - 1U) > UINT32_MAX - sector) {
        return RES_PARERR;
    }
    if (sd_status & STA_NOINIT) {
        return RES_NOTRDY;
    }

    spi_error = 0;

    for (UINT i = 0; i < count; ++i) {
        uint32_t argument;

        if (!sector_argument(sector + i, &argument)) {
            result = RES_PARERR;
            break;
        }

        if (send_command(17, argument) != 0 ||
            !receive_block(buff, SD_SECTOR_SIZE)) {
            result = RES_ERROR;
            break;
        }

        buff += SD_SECTOR_SIZE;
    }

    deselect_card();

    return spi_error ? RES_ERROR : result;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
    DRESULT result = RES_OK;

    if (pdrv != 0 || buff == NULL || count == 0 ||
        (DWORD)(count - 1U) > UINT32_MAX - sector) {
        return RES_PARERR;
    }
    if (sd_status & STA_NOINIT) {
        return RES_NOTRDY;
    }

    spi_error = 0;

    for (UINT i = 0; i < count; ++i) {
        uint32_t argument;

        if (!sector_argument(sector + i, &argument)) {
            result = RES_PARERR;
            break;
        }

        if (send_command(24, argument) != 0 ||
            !transmit_block(buff)) {
            result = RES_ERROR;
            break;
        }

        buff += SD_SECTOR_SIZE;
    }

    deselect_card();

    return spi_error ? RES_ERROR : result;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    DRESULT result = RES_PARERR;

    if (pdrv != 0) {
        return RES_PARERR;
    }
    if (sd_status & STA_NOINIT) {
        return RES_NOTRDY;
    }

    spi_error = 0;

    switch (cmd) {
    case CTRL_SYNC:
        if (select_card()) {
            result = RES_OK;
        } else {
            result = RES_ERROR;
        }
        break;

    case GET_SECTOR_SIZE:
        if (buff != NULL) {
            *(WORD *)buff = SD_SECTOR_SIZE;
            result = RES_OK;
        }
        break;

    case GET_BLOCK_SIZE:
        if (buff != NULL) {
            *(DWORD *)buff = 1;
            result = RES_OK;
        }
        break;

    default:
        /* Chua ho tro format the va truy van dung luong.
         * ffconf.h hien dat _USE_MKFS = 0.
         */
        break;
    }

    deselect_card();

    return spi_error ? RES_ERROR : result;
}
uint8_t SD_DiskProbe(void)
{
    uint8_t csd[16];
    uint8_t ok = 0;

    if (sd_status & STA_NOINIT) {
        return 0;
    }

    spi_error = 0;

    /* CMD9 doc truc tiep thanh ghi CSD, khong qua cache FatFs. */
    if (send_command(9, 0) == 0) {
        ok = (uint8_t)receive_block(csd, sizeof csd);
    }

    deselect_card();

    if (spi_error || !ok) {
        sd_status = STA_NOINIT;
        return 0;
    }

    return 1;
}
