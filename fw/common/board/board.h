/*
 * board — единственное место в проекте, где встречаются номера GPIO.
 * Two boards, chosen at build time by RG_BOARD (fw/CMakeLists.txt): 1.28 (default in this
 * fork, pin block below) and 2.1 (upstream, described in this comment).
 *
 * Плата: Waveshare ESP32-S3-Touch-LCD-2.1 (ESP32-S3R8, 16 МБ флеш, 8 МБ PSRAM),
 * схема: hw/board/ESP32-S3-Touch-LCD-2.1_schematic_diagram.pdf, даташиты — hw/datasheets/
 *
 * Почти все GPIO заняты RGB-экраном. Свободные выведены на 12-пиновый разъём
 * J9 «12PIN Multi-function Interface» (SH1.0), нумерация по схеме:
 *
 *   Пин  Сигнал        Назначение в проекте
 *   1    GND
 *   2    5V (USB_5V)   питание от DC-DC 12→5 В
 *   3    GPIO19 (D-)   CAN RX  ← RXD трансивера
 *   4    GPIO20 (D+)   CAN TX  → TXD трансивера
 *   5    GND
 *   6    3V3           питание трансивера
 *   7    GPIO7  SCL    I2C платы, резерв
 *   8    GPIO15 SDA    I2C платы, резерв
 *   9    GPIO43 TXD    логи (консоль UART0)
 *   10   GPIO44 RXD    кнопка 2
 *   11   GND
 *   12   GPIO0         кнопка 1 (параллельно BOOT)
 *
 * Три особенности платы, из которых выросла раскладка:
 *
 *  1. Type-C на плате идёт через CH343 на UART0, а не на встроенный USB.
 *     Поэтому GPIO19/20 свободны, и CAN на них не мешает ни прошивке, ни логам.
 *
 *  2. Переключатель FSUSB42 отдаёт GPIO43/44 то CH343, то разъёму J9, и
 *     управляется напряжением VBUS на Type-C. Пока Type-C под питанием, кнопка 2
 *     от разъёма отключена. В машине плату нужно питать через пин 2 разъёма, не
 *     через Type-C (диоды на плате не пустят 5 В обратно в USB компьютера).
 *
 *  3. GPIO0 — strapping-пин и кнопка BOOT. Кнопка 1, зажатая в момент включения,
 *     переводит плату в режим прошивки. На плате есть подтяжка 10 кОм.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_lcd_types.h"
#include "esp_lcd_touch.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

#if RG_BOARD_1_28
// ==================================================================
// Waveshare ESP32-S3-Touch-LCD-1.28 (ESP32-S3R2, 16 MB flash, 2 MB quad PSRAM)
// Schematic Rev3 (roundDash repo: docs/hw/ESP32-S3-Touch-LCD-1.28-Sch.pdf).
//
// External connector P2 "GPIO_OUT" (SH1.0 12-pin):
//   1 GND   2 VSYS (5 V in)   3 RESET   4 BOOT (GPIO0)   5 GND   6 3V3
//   7 GPIO15   8 GPIO16   9 GPIO17 (CAN TX)   10 GPIO18 (CAN RX)   11 GPIO21   12 GPIO33
//
// Type-C goes through a CH343P to UART0 (GPIO43/44), as on the 2.1. No IO expander,
// no microSD, no buzzer, no RTC.
// ==================================================================
#define RG_BOARD_NAME "ESP32-S3-Touch-LCD-1.28"
#define RG_BOARD_RGB_PANEL 0
#define RG_BOARD_HAS_SD 0
#define RG_BOARD_HAS_BUZZER 0

// P2: CAN via an external SN65HVD230 (TX -> CTX, RX <- CRX)
#define RG_PIN_CAN_TX GPIO_NUM_17 /* P2 pin 9 */
#define RG_PIN_CAN_RX GPIO_NUM_18 /* P2 pin 10 */

// Button 1 = the on-board BOOT key (GPIO0, 10 k pull-up on board). No button 2.
#define RG_PIN_BTN1 GPIO_NUM_0
#define RG_PIN_BTN2 GPIO_NUM_NC

// On-board I2C: touch CST816S and IMU QMI8658A
#define RG_PIN_I2C_SDA GPIO_NUM_6
#define RG_PIN_I2C_SCL GPIO_NUM_7
#define RG_I2C_ADDR_TOUCH 0x15 /* CST816S */
#define RG_I2C_ADDR_IMU 0x6B   /* QMI8658A; confirm on hardware */

