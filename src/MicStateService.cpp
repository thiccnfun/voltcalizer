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







// #include <ZapMe.h>
#include <ArduinoFFT.h>
#include <cmath>
#include <complex>
#include <vector>
#include <driver/i2s.h>
#include <sos-iir-filter.h>

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
// IIR Filters
//

// DC-Blocker filter - removes DC component from I2S data
// See: https://www.dsprelated.com/freebooks/filters/DC_Blocker.html
// a1 = -0.9992 should heavily attenuate frequencies below 10Hz
SOS_IIR_Filter DC_BLOCKER = { 
  gain: 1.0,
  sos: {{-1.0, 0.0, +0.9992, 0}}
};

// No_IIR_Filter None;

// 
// Equalizer IIR filters to flatten microphone frequency response
// See respective .m file for filter design. Fs = 48Khz.
//
// Filters are represented as Second-Order Sections cascade with assumption
// that b0 and a0 are equal to 1.0 and 'gain' is applied at the last step 
// B and A coefficients were transformed with GNU Octave: 
// [sos, gain] = tf2sos(B, A)
// See: https://www.dsprelated.com/freebooks/filters/Series_Second_Order_Sections.html
// NOTE: SOS matrix 'a1' and 'a2' coefficients are negatives of tf2sos output
//

// TDK/InvenSense ICS-43434
// Datasheet: https://www.invensense.com/wp-content/uploads/2016/02/DS-000069-ICS-43434-v1.1.pdf
// B = [0.477326418836803, -0.486486982406126, -0.336455844522277, 0.234624646917202, 0.111023257388606];
// A = [1.0, -1.93073383849136326, 0.86519456089576796, 0.06442838283825100, 0.00111249298800616];
SOS_IIR_Filter ICS43434 = { 
  gain: 0.477326418836803,
  sos: { // Second-Order Sections {b1, b2, -a1, -a2}
   {+0.96986791463971267, 0.23515976355743193, -0.06681948004769928, -0.00111521990688128},
   {-1.98905931743624453, 0.98908924206960169, +1.99755331853906037, -0.99755481510122113}
  }
};

// TDK/InvenSense ICS-43432
// Datasheet: https://www.invensense.com/wp-content/uploads/2015/02/ICS-43432-data-sheet-v1.3.pdf
// B = [-0.45733702338341309   1.12228667105574775  -0.77818278904413563, 0.00968926337978037, 0.10345668405223755]
// A = [1.0, -3.3420781082912949, 4.4033694320978771, -3.0167072679918010, 1.2265536567647031, -0.2962229189311990, 0.0251085747458112]
SOS_IIR_Filter ICS43432 = {
  gain: -0.457337023383413,
  sos: { // Second-Order Sections {b1, b2, -a1, -a2}
    {-0.544047931916859, -0.248361759321800, +0.403298891662298, -0.207346186351843},
    {-1.909911869441421, +0.910830292683527, +1.790285722826743, -0.804085812369134},
    {+0.000000000000000, +0.000000000000000, +1.148493493802252, -0.150599527756651}
  }
};

// TDK/InvenSense INMP441
// Datasheet: https://www.invensense.com/wp-content/uploads/2015/02/INMP441.pdf
// B ~= [1.00198, -1.99085, 0.98892]
// A ~= [1.0, -1.99518, 0.99518]
SOS_IIR_Filter INMP441 = {
  gain: 1.00197834654696, 
  sos: { // Second-Order Sections {b1, b2, -a1, -a2}
    {-1.986920458344451, +0.986963226946616, +1.995178510504166, -0.995184322194091}
  }
};

// Infineon IM69D130 Shield2Go
// Datasheet: https://www.infineon.com/dgdl/Infineon-IM69D130-DS-v01_00-EN.pdf?fileId=5546d462602a9dc801607a0e46511a2e
// B ~= [1.001240684967527, -1.996936108836337, 0.995703101823006]
// A ~= [1.0, -1.997675693595542, 0.997677044195563]
// With additional DC blocking component
SOS_IIR_Filter IM69D130 = {
  gain: 1.00124068496753,
  sos: {
    {-1.0, 0.0, +0.9992, 0}, // DC blocker, a1 = -0.9992
    {-1.994461610298131, 0.994469278738208, +1.997675693595542, -0.997677044195563}
  }
};

