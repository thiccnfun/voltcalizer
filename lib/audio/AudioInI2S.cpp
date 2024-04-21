#include "AudioInI2S.h"

AudioInI2S::AudioInI2S(int bck_pin, int ws_pin, int data_pin, int channel_pin, i2s_channel_fmt_t channel_format)
{
  _bck_pin = bck_pin;
  _ws_pin = ws_pin;
  _data_pin = data_pin;
  _channel_pin = channel_pin;
  _channel_format = channel_format;
}

void AudioInI2S::begin(int sample_size, int sample_rate, i2s_port_t i2s_port_number)
{
  if (_channel_pin >= 0)
  {
    pinMode(_channel_pin, OUTPUT);
    digitalWrite(_channel_pin, _channel_format == I2S_CHANNEL_FMT_ONLY_RIGHT ? LOW : HIGH);
  }

  _sample_rate = sample_rate;
  _sample_size = sample_size;
  _i2s_port_number = i2s_port_number;

  _i2s_mic_pins.bck_io_num = _bck_pin;
  _i2s_mic_pins.ws_io_num = _ws_pin;
  _i2s_mic_pins.data_in_num = _data_pin;

  _i2s_config.sample_rate = _sample_rate;
  _i2s_config.dma_buf_len = _sample_size;
  _i2s_config.channel_format = _channel_format;

  // start up the I2S peripheral
  i2s_driver_install(_i2s_port_number, &_i2s_config, 0, NULL);
  i2s_set_pin(_i2s_port_number, &_i2s_mic_pins);
}

void AudioInI2S::read(int32_t _samples[])
{
  // read I2S stream data into the samples buffer
  size_t bytes_read = 0;
  i2s_read(_i2s_port_number, _samples, sizeof(int32_t) * _sample_size, &bytes_read, portMAX_DELAY);
  int samples_read = bytes_read / sizeof(int32_t);
}