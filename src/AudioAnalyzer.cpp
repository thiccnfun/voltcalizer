#include <AudioAnalyzer.h>
#include <filters.h>



//
// Configuration
//

#define LEQ_PERIOD        0.25           // second(s)
#define WEIGHTING         A_weighting // Also avaliable: 'C_weighting' or 'None' (Z_weighting)
#define LEQ_UNITS         "LAeq"      // customize based on above weighting used
#define DB_UNITS          "dBA"       // customize based on above weighting used

// NOTE: Some microphones require at least DC-Blocker filter
#define MIC_EQUALIZER     None    // See below for defined IIR filters or set to 'None' to disable
#define MIC_OFFSET_DB     2.0103      // Default offset (sine-wave RMS vs. dBFS). Modify this value for linear calibration

// Customize these values from microphone datasheet
#define MIC_SENSITIVITY   -29         // dBFS value expected at MIC_REF_DB (Sensitivity value from datasheet)
#define MIC_REF_DB        94.0        // Value at which point sensitivity is specified in datasheet (dB)
#define MIC_OVERLOAD_DB   116.0       // dB - Acoustic overload point
#define MIC_NOISE_DB      29          // dB - Noise floor
#define MIC_BITS          24          // valid number of bits in I2S data
#define MIC_CONVERT(s)    (s >> (SAMPLE_BITS - MIC_BITS))
#define MIC_TIMING_SHIFT  0           // Set to one to fix MSB timing for some microphones, i.e. SPH0645LM4H-x

// Calculate reference amplitude value at compile time
double MIC_REF_AMPL = pow(10, double(MIC_SENSITIVITY)/20) * ((1<<(MIC_BITS-1))-1);

//
// I2S pins - Can be routed to almost any (unused) ESP32 pin.
//            SD can be any pin, inlcuding input only pins (36-39).
//            SCK (i.e. BCLK) and WS (i.e. L/R CLK) must be output capable pins
//
// Below ones are just example for my board layout, put here the pins you will use
//
#define I2S_WS            15  
#define I2S_SCK           14 
#define I2S_SD            39 

#define PIEZO_PIN         35
#define RF_PIN            21


// I2S peripheral to use (0 or 1)
#define I2S_PORT          I2S_NUM_0


//
// Sampling
//
#define SAMPLE_RATE       16000 // Hz, fixed to design of IIR filters
#define SAMPLE_BITS       32    // bits
#define SAMPLE_T          int32_t 
// #define SAMPLES_SHORT     1024 // ~125ms
#define SAMPLES_LEQ       (SAMPLE_RATE * LEQ_PERIOD)
#define DMA_BANK_SIZE     (SAMPLES_SHORT / 16)
#define DMA_BANKS         8

// const double signalFrequency = 1000;
const double samplingFrequency = 5000;
const uint8_t amplitude = 100;

#define I2S_TASK_STACK 2048


AudioAnalyzer::AudioAnalyzer() {
  samplesQueue = xQueueCreate(1, sizeof(sum_queue_t));
}

void AudioAnalyzer::initializeI2s() {
  // Setup I2S to sample mono channel for SAMPLE_RATE * SAMPLE_BITS
  // NOTE: Recent update to Arduino_esp32 (1.0.2 -> 1.0.3)
  //       seems to have swapped ONLY_LEFT and ONLY_RIGHT channels
  const i2s_config_t i2s_config = {
      mode: i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
      sample_rate: SAMPLE_RATE,
      bits_per_sample: i2s_bits_per_sample_t(SAMPLE_BITS),
      channel_format: I2S_CHANNEL_FMT_ONLY_RIGHT,
      communication_format: i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
      intr_alloc_flags: ESP_INTR_FLAG_LEVEL1,
      dma_buf_count: DMA_BANKS,
      dma_buf_len: DMA_BANK_SIZE,
      use_apll: true,
      tx_desc_auto_clear: false,
      fixed_mclk: 0
  };
  // I2S pin mapping
  const i2s_pin_config_t pin_config = {
      bck_io_num:   I2S_SCK,  
      ws_io_num:    I2S_WS,    
      data_out_num: -1, // not used
      data_in_num:  I2S_SD   
  };

  esp_err_t err;
  err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
      Serial.printf("Failed installing driver: %d\n", err);
      while (true);
  }

  #if (MIC_TIMING_SHIFT > 0) 
      // Undocumented (?!) manipulation of I2S peripheral registers
      // to fix MSB timing issues with some I2S microphones
      REG_SET_BIT(I2S_TIMING_REG(I2S_PORT), BIT(9));   
      REG_SET_BIT(I2S_CONF_REG(I2S_PORT), I2S_RX_MSB_SHIFT);  
  #endif
  
  err = i2s_set_pin(I2S_PORT, &pin_config);
  if (err != ESP_OK) {
      Serial.printf("Failed setting pin: %d\n", err);
      while (true);
  }
  Serial.println("I2S driver installed.");

}

