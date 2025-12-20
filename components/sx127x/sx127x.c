// Stolen from https://github.com/nopnop2002/esp-idf-sx127x

#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "freertos/task.h"
#include <string.h>

#define REG_FIFO            0x00
#define REG_OP_MODE         0x01
#define REG_BITRATE_MSB     0x02
#define REG_BITRATE_LSB     0x03
#define REG_FRF_MSB         0x06
#define REG_FRF_MID         0x07
#define REG_FRF_LSB         0x08
#define REG_LNA             0x0C
#define REG_RX_CONFIG       0x0D
#define REG_RSSI_VALUE      0x11
#define REG_RX_BW           0x12
#define REG_AFC_BW          0x13
#define REG_AFC_MSB         0x1b
#define REG_AFC_LSB         0x1C
#define REG_PREAMBLE_DETECT 0x1F
#define REG_SYNC_CONFIG     0x27
#define REG_SYNC_VALUE1     0x28
#define REG_SYNC_VALUE2     0x29
#define REG_SYNC_VALUE3     0x2A
#define REG_SYNC_VALUE4     0x2B
#define REG_SYNC_VALUE5     0x2C
#define REG_SYNC_VALUE6     0x2D
#define REG_SYNC_VALUE7     0x2E
#define REG_SYNC_VALUE8     0x2F
#define REG_PACKET_CONFIG1  0x30
#define REG_PACKET_CONFIG2  0x31
#define REG_PAYLOAD_LENGTH  0x32
#define REG_IRQ_FLAGS_1     0x3E
#define REG_IRQ_FLAGS_2     0x3F
#define REG_VERSION         0x42
#define REG_PLL_HOP         0x44
#define REG_BIT_RATE_FRAC   0x5D

#define TIMEOUT_RESET          100

#define SX127x_CRYSTAL_FREQ 32000000
#define SX127x_FSTEP (SX127x_CRYSTAL_FREQ*1.0/(1<<19))

#define OP_FSK_SLEEP 0x00
#define OP_FSK_STANDBY 0x01
#define OP_FSK_TX 0x03
#define OP_FSK_RX 0x05

#define TAG "SX127x"

static spi_device_handle_t _sx127x_spi;

uint8_t sx127x_read_reg(int reg) {
    uint8_t out[2] = {reg, 0xff};
    uint8_t in[2];

    spi_transaction_t t = {
        .flags = 0,
        .length = 8 * sizeof(out),
        .tx_buffer = out,
        .rx_buffer = in
    };

    spi_device_transmit(_sx127x_spi, &t);

    return in[1];
}

void sx127x_write_reg(uint8_t reg, uint8_t val) {
    uint8_t out[2] = {0x80 | reg, val};
    uint8_t in[2];

    spi_transaction_t t = {
        .flags = 0,
        .length = 8 * sizeof(out),
        .tx_buffer = out,
        .rx_buffer = in
    };

    spi_device_transmit(_sx127x_spi, &t);
}

