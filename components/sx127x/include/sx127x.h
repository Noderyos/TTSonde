// Stolen from https://github.com/nopnop2002/esp-idf-sx127x

#ifndef __LORA_H__
#define __LORA_H__

void fsk_reset(void);
int sx127x_init(void);

void sx127x_dump_registers(void);

int fsk_get_lna_gain(void);
void fsk_set_lna_gain(int gain);

int fsk_set_fsk();
void fsk_enable_rx();

void fsk_clear_irq_flags();
uint8_t fsk_get_irq_flag_1(void);
uint8_t fsk_get_irq_flag_2(void);
uint8_t fsk_get_fifo(void);

float fsk_get_rssi();
int16_t fsk_get_raw_afc();
void fsk_set_raw_afc(uint16_t afc);
float fsk_get_afc();

void fsk_set_frequency(float freq);
float fsk_get_frequency();

int fsk_set_bitrate(float bps);
float fsk_get_bitrate();

int fsk_set_rx_bandwidth(float bw);
float fsk_get_rx_bandwidth();

int fsk_set_afc_bandwidth(float bw);
float fsk_get_afc_bandwidth();

void fsk_set_payload_length(int len);
void fsk_set_packet_config(uint8_t conf1, uint8_t conf2);

void fsk_set_rx_conf(uint8_t conf);
uint8_t fsk_get_rx_conf();

int fsk_set_sync_conf(uint8_t conf, int len, const uint8_t *syncpattern);
uint8_t fsk_get_sync_conf();

void fsk_set_preamble_detect(uint8_t conf);
uint8_t fsk_get_preamble_detect();

void fsk_set_fasthop(uint8_t s);

#endif