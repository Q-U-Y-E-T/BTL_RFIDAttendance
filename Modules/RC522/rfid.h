#ifndef RFID_H
#define RFID_H

#include "app_types.h"

typedef enum {
    RFID_OK = 0,
    RFID_NO_CARD,
    RFID_ERROR
} RFID_Status_t;

/*
 * Goi sau MX_GPIO_Init() va MX_SPI1_Init().
 * Khoi tao RC522.
 * Tra ve RFID_OK hoac RFID_ERROR.
 */
RFID_Status_t RFID_Init(void);

/*
 * Doc UID vao *card.
 *
 * card phai khac NULL.
 * RFID_OK: card chua UID hop le, uid_len va uptime_ms.
 * RFID_NO_CARD: khong doc duoc the trong lan tham do nay.
 * RFID_ERROR: loi giao tiep hoac du lieu.
 *
 * Chi su dung noi dung *card khi tra ve RFID_OK.
 * Ham phai co thoi han cho, khong cho vo han.
 * Ham co the tra lai cung UID neu the van con truoc dau doc.
 * Viec chong diem danh lap do tang ung dung xu ly.
 */
RFID_Status_t RFID_ReadCard(CardEvent_t *card);

#endif /* RFID_H */
