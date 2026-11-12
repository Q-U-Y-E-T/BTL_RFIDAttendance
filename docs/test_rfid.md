# Kiem thu driver RC522

- Board: STM32F103C8T6.
- Ngay kiem thu: 11/11/2026.
- RC522 dung SPI1: PA5 SCK, PA6 MISO, PA7 MOSI.
- CS: PA4. RST: PB0. Nguon: 3.3V, chung GND.
- Khoi tao bang chan RST, sau do dua chip ve Idle.
- RxMode: 0x08. RFCfg: 0x50.

## Ket qua
- Khong co the: RFID_NO_CARD.
- Doc dung UID B369A2A7.
- Rut the va dua lai: doc duoc.
- The khac: doc duoc UID khac.
- Reset board: khoi tao va doc lai duoc.

## Bat lai bai test
Chay git apply --check Tests/rfid_test.patch.
Neu thanh cong, chay git apply Tests/rfid_test.patch.
Build va nap firmware, xem UART PA9 o 115200-8N1.

Driver co the tra lai UID khi giu the.
Tang ung dung phu trach chong diem danh lap.