// Knowles SPH0645LM4H-B, rev. B
// https://cdn-shop.adafruit.com/product-files/3421/i2S+Datasheet.PDF
// B ~= [1.001234, -1.991352, 0.990149]
// A ~= [1.0, -1.993853, 0.993863]
// With additional DC blocking component
SOS_IIR_Filter SPH0645LM4H_B_RB = {
  gain: 1.00123377961525, 
  sos: { // Second-Order Sections {b1, b2, -a1, -a2}
    {-1.0, 0.0, +0.9992, 0}, // DC blocker, a1 = -0.9992
    {-1.988897663539382, +0.988928479008099, +1.993853376183491, -0.993862821429572}
  }
};

//
// Weighting filters
//

//
// A-weighting IIR Filter, Fs = 48KHz 
// (By Dr. Matt L., Source: https://dsp.stackexchange.com/a/36122)
// B = [0.169994948147430, 0.280415310498794, -1.120574766348363, 0.131562559965936, 0.974153561246036, -0.282740857326553, -0.152810756202003]
// A = [1.0, -2.12979364760736134, 0.42996125885751674, 1.62132698199721426, -0.96669962900852902, 0.00121015844426781, 0.04400300696788968]
SOS_IIR_Filter A_weighting = {
  gain: 0.169994948147430, 
  sos: { // Second-Order Sections {b1, b2, -a1, -a2}
    {-2.00026996133106, +1.00027056142719, -1.060868438509278, -0.163987445885926},
    {+4.35912384203144, +3.09120265783884, +1.208419926363593, -0.273166998428332},
    {-0.70930303489759, -0.29071868393580, +1.982242159753048, -0.982298594928989}
  }
};

//
// C-weighting IIR Filter, Fs = 48KHz 
// Designed by invfreqz curve-fitting, see respective .m file
// B = [-0.49164716933714026, 0.14844753846498662, 0.74117815661529129, -0.03281878334039314, -0.29709276192593875, -0.06442545322197900, -0.00364152725482682]
// A = [1.0, -1.0325358998928318, -0.9524000181023488, 0.8936404694728326   0.2256286147169398  -0.1499917107550188, 0.0156718181681081]
SOS_IIR_Filter C_weighting = {
  gain: -0.491647169337140,
  sos: { 
    {+1.4604385758204708, +0.5275070373815286, +1.9946144559930252, -0.9946217070140883},
    {+0.2376222404939509, +0.0140411206016894, -1.3396585608422749, -0.4421457807694559},
    {-2.0000000000000000, +1.0000000000000000, +0.3775800047420818, -0.0356365756680430}
  }
};


//
// Sampling
//
#define SAMPLE_RATE       16000 // Hz, fixed to design of IIR filters
#define SAMPLE_BITS       32    // bits
#define SAMPLE_T          int32_t 
#define SAMPLES_SHORT     (SAMPLE_RATE / 8) // ~125ms
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

// Static buffer for block of samples
float samples[SAMPLES_SHORT] __attribute__((aligned(4)));





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




#define CONDITIONS_NOT_EVALUATED 0
#define CONDITIONS_NOT_REACHED 1
#define CONDITIONS_REACHED 2





MicStateService::MicStateService(
  PsychicHttpServer *server,
  SecurityManager *securityManager,
  PsychicMqttClient *mqttClient,
  AppSettingsService *appSettingsService,
  NotificationEvents *notificationEvents,
  CH8803 *collar) : 
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
    _appSettingsService(appSettingsService),
    _notificationEvents(notificationEvents),
    _collar(collar)
{
}

void MicStateService::begin()
{
    _httpEndpoint.begin();
    _webSocketServer.begin();
    // onConfigUpdated();

    setupCollar();
    setupReader();
}

void MicStateService::onConfigUpdated()
{
    // digitalWrite(LED_BUILTIN, _state.ledOn ? 1 : 0);
}

void MicStateService::setupCollar() {
   // TODO: make this configurable
  _collar->setId(13, 37);
}

void MicStateService::vibrateCollar(int strength, int duration) {
  _collar->sendVibration(strength, duration);
}

