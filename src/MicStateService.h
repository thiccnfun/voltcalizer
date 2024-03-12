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
    int actDuration = 0;
    int collarId = 0;

    bool enabled = false;

    static void read(MicState &settings, JsonObject &root)
    {
        root["dbt"] = settings.dbThreshold;
        root["dbv"] = settings.dbValue;
        root["ecd"] = settings.eventCountdown;
        root["acd"] = settings.actDuration;
        root["pv"] = settings.pitchValue;
        root["pt"] = settings.pitchThreshold;
        root["en"] = settings.enabled;
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
    static void _readerTask(void *_this) { static_cast<MicStateService *>(_this)->readerTask(); }
    void assignDurationValues(
        int &idleDuration,
        int &actDuration
    );
    void assignConditionValues(
        double &dbThreshold
    );
    int evaluateConditions(double currentDb, int thresholdDb);
    void handleAffirmation();
    void handleCorrection();

private:
    HttpEndpoint<MicState> _httpEndpoint;
    MqttPubSub<MicState> _mqttPubSub;
    WebSocketServer<MicState> _webSocketServer;
    //  WebSocketClient<LightState> _webSocketClient;
    PsychicMqttClient *_mqttClient;
    AppSettingsService *_appSettingsService;
    NotificationEvents *_notificationEvents;
    CH8803 *_collar;

    QueueHandle_t samplesQueue;
    arduinoFFT fft;
    double vReal[SAMPLES_SHORT];
    double vImag[SAMPLES_SHORT];

    void registerConfig();
    void onConfigUpdated();
    void updateState(
        float dbValue, 
        float pitchValue, 
        int actCountdown,
        int thresholdDb
    );
    float calculatePitch(float samples[], int sampleRate);
    void printVector(double *vData, uint16_t bufferSize, uint8_t scaleType);
    // void fft(const std::vector<std::complex<float>>& input, std::vector<std::complex<float>>& output);

};

#endif