void fsk_reset(void) {
    gpio_set_level(CONFIG_SX_RST_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(1));
    gpio_set_level(CONFIG_SX_RST_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
}

int sx127x_init(void) {
    esp_err_t ret;

    gpio_reset_pin(CONFIG_SX_RST_GPIO);
    gpio_set_direction(CONFIG_SX_RST_GPIO, GPIO_MODE_OUTPUT);
    gpio_reset_pin(CONFIG_SX_CS_GPIO);
    gpio_set_direction(CONFIG_SX_CS_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(CONFIG_SX_CS_GPIO, 1);

    spi_bus_config_t bus = {
        .miso_io_num = CONFIG_SX_MISO_GPIO,
        .mosi_io_num = CONFIG_SX_MOSI_GPIO,
        .sclk_io_num = CONFIG_SX_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 0
    };

    ret = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
    assert(ret == ESP_OK);

    spi_device_interface_config_t dev = {
        .clock_speed_hz = 9000000,
        .mode = 0,
        .spics_io_num = CONFIG_SX_CS_GPIO,
        .queue_size = 7,
        .flags = 0,
        .pre_cb = NULL
    };

    ret = spi_bus_add_device(SPI2_HOST, &dev, &_sx127x_spi);
    assert(ret == ESP_OK);

    fsk_reset();

    uint8_t version;
    uint8_t i = 0;
    while (i++ < TIMEOUT_RESET) {
        version = sx127x_read_reg(REG_VERSION);
        ESP_LOGD(TAG, "version=0x%02x", version);
        if (version == 0x12) break;
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    ESP_LOGD(TAG, "i=%d, TIMEOUT_RESET=%d", i, TIMEOUT_RESET);

    if (i == TIMEOUT_RESET + 1)
        return -1; // Illegal version

    return 0;
}

void sx127x_dump_registers(void) {
    printf("00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n");
    for (int i = 0; i < 0x40; i++) {
        printf("%02X ", sx127x_read_reg(i));
        if ((i & 0x0f) == 0x0f)
            printf("\n");
    }
    printf("\n");
}

static int gaintab[] = {-999, 0, -6, -12, -24, -36, -48, -999};
int fsk_get_lna_gain(void) {
    int gain = (sx127x_read_reg(REG_LNA) >> 5) & 0x7;
    return gaintab[gain];
}

void fsk_set_lna_gain(int gain) {
    uint8_t g = 1;
    while (gain < gaintab[g] && g < 6) g++;
    sx127x_write_reg(REG_LNA, g << 5);
}

int fsk_set_fsk() {
    sx127x_write_reg(REG_OP_MODE, OP_FSK_SLEEP);
    sx127x_write_reg(REG_OP_MODE, OP_FSK_SLEEP);
    sx127x_write_reg(REG_OP_MODE, OP_FSK_STANDBY);

    vTaskDelay(pdMS_TO_TICKS(100));

    if (sx127x_read_reg(REG_OP_MODE) == OP_FSK_STANDBY)
        return 0; // FSK
    return -1;
}

void fsk_enable_rx() {
    sx127x_write_reg(REG_OP_MODE, OP_FSK_RX);
}

uint8_t fsk_get_irq_flag_1(void) {
    return sx127x_read_reg(REG_IRQ_FLAGS_1);
}

uint8_t fsk_get_irq_flag_2(void) {
    return sx127x_read_reg(REG_IRQ_FLAGS_2);
}

uint8_t fsk_get_fifo(void) {
    return sx127x_read_reg(REG_FIFO);
}

float fsk_get_rssi() {
    /* Average RSSI
    float total_rssi = 0;
    int total = 1;

    for (int i = 0; i < total; i++) {
        total_rssi += sx127x_read_reg(REG_RSSI_VALUE);
    }
    return total_rssi / total / 2.f;
    */
    return sx127x_read_reg(REG_RSSI_VALUE) / 2.f;
}

int16_t fsk_get_raw_afc() {
    return (int16_t) ((sx127x_read_reg(REG_AFC_MSB) << 8) | sx127x_read_reg(REG_AFC_LSB));
}

void fsk_set_raw_afc(uint16_t afc) {
    sx127x_write_reg(REG_AFC_MSB, afc >> 8);
    sx127x_write_reg(REG_AFC_LSB, afc & 0xFF);
}

float fsk_get_afc() {
    return fsk_get_raw_afc() * SX127x_FSTEP;
}

void fsk_clear_irq_flags() {
    sx127x_write_reg(REG_IRQ_FLAGS_1, 0xFF);
    sx127x_write_reg(REG_IRQ_FLAGS_2, 0xFF);
}

void fsk_set_frequency(float freq) {
    sx127x_write_reg(REG_OP_MODE, OP_FSK_STANDBY);

    uint32_t frf = freq * 1.0 * (1 << 19) / SX127x_CRYSTAL_FREQ;
    sx127x_write_reg(REG_FRF_MSB, (frf & 0xff0000) >> 16);
    sx127x_write_reg(REG_FRF_MID, (frf & 0x00ff00) >> 8);
    sx127x_write_reg(REG_FRF_LSB, (frf & 0x0000ff));
}

float fsk_get_frequency() {
    uint8_t fmsb = sx127x_read_reg(REG_FRF_MSB);
    uint8_t fmid = sx127x_read_reg(REG_FRF_MID);
    uint8_t flsb = sx127x_read_reg(REG_FRF_LSB);
    return ((fmsb << 16) | (fmid << 8) | flsb) * SX127x_FSTEP;
}

int fsk_set_bitrate(float bps) {
    if (bps < 1200 || bps > 300000)
        return -1;

    sx127x_write_reg(REG_OP_MODE, OP_FSK_STANDBY);

    uint16_t bitRate = (SX127x_CRYSTAL_FREQ * 1.0) / bps;
    sx127x_write_reg(REG_BITRATE_MSB, (bitRate & 0xFF00) >> 8);
    sx127x_write_reg(REG_BITRATE_LSB, (bitRate & 0x00FF));

    uint16_t fracRate = (SX127x_CRYSTAL_FREQ * 16.0) / bps - bitRate * 16 + 0.5;
    sx127x_write_reg(REG_BIT_RATE_FRAC, fracRate & 0x0F);
    return 0;
}

float fsk_get_bitrate() {
    uint8_t fmsb = sx127x_read_reg(REG_BITRATE_MSB);
    uint8_t flsb = sx127x_read_reg(REG_BITRATE_LSB);
    uint8_t ffrac = sx127x_read_reg(REG_BIT_RATE_FRAC) & 0x0F;
    return SX127x_CRYSTAL_FREQ / ((fmsb << 8) + flsb + ffrac / 16.0);
}

int fsk_set_rx_bandwidth(float bw) {
    if (bw < 2600 || bw > 250000)
        return -1;

    uint8_t rx_bw_exp = 1;
    bw = SX127x_CRYSTAL_FREQ / bw / 8;
    while (bw > 31) {
        rx_bw_exp++;
        bw /= 2.0;
    }
    uint8_t rx_bw_mant = bw < 17 ? 0 : bw < 21 ? 1 : 2;

    sx127x_write_reg(REG_OP_MODE, OP_FSK_STANDBY);
    sx127x_write_reg(REG_RX_BW, (rx_bw_mant << 3) | rx_bw_exp);

    return 0;
}

float fsk_get_rx_bandwidth() {
    uint8_t rx_bw = sx127x_read_reg(REG_RX_BW);
    uint8_t rx_bw_exp = rx_bw & 0x07;
    uint8_t rx_bw_mant = (rx_bw >> 3) & 0x03;
    rx_bw_mant = 16 + 4 * rx_bw_mant;
    return (float)SX127x_CRYSTAL_FREQ / (rx_bw_mant << (rx_bw_exp + 2));
}

int fsk_set_afc_bandwidth(float bw) {
    if (bw < 2600 || bw > 250000)
        return -1;

    uint8_t afc_bw_exp = 1;
    bw = SX127x_CRYSTAL_FREQ / bw / 8;
    while (bw > 31) {
        afc_bw_exp++;
        bw /= 2.0;
    }
    uint8_t afc_bw_mant = bw < 17 ? 0 : bw < 21 ? 1 : 2;

    sx127x_write_reg(REG_OP_MODE, OP_FSK_STANDBY);
    sx127x_write_reg(REG_AFC_BW, (afc_bw_mant << 3) | afc_bw_exp);

    return 0;
}

float fsk_get_afc_bandwidth() {
    uint8_t afc_bw = sx127x_read_reg(REG_AFC_BW);
    uint8_t afc_bw_exp = afc_bw & 0x07;
    uint8_t afc_bw_mant = (afc_bw >> 3) & 0x03;
    afc_bw_mant = 16 + 4 * afc_bw_mant;

    return (float)SX127x_CRYSTAL_FREQ / (afc_bw_mant << (afc_bw_exp + 2));
}

void fsk_set_payload_length(int len) {
    uint8_t conf2 = sx127x_read_reg(REG_PACKET_CONFIG2);
    conf2 = (conf2 & 0xF8) | ((len >> 8) & 0x7);
    sx127x_write_reg(REG_PACKET_CONFIG2, conf2);
    sx127x_write_reg(REG_PAYLOAD_LENGTH, len & 0xFF);
}

void fsk_set_packet_config(uint8_t conf1, uint8_t conf2) {
    sx127x_write_reg(REG_PACKET_CONFIG1, conf1);
    sx127x_write_reg(REG_PACKET_CONFIG2, conf2);
};

uint8_t fsk_get_rx_conf() {
    return sx127x_read_reg(REG_RX_CONFIG);
}

void fsk_set_rx_conf(uint8_t conf) {
    sx127x_write_reg(REG_RX_CONFIG, conf);
}

int fsk_set_sync_conf(uint8_t conf, int len, const uint8_t *syncpattern) {
    sx127x_write_reg(REG_SYNC_CONFIG, conf);
    if (len > 8)
        return -1;
    for (int i = 0; i < len; i++) {
        sx127x_write_reg(REG_SYNC_VALUE1 + i, syncpattern[i]);
    }
    return 0;
}

uint8_t fsk_get_sync_conf() {
    return sx127x_read_reg(REG_SYNC_CONFIG);
}

void fsk_set_preamble_detect(uint8_t conf) {
    sx127x_write_reg(REG_PREAMBLE_DETECT, conf);
}

uint8_t fsk_get_preamble_detect() {
    return sx127x_read_reg(REG_PREAMBLE_DETECT);
}

void fsk_set_fasthop(uint8_t s) {
    sx127x_write_reg(REG_PLL_HOP, s);
}