void AudioAnalyzer::begin() {
  initializeI2s();

  xTaskCreatePinnedToCore(
    this->_taskRunner,            // Function that should be called
    "AudioAnalyzer",             // Name of the task (for debugging)
    I2S_TASK_STACK,                       // Stack size (bytes)
    this,                       // Pass reference to this class instance
    (tskIDLE_PRIORITY),         // task priority
    NULL,                       // Task handle
    ESP32SVELTEKIT_RUNNING_CORE // Pin to application core
  );
}

void AudioAnalyzer::task() {

  // Discard first block, microphone may have startup time (i.e. INMP441 up to 83ms)
  size_t bytes_read = 0;
  i2s_read(I2S_PORT, &samples, SAMPLES_SHORT * sizeof(int32_t), &bytes_read, portMAX_DELAY);

  while (true) {
    i2s_read(I2S_PORT, &samples, SAMPLES_SHORT * sizeof(SAMPLE_T), &bytes_read, portMAX_DELAY);

    TickType_t start_tick = xTaskGetTickCount();
    
    // Convert (including shifting) integer microphone values to floats, 
    // using the same buffer (assumed sample size is same as size of float), 
    // to save a bit of memory
    SAMPLE_T* int_samples = (SAMPLE_T*)&samples;
    for(int i=0; i<SAMPLES_SHORT; i++) {
      // intSamples[i] = static_cast<int32_t>(samples[i]);
      samples[i] = MIC_CONVERT(int_samples[i]);
    }

    sum_queue_t q;
    // Apply equalization and calculate Z-weighted sum of squares, 
    // writes filtered samples back to the same buffer.
    q.sum_sqr_SPL = MIC_EQUALIZER.filter(samples, samples, SAMPLES_SHORT);

    // Apply weighting and calucate weigthed sum of squares
    q.sum_sqr_weighted = WEIGHTING.filter(samples, samples, SAMPLES_SHORT);

    // Debug only. Ticks we spent filtering and summing block of I2S data
    // q.proc_ticks = xTaskGetTickCount() - start_tick;

    // Calculate pitch
    q.pitch = 0;
    // q.pitch = calculatePitch();

    // analogRead PIEZO_PIN
    // Serial.println(analogRead(PIEZO_PIN));

    // Send the sums to FreeRTOS queue where main task will pick them up
    // and further calcualte decibel values (division, logarithms, etc...)
    xQueueSend(samplesQueue, &q, portMAX_DELAY);
  }
}

double AudioAnalyzer::getDecibels(sum_queue_t q) {

  // Calculate dB values relative to MIC_REF_AMPL and adjust for microphone reference
  double short_RMS = sqrt(double(q.sum_sqr_SPL) / SAMPLES_SHORT);
  double short_SPL_dB = MIC_OFFSET_DB + MIC_REF_DB + 20 * log10(short_RMS / MIC_REF_AMPL);

  // In case of acoustic overload or below noise floor measurement, report infinty Leq value
  if (short_SPL_dB > MIC_OVERLOAD_DB) {
    Leq_sum_sqr = INFINITY;
  } else if (isnan(short_SPL_dB) || (short_SPL_dB < MIC_NOISE_DB)) {
    Serial.println("Noise");
    Leq_sum_sqr = -INFINITY;
  }

  // Accumulate Leq sum
  Leq_sum_sqr += q.sum_sqr_weighted;
  Leq_samples += SAMPLES_SHORT;

  double output = -1;
  if (Leq_samples >= SAMPLE_RATE * LEQ_PERIOD) {
    double Leq_RMS = sqrt(Leq_sum_sqr / Leq_samples);
    output = MIC_OFFSET_DB + MIC_REF_DB + 20 * log10(Leq_RMS / MIC_REF_AMPL);
    Leq_sum_sqr = 0;
    Leq_samples = 0;
  }

  return output;
}