void MicStateService::beepCollar(int duration) {
  _collar->sendAudio(0, duration);
}

void MicStateService::setupReader() {
    

    // Create FreeRTOS queue
    samplesQueue = xQueueCreate(8, sizeof(sum_queue_t));
    
    // Create the I2S reader FreeRTOS task
    // NOTE: Current version of ESP-IDF will pin the task 
    //       automatically to the first core it happens to run on
    //       (due to using the hardware FPU instructions).
    //       For manual control see: xTaskCreatePinnedToCore
    // xTaskCreate(_readerTask, "Mic I2S Reader", I2S_TASK_STACK, NULL, I2S_TASK_PRI, NULL);

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
    int eventDuration = 5000; // how long until the event ends
    int actDuration = 2000; // how long the user has to act before the event ends
    // int actWindow = 2000;

    assignDurationValues(idleDuration, eventDuration, actDuration);
    int sequenceDuration = eventDuration + actDuration;
    int eventCountdown = sequenceDuration;


    unsigned long currentTime = millis();
    unsigned long elapsedTime = currentTime - startTime;

    int conditionEvaluation = CONDITIONS_NOT_EVALUATED;

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
            
            // Serial output, customize (or remove) as needed
            // Serial.printf("%.1fdB\n", Leq_dB);
            currentTime = millis();
            elapsedTime = currentTime - startTime;
            
            eventCountdown = sequenceDuration - elapsedTime + idleDuration;
            
            if (_state.enabled && eventCountdown <= 0) {
                if (conditionEvaluation == CONDITIONS_REACHED) {
                    handleAffirmation();
                } else if (conditionEvaluation == CONDITIONS_NOT_REACHED) {
                    handleCorrection();
                }

                // NOTE: maybe delay accordingly
                startTime = currentTime;

                assignDurationValues(idleDuration, eventDuration, actDuration);
                sequenceDuration = eventDuration + actDuration;
                eventCountdown = eventCountdown;
                conditionEvaluation = 0;

            } else if (_state.enabled && eventCountdown <= actDuration) {
                Serial.println("--------- ACTION WINDOW --------------");

                // if the sample has not reached the threshold, evaluate it
                if (conditionEvaluation < CONDITIONS_REACHED) {
                    conditionEvaluation = evaluateConditions(Leq_dB);
                }
            }

            // Update the state, emitting change event if value changed
            updateDbValue(Leq_dB, q.pitch, elapsedTime <= idleDuration ? -1 : eventCountdown);
        }
    
        // Debug only
        //Serial.printf("%u processing ticks\n", q.proc_ticks);
    }
}

int MicStateService::evaluateConditions(double currentDb) {
  double thresholdDb = 80;
  assignConditionValues(thresholdDb);

  // TODO: different evaluation methods
  if (currentDb >= thresholdDb) {
      return CONDITIONS_REACHED;
  }

  return CONDITIONS_NOT_REACHED;
}

void MicStateService::handleAffirmation() {
 Serial.println("==========REWARD==========");
}

