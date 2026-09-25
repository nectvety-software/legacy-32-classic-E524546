#pragma once
#include "Arduino.h"
typedef int esp_err_t; static const esp_err_t ESP_OK=0;
typedef int i2s_mode_t; typedef int i2s_bits_per_sample_t;
static const int I2S_MODE_MASTER=1,I2S_MODE_TX=2,I2S_BITS_PER_SAMPLE_16BIT=16,I2S_CHANNEL_FMT_RIGHT_LEFT=0,I2S_COMM_FORMAT_STAND_I2S=0,I2S_NUM_0=0,I2S_PIN_NO_CHANGE=-1,I2S_CHANNEL_STEREO=2,ESP_INTR_FLAG_LEVEL1=1;
struct i2s_config_t{ i2s_mode_t mode; int sample_rate; int bits_per_sample; int channel_format; int communication_format; int intr_alloc_flags; int dma_buf_count; int dma_buf_len; bool use_apll; bool tx_desc_auto_clear; int fixed_mclk;};
struct i2s_pin_config_t{int bck_io_num;int ws_io_num;int data_out_num;int data_in_num;};
inline esp_err_t i2s_driver_install(int,const i2s_config_t*,int,void*){return ESP_OK;} inline esp_err_t i2s_set_pin(int,const i2s_pin_config_t*){return ESP_OK;} inline void i2s_zero_dma_buffer(int){} inline esp_err_t i2s_set_clk(int,uint32_t,int,int){return ESP_OK;} inline esp_err_t i2s_write(int,const void*,size_t n,size_t*w,int){if(w)*w=n;return ESP_OK;}
