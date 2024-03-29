#ifndef MicStateService_h
#define MicStateService_h

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

#include <AppSettingsService.h>

// #include <complex>
// #include <vector>

#include <AudioAnalysis.h>
#include <ArduinoFFT.h>
#include <HttpEndpoint.h>
#include <MqttPubSub.h>
#include <WebSocketServer.h>
#include <NotificationEvents.h>
// #include <WebSocketClient.h>
#include <ZapMe.h>


#define DEFAULT_LED_STATE false
// #define OFF_STATE "OFF"
// #define ON_STATE "ON"

#define MIC_STATE_ENDPOINT_PATH "/rest/micState"
#define MIC_STATE_SOCKET_PATH "/ws/micState"


#define SAMPLE_RATE       16000 // Hz, fixed to design of IIR filters
#define SAMPLE_BITS       32    // bits
#define SAMPLE_T          int32_t 
#define SAMPLES_SHORT     (SAMPLE_RATE / 8) // ~125ms

class MicState
{
public:
    double dbThreshold = 70;
    double dbValue = 0;
    double pitchThreshold = 0;
    double pitchValue = 0;
    int eventCountdown = 0;
    // int actDuration = 0;
    // int collarId = 0;
    float dbPassRate = 0;
    float pitchPassRate = 0;

    bool enabled = false;

    static void read(MicState &settings, JsonObject &root)
    {
        root["dbt"] = settings.dbThreshold;
        root["dbv"] = settings.dbValue;
        root["ecd"] = settings.eventCountdown;
        // root["acd"] = settings.actDuration;
        root["pv"] = settings.pitchValue;
        root["pt"] = settings.pitchThreshold;
        root["en"] = settings.enabled;
        root["dpr"] = settings.dbPassRate;
        root["ppr"] = settings.pitchPassRate;
    }

    static StateUpdateResult update(JsonObject &root, MicState &micState)
    {
        boolean newEnabled = root["en"] | micState.enabled;
        if (micState.enabled != newEnabled) {
            micState.enabled = newEnabled;
            return StateUpdateResult::CHANGED;
        }
        return StateUpdateResult::UNCHANGED;
    }
};

class MicStateService : public StatefulService<MicState>
{
public:
    MicStateService(PsychicHttpServer *server,
                      SecurityManager *securityManager,
                      PsychicMqttClient *mqttClient,
                      AppSettingsService *appSettingsService,
                      NotificationEvents *notificationEvents,
                      CH8803 *collar);
    void begin();
    void setupReader();
    void setupCollar();
    void initializeI2s();
    void vibrateCollar(int strength, int duration);
    void beepCollar(int duration);

protected:
    void readerTask();
    void eventsTask();
    static void _readerTask(void *_this) { static_cast<MicStateService *>(_this)->readerTask(); }
    static void _eventsTask(void *_this) { static_cast<MicStateService *>(_this)->eventsTask(); }
    void assignRoutineConditionValues(
        double &dbThreshold,
        int &idleDuration,
        int &actDuration,
        AlertType &alertType
    );
    int evaluateConditions(double currentDb, int thresholdDb);
    bool evaluatePassed(float passRate);
    void handleAffirmation(float passRate);
    void handleCorrection(float passRate);
    void assignAffirmationSteps(
        std::vector<EventStep> &affirmationSteps
    );
    void assignCorrectionSteps(
        std::vector<EventStep> &correctionSteps
    );
    void processStep(EventStep step, float passRate);

private:
    HttpEndpoint<MicState> _httpEndpoint;
    MqttPubSub<MicState> _mqttPubSub;
    WebSocketServer<MicState> _webSocketServer;
    //  WebSocketClient<LightState> _webSocketClient;
    PsychicMqttClient *_mqttClient;
    AppSettingsService *_appSettingsService;
    NotificationEvents *_notificationEvents;
    CH8803 *_collar;
    ArduinoFFT<float> *_fft;
    AudioAnalysis _audioInfo;
    float samples[SAMPLES_SHORT] __attribute__((aligned(4)));
    int32_t* intSamples = new int32_t[SAMPLES_SHORT];

    QueueHandle_t samplesQueue;
    QueueHandle_t eventsQueue;
    float _real[SAMPLES_SHORT];
    float _imag[SAMPLES_SHORT];
    float _weighingFactors[SAMPLES_SHORT];

    void registerConfig();
    void onConfigUpdated();
    void updateState(
        float dbValue, 
        float pitchValue, 
        int actCountdown,
        int thresholdDb,
        float dbPassRate
    );
    float calculatePitch();
    void printVector(double *vData, uint16_t bufferSize, uint8_t scaleType);
    // void fft(const std::vector<std::complex<float>>& input, std::vector<std::complex<float>>& output);
    void computeFrequencies(uint8_t bandSize);

};

#endif