// Display GC9A01A, 4-wire SPI, RGB565, 240x240
#define RG_PIN_LCD_DC GPIO_NUM_8
#define RG_PIN_LCD_CS GPIO_NUM_9
#define RG_PIN_LCD_CLK GPIO_NUM_10
#define RG_PIN_LCD_MOSI GPIO_NUM_11
#define RG_PIN_LCD_MISO GPIO_NUM_12 /* not used: the panel is write-only here */
#define RG_PIN_LCD_RST GPIO_NUM_14
#define RG_PIN_LCD_BL GPIO_NUM_2 /* via N-FET Q1, active high */

#define RG_LCD_H_RES 240
#define RG_LCD_V_RES 240
#define RG_LCD_MIRROR_X 0    /* orientation: confirm on hardware */
#define RG_LCD_MIRROR_Y 0
#define RG_LCD_DRAW_LINES 40 /* LVGL partial draw buffer height (x2, internal DMA RAM) */

#define RG_PIN_TOUCH_INT GPIO_NUM_5
#define RG_PIN_TOUCH_RST GPIO_NUM_13

#define RG_PIN_IMU_INT1 GPIO_NUM_4
#define RG_PIN_IMU_INT2 GPIO_NUM_3
#define RG_PIN_BAT_ADC GPIO_NUM_1 /* 200 k / 100 k divider */

#else // ===== Waveshare ESP32-S3-Touch-LCD-2.1 (upstream) =====
#define RG_BOARD_NAME "ESP32-S3-Touch-LCD-2.1"
#define RG_BOARD_RGB_PANEL 1
#define RG_BOARD_HAS_SD 1
#define RG_BOARD_HAS_BUZZER 1

// ------------------------------------------------------------------
// Разъём J9: CAN и кнопки
// ------------------------------------------------------------------
#define RG_PIN_CAN_TX GPIO_NUM_20 /* J9 пин 4 */
#define RG_PIN_CAN_RX GPIO_NUM_19 /* J9 пин 3 */

#define RG_PIN_BTN1 GPIO_NUM_0  /* J9 пин 12, strapping, параллельно BOOT */
#define RG_PIN_BTN2 GPIO_NUM_44 /* J9 пин 10, UART0 RX, через FSUSB42 */

// ------------------------------------------------------------------
// I2C платы: тач, TCA9554, RTC, IMU (выведен и на J9 пины 7/8)
// ------------------------------------------------------------------
#define RG_PIN_I2C_SCL GPIO_NUM_7
#define RG_PIN_I2C_SDA GPIO_NUM_15

// Занятые адреса по FAQ Waveshare: 0x15, 0x20, 0x51, 0x6B и 0x7E (чем занят
// последний, в FAQ не сказано). Внешние устройства на J9 — только на свободные.
#define RG_I2C_ADDR_TOUCH 0x15    /* CST820 */
#define RG_I2C_ADDR_EXPANDER 0x20 /* TCA9554 */
#define RG_I2C_ADDR_RTC 0x51      /* PCF85063 */
#define RG_I2C_ADDR_IMU 0x6B      /* QMI8658 */

// ------------------------------------------------------------------
// TCA9554: все 8 выводов заняты платой
// ------------------------------------------------------------------
#define RG_EXIO_LCD_RST 0   /* EXIO1 */
#define RG_EXIO_TP_RST 1    /* EXIO2 */
#define RG_EXIO_LCD_CS 2    /* EXIO3 */
#define RG_EXIO_SD_CS 3     /* EXIO4 */
#define RG_EXIO_IMU_INT2 4  /* EXIO5 */
#define RG_EXIO_IMU_INT1 5  /* EXIO6 */
#define RG_EXIO_RTC_INT 6   /* EXIO7 */
#define RG_EXIO_BUZZER 7    /* EXIO8, зуммер (roundGauge_board_buzzer_set) */

// ------------------------------------------------------------------
// Экран ST7701S, RGB565, 480×480
// ------------------------------------------------------------------
// 3-wire SPI для init-последовательности ST7701 (делит ножки с SD-картой).
#define RG_PIN_LCD_SPI_SDA GPIO_NUM_1
#define RG_PIN_LCD_SPI_SCL GPIO_NUM_2

