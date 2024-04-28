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

#include <MicStateService.h>

#include <ArduinoFFT.h>
#include <driver/i2s.h>
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
#define SAMPLES_SHORT     1024 // ~125ms
#define SAMPLES_LEQ       (SAMPLE_RATE * LEQ_PERIOD)
#define DMA_BANK_SIZE     (SAMPLES_SHORT / 16)
#define DMA_BANKS         8

// const double signalFrequency = 1000;
const double samplingFrequency = 5000;
const uint8_t amplitude = 100;


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

//
// I2S Reader Task
//
// Rationale for separate task reading I2S is that IIR filter
// processing cam be scheduled to different core on the ESP32
// while main task can do something else, like update the 
// display in the example
//
// As this is intended to run as separate hihg-priority task, 
// we only do the minimum required work with the I2S data
// until it is 'compressed' into sum of squares 
//
// FreeRTOS priority and stack size (in 32-bit words) 
#define I2S_TASK_PRI   4
#define I2S_TASK_STACK 2048


#define SCL_INDEX 0x00
#define SCL_TIME 0x01
#define SCL_FREQUENCY 0x02
#define SCL_PLOT 0x03

const char* TAG = "MicStateService";



MicStateService::MicStateService(
  PsychicHttpServer *server,
  SecurityManager *securityManager,
  PsychicMqttClient *mqttClient,
  AppSettingsService *appSettingsService) : 
    _httpEndpoint(
      MicState::read,
      MicState::update,
      this,
      server,
      MIC_STATE_ENDPOINT_PATH,
      securityManager,
      AuthenticationPredicates::IS_AUTHENTICATED
    ),
    _mqttPubSub(MicState::read, MicState::update, this, mqttClient),
    _webSocketServer(
      MicState::read,
      MicState::update,
      this,
      server,
      MIC_STATE_SOCKET_PATH,
      securityManager,
      AuthenticationPredicates::IS_AUTHENTICATED
    ),
    _mqttClient(mqttClient),
    _appSettingsService(appSettingsService)
{
}

void MicStateService::begin()
{
    _httpEndpoint.begin();
    _webSocketServer.begin();

    _evaluator = new Evaluator(_appSettingsService);
    _evaluator->begin();

    setupReader();
}

