/*
 * Board layer for the Waveshare ESP32-S3-Touch-LCD-1.28 (RG_BOARD=1_28).
 * Same API as board_2_1.c (board.h). Pins: board.h, from the Waveshare schematic Rev3.
 *
 * Differences from the 2.1:
 *  - GC9A01A over 4-wire SPI (SPI2), 240x240, instead of an ST7701S RGB panel.
 *    LVGL renders into small DMA buffers in internal RAM, not into full frame buffers.
 *  - No TCA9554: LCD and touch resets are plain GPIOs.
 *  - No microSD and no buzzer: those functions return ESP_ERR_NOT_SUPPORTED.
 *  - One button: the on-board BOOT key (GPIO0).
 */
#include "board.h"
#if RG_BOARD_1_28
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_lcd_gc9a01.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch_cst816s.h"
#include "esp_log.h"

static const char *TAG = "BOARD";

static i2c_master_bus_handle_t s_i2c_bus;
static esp_lcd_panel_io_handle_t s_lcd_io;
static esp_lcd_panel_handle_t s_lcd_panel;
static esp_lcd_touch_handle_t s_touch;

#define LCD_SPI_HOST SPI2_HOST
#define LCD_PCLK_HZ (80 * 1000 * 1000) /* Waveshare: "SPI rate can be increased to 80MHz" */
// Largest single transfer: one LVGL draw buffer (RG_LCD_DRAW_LINES lines of RGB565).
#define LCD_MAX_TRANSFER (RG_LCD_H_RES * RG_LCD_DRAW_LINES * 2)

#define BL_LEDC_TIMER LEDC_TIMER_0
#define BL_LEDC_CHANNEL LEDC_CHANNEL_0
#define BL_LEDC_FREQ_HZ 5000
#define BL_LEDC_RES LEDC_TIMER_10_BIT
#define BL_LEDC_DUTY_MAX ((1 << 10) - 1)

// ------------------------------------------------------------------

static esp_err_t buttons_init(void)
{
    // BOOT key: closes to GND, 10 k pull-up on the board.
    gpio_config_t btn = {
        .pin_bit_mask = 1ULL << RG_PIN_BTN1,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    return gpio_config(&btn);
}

static esp_err_t i2c_init(void)
{
    i2c_master_bus_config_t bus = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = RG_PIN_I2C_SDA,
        .scl_io_num = RG_PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    return i2c_new_master_bus(&bus, &s_i2c_bus);
}

static esp_err_t lcd_init(void)
{
    const spi_bus_config_t bus = {
        .sclk_io_num = RG_PIN_LCD_CLK,
        .mosi_io_num = RG_PIN_LCD_MOSI,
        .miso_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = LCD_MAX_TRANSFER,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(LCD_SPI_HOST, &bus, SPI_DMA_CH_AUTO), TAG, "spi bus");

    const esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = RG_PIN_LCD_CS,
        .dc_gpio_num = RG_PIN_LCD_DC,
        .spi_mode = 0,
        .pclk_hz = LCD_PCLK_HZ,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST, &io_config, &s_lcd_io),
                        TAG, "panel io");

    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = RG_PIN_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_gc9a01(s_lcd_io, &panel_config, &s_lcd_panel), TAG, "gc9a01");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_lcd_panel), TAG, "panel reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_lcd_panel), TAG, "panel init");
    // GC9A01A modules need inverted colour. Orientation: RG_LCD_MIRROR_* in board.h.
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(s_lcd_panel, true), TAG, "invert");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(s_lcd_panel, RG_LCD_MIRROR_X, RG_LCD_MIRROR_Y), TAG, "mirror");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_lcd_panel, true), TAG, "disp on");
    return ESP_OK;
}

#if RG_HAS_TOUCH
static esp_err_t touch_init(void)
{
    esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
    esp_lcd_panel_io_handle_t io = NULL;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(s_i2c_bus, &io_config, &io), TAG, "touch io");

    // Interrupt-driven, as on the 2.1: the CST816S sleeps without a touch and stops answering I2C.
    esp_lcd_touch_config_t tp_config = {
        .x_max = RG_LCD_H_RES,
        .y_max = RG_LCD_V_RES,
        .rst_gpio_num = RG_PIN_TOUCH_RST,
        .int_gpio_num = RG_PIN_TOUCH_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0, // CST816S pulses INT low on a touch
        },
        .flags = {
            .mirror_x = RG_TOUCH_MIRROR_X,
            .mirror_y = RG_TOUCH_MIRROR_Y,
        },
    };
    return esp_lcd_touch_new_i2c_cst816s(io, &tp_config, &s_touch);
}
#endif // RG_HAS_TOUCH

static esp_err_t backlight_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = BL_LEDC_RES,
        .timer_num = BL_LEDC_TIMER,
        .freq_hz = BL_LEDC_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), TAG, "bl timer");

    ledc_channel_config_t channel = {
        .gpio_num = RG_PIN_LCD_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = BL_LEDC_CHANNEL,
        .timer_sel = BL_LEDC_TIMER,
        .duty = 0, // off until the first frame
    };
    return ledc_channel_config(&channel);
}

// ------------------------------------------------------------------

esp_err_t roundGauge_board_init(void)
{
    ESP_RETURN_ON_ERROR(backlight_init(), TAG, "backlight");
    ESP_RETURN_ON_ERROR(buttons_init(), TAG, "buttons");
    ESP_RETURN_ON_ERROR(i2c_init(), TAG, "i2c");
    ESP_RETURN_ON_ERROR(lcd_init(), TAG, "lcd");
#if RG_HAS_TOUCH
    ESP_RETURN_ON_ERROR(touch_init(), TAG, "touch");
#endif
    ESP_LOGI(TAG, "init done (%s)", RG_BOARD_NAME);
    return ESP_OK;
}

bool roundGauge_board_btn_pressed(int idx)
{
    return idx == 0 && gpio_get_level(RG_PIN_BTN1) == 0;
}

esp_lcd_panel_handle_t roundGauge_board_lcd_panel(void)
{
    return s_lcd_panel;
}

esp_lcd_panel_io_handle_t roundGauge_board_lcd_io(void)
{
    return s_lcd_io;
}

esp_lcd_touch_handle_t roundGauge_board_touch(void)
{
    return s_touch;
}

i2c_master_bus_handle_t roundGauge_board_i2c(void)
{
    return s_i2c_bus;
}

esp_err_t roundGauge_board_backlight_set(uint8_t pct)
{
    if (pct > 100) {
        pct = 100;
    }
    uint32_t duty = (uint32_t)BL_LEDC_DUTY_MAX * pct / 100;
    ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE, BL_LEDC_CHANNEL, duty), TAG, "bl duty");
    return ledc_update_duty(LEDC_LOW_SPEED_MODE, BL_LEDC_CHANNEL);
}

// No buzzer and no microSD on this board.
esp_err_t roundGauge_board_buzzer_set(bool on)
{
    (void)on;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t roundGauge_board_sd_mount(const char *mount_path)
{
    (void)mount_path;
    return ESP_ERR_NOT_SUPPORTED;
}

void roundGauge_board_sd_unmount(void)
{
}

bool roundGauge_board_sd_present(void)
{
    return false;
}

esp_err_t roundGauge_board_sd_space(uint64_t *total_kb, uint64_t *free_kb)
{
    (void)total_kb;
    (void)free_kb;
    return ESP_ERR_NOT_SUPPORTED;
}

#endif // RG_BOARD_1_28