void MicStateService::handleCorrection() {
  // TODO: get settings for punishment

  vibrateCollar(50, 1000);
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
        // Block and wait for microphone values from I2S
        //
        // Data is moved from DMA buffers to our 'samples' buffer by the driver ISR
        // and when there is requested ammount of data, task is unblocked
        //
        // Note: i2s_read does not care it is writing in float[] buffer, it will write
        //       integer values to the given address, as received from the hardware peripheral. 
        i2s_read(I2S_PORT, &samples, SAMPLES_SHORT * sizeof(SAMPLE_T), &bytes_read, portMAX_DELAY);

        TickType_t start_tick = xTaskGetTickCount();
        
        // Convert (including shifting) integer microphone values to floats, 
        // using the same buffer (assumed sample size is same as size of float), 
        // to save a bit of memory
        SAMPLE_T* int_samples = (SAMPLE_T*)&samples;
        for(int i=0; i<SAMPLES_SHORT; i++) samples[i] = MIC_CONVERT(int_samples[i]);

        sum_queue_t q;
        // Apply equalization and calculate Z-weighted sum of squares, 
        // writes filtered samples back to the same buffer.
        q.sum_sqr_SPL = MIC_EQUALIZER.filter(samples, samples, SAMPLES_SHORT);

        // Apply weighting and calucate weigthed sum of squares
        q.sum_sqr_weighted = WEIGHTING.filter(samples, samples, SAMPLES_SHORT);

        // Debug only. Ticks we spent filtering and summing block of I2S data
        // q.proc_ticks = xTaskGetTickCount() - start_tick;

        // Calculate pitch
        q.pitch = calculatePitch(samples, SAMPLE_RATE);

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
float MicStateService::calculatePitch(float samples[SAMPLES_SHORT], int sampleRate) {
  // int N = samples.size();
  // std::vector<double> fftInput(N);




  // // Convert samples to double for FFT
  // for (int i = 0; i < N; i++) {
  //   fftInput[i] = static_cast<double>(samples[i]);
  // }
  double ratio = twoPi * SAMPLES_SHORT / samplingFrequency; // Fraction of a complete cycle stored at each sample (in radians)
  for (uint16_t i = 0; i < SAMPLES_SHORT; i++)
  {
    vReal[i] = static_cast<float>(samples[i]);/* Build data with positive and negative values*/
    vImag[i] = 0.0; //Imaginary part must be zeroed in case of looping to avoid wrong calculations and overflows
  }

  fft = arduinoFFT(vReal, vImag, SAMPLE_BITS, samplingFrequency); /* Create FFT object */

  // Serial.println("Data:");
  // printVector(vReal, SAMPLES_SHORT, SCL_TIME);
  fft.Windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);	/* Weigh data */
  // Serial.println("Weighed data:");
  // printVector(vReal, SAMPLES_SHORT, SCL_TIME);
  // fft.Compute(FFT_FORWARD); /* Compute FFT */
  // Serial.println("Computed Real values:");
  // printVector(vReal, SAMPLES_SHORT, SCL_INDEX);
  // Serial.println("Computed Imaginary values:");
  // printVector(vImag, SAMPLES_SHORT, SCL_INDEX);
  // fft.ComplexToMagnitude(); /* Compute magnitudes */
  // Serial.println("Computed magnitudes:");
  // printVector(vReal, (SAMPLES_SHORT >> 1), SCL_FREQUENCY);
  double pitch = fft.MajorPeak();
  // Serial.println(x, 6); //Print out what frequency is the most dominant.

  // return pitch as float
  return static_cast<float>(pitch);

  // // // Perform FFT
  // // fft.Windowing(fftInput.data(), N, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  // // fft.Compute(fftInput.data(), fftOutput.data(), N, FFT_FORWARD);
  // // // fft.ComplexToMagnitude(fftOutput.data(), N);

  // // Find the peak frequency (pitch)
  // float maxMagnitude = 0.0f;
  // float pitch = 0.0f;

  // // for (int i = 1; i < N / 2; i++) {
  // //   if (fftOutput[i] > maxMagnitude) {
  // //     maxMagnitude = fftOutput[i];
  // //     pitch = i * sampleRate / N;
  // //   }
  // // }

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

void MicStateService::updateDbValue(float dbValue, float pitchValue, unsigned long eventCountdown)
{
  update([&](MicState& state) {
    if (state.dbValue == dbValue && state.eventCountdown == eventCountdown) {
      return StateUpdateResult::UNCHANGED;
    }
    state.dbValue = dbValue;
    state.eventCountdown = eventCountdown;
    state.dbThreshold = eventCountdown == -1 ? 0 : 70;
    state.pitchValue = pitchValue;
    return StateUpdateResult::CHANGED;
  }, "db_set");
}

void MicStateService::assignDurationValues(
    int &idleDuration,
    int &eventDuration,
    int &actDuration
) {
    _appSettingsService->read([&](AppSettings &settings) {
        // actDuration = settings.actionPeriodMinMs;
        // actWindow = settings.actionPeriodMaxMs;
        // eventCountdown = Duration;

        // TODO: assign values from settings
    });
}

void MicStateService::assignConditionValues(
    double &dbThreshold
) {
    _appSettingsService->read([&](AppSettings &settings) {
        dbThreshold = settings.decibelThresholdMin;
    });
}