void MicStateService::setupReader() {
    
  // audio analysis setup
  _audioInfo.setNoiseFloor(10);       // sets the noise floor
  _audioInfo.normalize(true, 0, 255); // normalize all values to range provided.

  // _audioInfo.autoLevel(AudioAnalysis::ACCELERATE_FALLOFF, 1); // uncomment this line to set auto level falloff rate
  // _audioInfo.bandPeakFalloff(AudioAnalysis::EXPONENTIAL_FALLOFF, 0.05); // set the band peak fall off rate
  // _audioInfo.vuPeakFalloff(AudioAnalysis::EXPONENTIAL_FALLOFF, 0.05);    // set 

    // Create FreeRTOS queue
    samplesQueue = xQueueCreate(8, sizeof(sum_queue_t));

    xTaskCreatePinnedToCore(
        this->_readerTask,            // Function that should be called
        "Mic I2S Reader",       // Name of the task (for debugging)
        I2S_TASK_STACK,                       // Stack size (bytes)
        this,                       // Pass reference to this class instance
        (tskIDLE_PRIORITY),         // task priority
        NULL,                       // Task handle
        ESP32SVELTEKIT_RUNNING_CORE // Pin to application core
    );

    sum_queue_t q;
    uint32_t Leq_samples = 0;
    double Leq_sum_sqr = 0;
    double Leq_dB = 0;
    unsigned long startTime = millis();

    // 5 seconds in milliseconds
    int idleDuration = 5000; // how long to wait before starting the sequence
    // int eventDuration = 5000; // how long until the event ends
    int actDuration = 4000; // how long the user has to act before the event ends
    // int actWindow = 2000;
    double thresholdDb = 80;
    AlertType alertType = AlertType::NONE;
    int alertDuration = 1000;
    int alertStrength = 10;
    PassType passType = PassType::FIRST_PASS;

    // assignConditionValues(thresholdDb);
    // assignDurationValues(idleDuration, actDuration);
    assignRoutineConditionValues(
      thresholdDb, 
      idleDuration, 
      actDuration, 
      alertType, 
      alertDuration, 
      alertStrength,
      passType
    );
    // int sequenceDuration = actDuration;
    int eventCountdown = actDuration;

    unsigned long currentTime = millis();
    unsigned long elapsedTime = currentTime - startTime;

    bool resetConditions = false;
    int ticks = 0;
    int ticksPassed = 0;
    bool doEvaluation = false;
    float dbPassRate = 0;
    int alertTime = 1500; // 1 second plus a little buffer
    bool hasAlerted = false;
    bool doAlert = false;

    // Read sum of samaples, calculated by 'i2s_reader_task'
    while (xQueueReceive(samplesQueue, &q, portMAX_DELAY)) {

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

        // When we gather enough samples, calculate new Leq value
        if (Leq_samples >= SAMPLE_RATE * LEQ_PERIOD) {

            double Leq_RMS = sqrt(Leq_sum_sqr / Leq_samples);
            Leq_dB = MIC_OFFSET_DB + MIC_REF_DB + 20 * log10(Leq_RMS / MIC_REF_AMPL);
            Leq_sum_sqr = 0;
            Leq_samples = 0;

            // Serial.printf(">decibel:%.1f", Leq_dB);
            // Serial.println();
            
            // Serial output, customize (or remove) as needed
            // Serial.printf("%.1fdB\n", Leq_dB);
            currentTime = millis();
            elapsedTime = currentTime - startTime;  
            
            eventCountdown = actDuration - elapsedTime + idleDuration + (alertType == AlertType::NONE ? 0 : alertTime);
            
            if (_state.enabled) {

              // proceed to evaluation
              if (eventCountdown <= 0) {
                doEvaluation = true;
                resetConditions = true;

              // start evaluation window
              } else if (eventCountdown <= actDuration) {
                Serial.println("--------- ACTION WINDOW --------------");
                ticks += 1;

                if (_evaluator->evaluateConditions(Leq_dB, thresholdDb) == ConditionState::REACHED) {
                  ticksPassed += 1;

                  // if setup to stop on the first pass, proceed to evaluation
                  // otherwise, continue to accumulate ticks and evaluate at the end
                  if (passType == PassType::FIRST_PASS) {
                    dbPassRate = 1;
                    doEvaluation = true;
                    resetConditions = true;
                  }
                }

                if (!doEvaluation) {
                  dbPassRate = (float)ticksPassed / ticks;
                }

              // start alert window
              } else if (!hasAlerted && eventCountdown <= actDuration + alertTime) {
                hasAlerted = true;
                doAlert = true;
              }


            } else {
                startTime = currentTime;
            }

            // Update the state, emitting change event if value changed
            updateState(
              Leq_dB, 
              q.pitch, 
              elapsedTime <= (idleDuration + alertTime) ? -1 : eventCountdown,
              thresholdDb,
              dbPassRate
            );

            if (doEvaluation) {
              _evaluator->queueEvaluation(dbPassRate);
              doEvaluation = false;
            } else if (doAlert) {
              _evaluator->queueAlert(alertType, alertDuration, alertStrength);
              doAlert = false;
            }

            if (resetConditions) {
                resetConditions = false;

                // NOTE: maybe delay accordingly
                startTime = currentTime;

                assignRoutineConditionValues(
                  thresholdDb, 
                  idleDuration, 
                  actDuration, 
                  alertType, 
                  alertDuration, 
                  alertStrength,
                  passType
                );
                // sequenceDuration = actDuration;
                ticks = 0;
                ticksPassed = 0;
                dbPassRate = 0;
                hasAlerted = false;
            }
        }
    
        // Debug only
        //Serial.printf("%u processing ticks\n", q.proc_ticks);
    }
}