float AudioAnalyzer::calculatePitch() {
  // int N = samples.size();
  // std::vector<double> fftInput(N);

  // // Convert samples to complex numbers for FFT
  // for (int i = 0; i < SAMPLES_SHORT; i++) {
  //   intSamples[i] = static_cast<int32_t>(samples[i]);
  // }

  // Serial.println("Calculating pitch...");

  _audioInfo.computeFFT(intSamples, SAMPLES_SHORT, SAMPLE_RATE);
  _audioInfo.computeFrequencies(8);

  // // float *bands = _audioInfo.getBands();
  float *peaks = _audioInfo.getPeaks();

    for (int i = 0; i < 8; i++)
  {
    // Serial.printf("%d:%.1f,", i, peaks[i]);
    Serial.printf(">peak%d:%.1f", i, peaks[i]);
    Serial.println();
  }

  return 0;
  // if (_fft == nullptr) {
  //   _fft = new ArduinoFFT<float>(_real, _imag, SAMPLES_SHORT, SAMPLE_RATE, _weighingFactors);
  // }

  // // prep samples for analysis
  // for (int i = 0; i < SAMPLES_SHORT; i++)
  // {
  //   _real[i] = samples[i];
  //   _imag[i] = 0;
  // }

  // _fft->dcRemoval();
  // _fft->windowing(FFTWindow::Hamming, FFTDirection::Forward, false); /* Weigh data (compensated) */
  // _fft->compute(FFTDirection::Forward);                              /* Compute FFT */
  // _fft->complexToMagnitude();                                        /* Compute magnitudes */





  // fft = arduinoFFT(vReal, vImag, SAMPLE_BITS, samplingFrequency); /* Create FFT object */

  // // Serial.println("Data:");
  // // printVector(vReal, SAMPLES_SHORT, SCL_TIME);
  // fft.Windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);	/* Weigh data */
  // // Serial.println("Weighed data:");
  // // printVector(vReal, SAMPLES_SHORT, SCL_TIME);
  // // fft.Compute(FFT_FORWARD); /* Compute FFT */
  // // Serial.println("Computed Real values:");
  // // printVector(vReal, SAMPLES_SHORT, SCL_INDEX);
  // // Serial.println("Computed Imaginary values:");
  // // printVector(vImag, SAMPLES_SHORT, SCL_INDEX);
  // // fft.ComplexToMagnitude(); /* Compute magnitudes */
  // // Serial.println("Computed magnitudes:");
  // // printVector(vReal, (SAMPLES_SHORT >> 1), SCL_FREQUENCY);
  // double pitch = fft.MajorPeak();
  // // Serial.println(x, 6); //Print out what frequency is the most dominant.

  // // return pitch as float
  // return static_cast<float>(pitch);

  // // // // Perform FFT
  // // // fft.Windowing(fftInput.data(), N, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  // // // fft.Compute(fftInput.data(), fftOutput.data(), N, FFT_FORWARD);
  // // // // fft.ComplexToMagnitude(fftOutput.data(), N);

  // // // Find the peak frequency (pitch)
  // // float maxMagnitude = 0.0f;
  // // float pitch = 0.0f;

  // // // for (int i = 1; i < N / 2; i++) {
  // // //   if (fftOutput[i] > maxMagnitude) {
  // // //     maxMagnitude = fftOutput[i];
  // // //     pitch = i * sampleRate / N;
  // // //   }
  // // // }

  // return pitch;
}