#define RG_PIN_LCD_PCLK GPIO_NUM_41
#define RG_PIN_LCD_DE GPIO_NUM_40
#define RG_PIN_LCD_VSYNC GPIO_NUM_39
#define RG_PIN_LCD_HSYNC GPIO_NUM_38
#define RG_PIN_LCD_BL GPIO_NUM_6

// Шина данных в порядке esp_lcd (D0..D15 = B0..B4, G0..G5, R0..R4). На
// разъёме панели это B1-B5, G0-G5, R1-R5: R0/B0 панели не разведены.
#define RG_PIN_LCD_DATA                                      \
    {                                                        \
        GPIO_NUM_5, GPIO_NUM_45, GPIO_NUM_48, GPIO_NUM_47,   \
        GPIO_NUM_21, GPIO_NUM_14, GPIO_NUM_13, GPIO_NUM_12,  \
        GPIO_NUM_11, GPIO_NUM_10, GPIO_NUM_9, GPIO_NUM_46,   \
        GPIO_NUM_3, GPIO_NUM_8, GPIO_NUM_18, GPIO_NUM_17,    \
    }

#define RG_LCD_H_RES 480
#define RG_LCD_V_RES 480

#define RG_PIN_TOUCH_INT GPIO_NUM_16

// ------------------------------------------------------------------
// Прочее
// ------------------------------------------------------------------
#define RG_PIN_SD_MISO GPIO_NUM_42 /* MOSI/SCK — общие с RG_PIN_LCD_SPI_* */
#define RG_PIN_SD_MOSI RG_PIN_LCD_SPI_SDA
#define RG_PIN_SD_SCK RG_PIN_LCD_SPI_SCL
#define RG_PIN_BAT_ADC GPIO_NUM_4

#endif // RG_BOARD_1_28

// Инициализация кнопок, I2C, TCA9554, экрана и тача. Подсветка после неё
// выключена: её включает ui_task, когда первый кадр уже нарисован.
esp_err_t roundGauge_board_init(void);

// Сырое состояние кнопки, без подавления дребезга. idx: 0 — кнопка 1, 1 — кнопка 2.
bool roundGauge_board_btn_pressed(int idx);

// Панель RGB с двумя кадровыми буферами в PSRAM (для esp_lvgl_port).
esp_lcd_panel_handle_t roundGauge_board_lcd_panel(void);

// Panel IO (SPI) for esp_lvgl_port on SPI panels (1.28). NULL on the RGB board.
esp_lcd_panel_io_handle_t roundGauge_board_lcd_io(void);

// Тач CST820 (драйвер CST816S), прерывание на RG_PIN_TOUCH_INT. Без тача (RG_HAS_TOUCH=0)
// не инициализируется, возвращает NULL.
esp_lcd_touch_handle_t roundGauge_board_touch(void);

// Общая шина I2C платы (тач, TCA9554, RTC, IMU): устройства на ней добавляют сами пользователи.
i2c_master_bus_handle_t roundGauge_board_i2c(void);

// Яркость подсветки, 0-100 %.
esp_err_t roundGauge_board_backlight_set(uint8_t pct);

// Зуммер на выводе EXIO8 расширителя TCA9554: включить или выключить (простое включение, тонов нет;
// звучит ли непрерывно или нужны импульсы - зависит от зуммера: активный пищит сам).
esp_err_t roundGauge_board_buzzer_set(bool on);

// microSD в режиме SPI (SPI2). MOSI и SCK общие с 3-wire SPI экрана: он нужен только на старте и пока его CS
// (EXIO3) высокий, не реагирует на обмен с картой. CS карты - на расширителе (EXIO4), не на GPIO: пока карта
// смонтирована, вывод держится низким. Вызывать из одной задачи (logger_task): монтирование и размонтирование
// между собой не синхронизируются. Нужна FAT32-карта.
esp_err_t roundGauge_board_sd_mount(const char *mount_path);
void roundGauge_board_sd_unmount(void);

// Отвечает ли смонтированная карта (CMD13). false - карту вынули или она не смонтирована.
bool roundGauge_board_sd_present(void);

// Размер и свободное место файловой системы, КБ.
esp_err_t roundGauge_board_sd_space(uint64_t *total_kb, uint64_t *free_kb);

#ifdef __cplusplus
}
#endif