void MicStateService::initializeI2s() {
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

void MicStateService::readerTask() {
    initializeI2s();

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
          intSamples[i] = static_cast<int32_t>(samples[i]);
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

// Function to calculate the pitch using FFT
// float MicStateService::calculatePitch(const std::vector<float>& samples, int sampleRate) {
//     // int N = samples.size();
//     // std::vector<std::complex<float>> fftInput(N);

//     // // Convert samples to complex numbers for FFT
//     // for (int i = 0; i < N; i++) {
//     //     fftInput[i] = std::complex<float>(samples[i], 0.0f);
//     // }


//     // std::vector<std::complex<float>> fftOutput(N);
//     // std::vector<float> magnitudes(N);
//     // std::vector<float> frequencies(N);

    
//     // fft(fftInput, fftOutput);

//     // // Calculate magnitudes and frequencies
//     // for (int i = 0; i < N; i++) {
//     //   magnitudes[i] = std::abs(fftOutput[i]);
//     //   frequencies[i] = i * sampleRate / N;
//     // }



//     // 
    
        

//     // Find the peak frequency (pitch)
//     float maxMagnitude = 0.0f;
//     float pitch = 0.0f;

//     // for (int i = 1; i < N / 2; i++) {
//     //     if (magnitudes[i] > maxMagnitude) {
//     //         maxMagnitude = magnitudes[i];
//     //         pitch = frequencies[i];
//     //     }
//     // }

//     return pitch;
// }


// ...

// Function to calculate the pitch using FFT
float MicStateService::calculatePitch() {
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

void MicStateService::printVector(double *vData, uint16_t bufferSize, uint8_t scaleType)
{
  for (uint16_t i = 0; i < bufferSize; i++)
  {
        double abscissa;
        /* Print abscissa value */
        switch (scaleType)
        {
          case SCL_INDEX:
            abscissa = (i * 1.0);
    	break;
          case SCL_TIME:
            abscissa = ((i * 1.0) / samplingFrequency);
    	break;
          case SCL_FREQUENCY:
            abscissa = ((i * 1.0 * samplingFrequency) / static_cast<double>(SAMPLES_SHORT));
    	break;
        }
        Serial.print(abscissa, 6);
    if(scaleType==SCL_FREQUENCY)
      Serial.print("Hz");
    Serial.print(" ");
    Serial.println(vData[i], 4);
  }
  Serial.println();
}

// ...

// void MicStateService::fft(const std::vector<std::complex<float>>& input, std::vector<std::complex<float>>& output) {
//     int N = input.size();
//     if (N == 1) {
//         output = input;
//         return;
//     }

//     std::vector<std::complex<float>> even(N / 2);
//     std::vector<std::complex<float>> odd(N / 2);
//     for (int i = 0; i < N / 2; i++) {
//         even[i] = input[2 * i];
//         odd[i] = input[2 * i + 1];
//     }

//     std::vector<std::complex<float>> evenTransformed(N / 2);
//     std::vector<std::complex<float>> oddTransformed(N / 2);
//     fft(even, evenTransformed);
//     fft(odd, oddTransformed);

//     for (int i = 0; i < N / 2; i++) {
//         float angle = -2 * M_PI * i / N;
//         std::complex<float> twiddle = std::polar(1.0f, angle);
//         output[i] = evenTransformed[i] + twiddle * oddTransformed[i];
//         output[i + N / 2] = evenTransformed[i] - twiddle * oddTransformed[i];
//     }
// }

void MicStateService::updateState(
  float dbValue, 
  float pitchValue, 
  int eventCountdown,
  int thresholdDb,
  float dbPassRate
) {
  update([&](MicState& state) {
    if (state.dbValue == dbValue && state.eventCountdown == eventCountdown) {
      return StateUpdateResult::UNCHANGED;
    }
    state.dbValue = dbValue;
    state.dbThreshold = eventCountdown == -1 ? 0 : thresholdDb;
    state.dbPassRate = dbPassRate;
    state.pitchValue = pitchValue;
    state.eventCountdown = eventCountdown;
    return StateUpdateResult::CHANGED;
  }, "db_set");
}

void MicStateService::assignRoutineConditionValues(
    double &dbThreshold,
    int &idleDuration,
    int &actDuration,
    AlertType &alertType,
    int &alertDuration,
    int &alertStrength,
    PassType &passType
) {
  _appSettingsService->read([&](AppSettings &settings) {
    if (settings.decibelThresholdMax == settings.decibelThresholdMin) {
      dbThreshold = settings.decibelThresholdMin;
    } else {
      dbThreshold = random(settings.decibelThresholdMin, settings.decibelThresholdMax);
    }

    if (settings.actionPeriodMaxMs == settings.actionPeriodMinMs) {
      actDuration = settings.actionPeriodMinMs;
    } else {
      actDuration = random(settings.actionPeriodMinMs, settings.actionPeriodMaxMs);
    }

    if (settings.idlePeriodMaxMs == settings.idlePeriodMinMs) {
      idleDuration = settings.idlePeriodMinMs;
    } else {
      idleDuration = random(settings.idlePeriodMinMs, settings.idlePeriodMaxMs);
    }

    alertType = settings.alertType;
    alertDuration = settings.alertDuration;
    alertStrength = settings.alertStrength;
    passType = settings.passType;
  });
}

// void MicStateService::computeFrequencies(uint8_t bandSize)
// {
//   // TODO: use maths to calculate these offset values. Inputs being _sampleSize and _bandSize output being similar exponential curve below.
//   // band offsets helpers based on 1024 samples
//   const static uint16_t _2frequencyOffsets[2] = {24, 359};
//   const static uint16_t _4frequencyOffsets[4] = {6, 18, 72, 287};
//   const static uint16_t _8frequencyOffsets[8] = {2, 4, 6, 12, 25, 47, 92, 195};
//   const static uint16_t _16frequencyOffsets[16] = {1, 1, 2, 2, 2, 4, 5, 7, 11, 14, 19, 28, 38, 54, 75, 120}; // initial 
//   // 32 and 64 frequency offsets are low end biased because of int math... the first 4 and 8 buckets should be 0.5f but we cant do that here.
//   const static uint16_t _32frequencyOffsets[32] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 5, 5, 7, 7, 8, 8, 14, 14, 19, 19, 27, 27, 37, 37, 60, 60};
//   const static uint16_t _64frequencyOffsets[64] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 7, 7, 7, 7, 8, 8, 8, 8, 13, 13, 13, 13, 18, 18, 18, 18, 30, 30, 30, 30}; // low end biased because of int
//   const uint16_t *_frequencyOffsets;
// try_frequency_offsets_again:
//   switch (bandSize)
//   {
//   case 2:
//     _frequencyOffsets = _2frequencyOffsets;
//     break;
//   case 4:
//     _frequencyOffsets = _4frequencyOffsets;
//     break;
//   case 8:
//     _frequencyOffsets = _8frequencyOffsets;
//     break;
//   case 16:
//     _frequencyOffsets = _16frequencyOffsets;
//     break;
//   case 32:
//     _frequencyOffsets = _32frequencyOffsets;
//     break;
//   case 64:
//     _frequencyOffsets = _64frequencyOffsets;
//     break;
//   default:
//     bandSize = 8;
//     goto try_frequency_offsets_again;
//   }
//   _bandSize = bandSize;
//   _isClipping = false;
//   // for normalize falloff rates
//   if (_isAutoLevel)
//   {
//     if (_autoLevelPeakMax > _autoMin)
//     {
//       // _autoLevelPeakMaxFalloffRate = calculateFalloff(_autoLevelFalloffType, _autoLevelFalloffRate, _autoLevelPeakMaxFalloffRate);
//       _autoLevelPeakMaxFalloffRate = 0;
//       _autoLevelPeakMax -= _autoLevelPeakMaxFalloffRate;
//     }
//     if (_autoLevelVuPeakMax > _autoMin * 1.5)
//     {
//       // _autoLevelMaxFalloffRate = calculateFalloff(_autoLevelFalloffType, _autoLevelFalloffRate, _autoLevelMaxFalloffRate);
//       _autoLevelMaxFalloffRate = 0;
//       _autoLevelVuPeakMax -= _autoLevelMaxFalloffRate;
//     }
//   }
//   _vu = 0;
//   _bandMax = 0;
//   _bandAvg = 0;
//   _peakAvg = 0;
//   _bandMaxIndex = -1;
//   _bandMinIndex = -1;
//   _peakMaxIndex = -1;
//   _peakMinIndex = -1;
//   int offset = 2; // first two values are noise
//   for (int i = 0; i < _bandSize; i++)
//   {
//     _bands[i] = 0;
//     // handle band peaks fall off
//     // _peakFallRate[i] = calculateFalloff(_bandPeakFalloffType, _bandPeakFalloffRate, _peakFallRate[i]);
//     _peakFallRate[i] = 0;
//     if (_peaks[i] - _peakFallRate[i] <= _bands[i])
//     {
//       _peaks[i] = _bands[i];
//     }
//     else
//     {
//       _peaks[i] -= _peakFallRate[i]; // fall off rate
//     }
//     for (int j = 0; j < _frequencyOffsets[i]; j++)
//     {
//       // scale down factor to prevent overflow
//       int rv = (_real[offset + j] / (0xFFFF * 0xFF));
//       int iv = (_imag[offset + j] / (0xFFFF * 0xFF));
//       // some smoothing with imaginary numbers.
//       rv = sqrt(rv * rv + iv * iv);
//       // add eq offsets
//       rv = rv * _bandEq[i];
//       // combine band amplitudes for current band segment
//       _bands[i] += rv;
//       _vu += rv;
//     }
//     offset += _frequencyOffsets[i];

//     // remove noise
//     if (_bands[i] < _noiseFloor)
//     {
//       _bands[i] = 0;
//     }

//     if (_bands[i] > _peaks[i])
//     {
//       _peakFallRate[i] = 0;
//       _peaks[i] = _bands[i];
//     }

//     // handle min/max band
//     if (_bands[i] > _bandMax && _bands[i] > _noiseFloor)
//     {
//       _bandMax = _bands[i];
//       _bandMaxIndex = i;
//     }
//     if (_bands[i] < _bandMin)
//     {
//       _bandMin = _bands[i];
//       _bandMinIndex = i;
//     }
//     // handle min/max peak
//     if (_peaks[i] > _autoLevelPeakMax)
//     {
//       _autoLevelPeakMax = _peaks[i];
//       if (_isAutoLevel && _autoMax != -1 && _peaks[i] > _autoMax)
//       {
//         _isClipping = true;
//         _autoLevelPeakMax = _autoMax;
//       }
//       _peakMaxIndex = i;
//       _autoLevelPeakMaxFalloffRate = 0;
//     }
//     if (_peaks[i] < _peakMin && _peaks[i] > _noiseFloor)
//     {
//       _peakMin = _peaks[i];
//       _peakMinIndex = i;
//     }

//     // handle band average
//     _bandAvg += _bands[i];
//     _peakAvg += _peaks[i];
//   } // end bands
//   // handle band average
//   _bandAvg = _bandAvg / _bandSize;
//   _peakAvg = _peakAvg / _bandSize;

//   // handle vu peak fall off
//   _vu = _vu / 8.0; // get it closer to the band peak values
//   _vuPeakFallRate = calculateFalloff(_vuPeakFalloffType, _vuPeakFalloffRate, _vuPeakFallRate);
//   _vuPeak -= _vuPeakFallRate;
//   if (_vu > _vuPeak)
//   {
//     _vuPeakFallRate = 0;
//     _vuPeak = _vu;
//   }
//   if (_vu > _vuMax)
//   {
//     _vuMax = _vu;
//   }
//   if (_vu < _vuMin)
//   {
//     _vuMin = _vu;
//   }
//   if (_vuPeak > _autoLevelVuPeakMax)
//   {
//     _autoLevelVuPeakMax = _vuPeak;
//     if (_isAutoLevel && _autoMax != -1 && _vuPeak > _autoMax)
//     {
//       _isClipping = true;
//       _autoLevelVuPeakMax = _autoMax;
//     }
//     _autoLevelMaxFalloffRate = 0;
//   }
//   if (_vuPeak < _vuPeakMin)
//   {
//     _vuPeakMin = _vuPeak;
//   }
// }