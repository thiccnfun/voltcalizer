#ifndef AudioAnalyzer_h
#define AudioAnalyzer_h

/**
 *   ESP32 SvelteKit
 *
 *   A simple, secure and extensible framework for IoT projects for ESP32 platforms
 *   with responsive Sveltekit front-end built with TailwindCSS and DaisyUI.
 *   https://github.com/theelims/ESP32-sveltekit
 *
 *   Copyright (C) 2018 - 2023 rjwats
 *   Copyright (C) 2023 theelims
 *
 *   All Rights Reserved. This software may be modified and distributed under
 *   the terms of the LGPL v3 license. See the LICENSE file for details.
 **/

#include <driver/i2s.h>
#include <cmath>
#include <HardwareSerial.h>
#include <AudioAnalysis.h>
#include <ArduinoFFT.h>

const int SAMPLES_SHORT = 1024;

// Data we push to 'samplesQueue'
struct sum_queue_t {
  // Sum of squares of mic samples, after Equalizer filter
  float sum_sqr_SPL;
  // Sum of squares of weighted mic samples
  float sum_sqr_weighted;
  // Debug only, FreeRTOS ticks we spent processing the I2S data
//   uint32_t proc_ticks;

  float pitch;
};

class AudioAnalyzer
{
public:
    AudioAnalyzer();
    void initializeI2s();
    void begin();
    QueueHandle_t samplesQueue;
    double getDecibels(sum_queue_t q);
    float calculatePitch();

protected:
    void task();
    static void _taskRunner(void *_this) { static_cast<AudioAnalyzer *>(_this)->task(); }
    float samples[SAMPLES_SHORT] __attribute__((aligned(4)));
    int32_t* intSamples = new int32_t[SAMPLES_SHORT];


private:
    uint32_t Leq_samples = 0;
    double Leq_sum_sqr = 0;
    unsigned long startTime = millis();
    ArduinoFFT<float> *_fft;
    AudioAnalysis _audioInfo;
};

#endif
