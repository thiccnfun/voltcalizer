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
#include <Evaluator.h>
#include <HttpEndpoint.h>
#include <MqttPubSub.h>
#include <WebSocketServer.h>
#include <AudioAnalyzer.h>
// #include <WebSocketClient.h>

#define MIC_STATE_ENDPOINT_PATH "/rest/micState"
#define MIC_STATE_SOCKET_PATH "/ws/micState"

class MicState
{
public:
    double dbThreshold = 70;
    double dbValue = 0;
    double pitchThreshold = 0;
    double pitchValue = 0;
    int eventCountdown = 0;
    float dbPassRate = 0;
    float pitchPassRate = 0;

    bool enabled = false;

    static void read(MicState &settings, JsonObject &root)
    {
        root["dbt"] = settings.dbThreshold;
        root["dbv"] = settings.dbValue;
        root["ecd"] = settings.eventCountdown;
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
    MicStateService(
        PsychicHttpServer *server,
        SecurityManager *securityManager,
        PsychicMqttClient *mqttClient,
        AppSettingsService *appSettingsService
    );
    void begin();
    void setupReader();

protected:
    Evaluator *_evaluator;
    AudioAnalyzer *_audioAnalyzer;
    void assignRoutineConditionValues(
        double &dbThreshold,
        int &idleDuration,
        int &actDuration,
        AlertType &alertType,
        int &alertDuration,
        int &alertStrength,
        PassType &passType
    );

private:
    HttpEndpoint<MicState> _httpEndpoint;
    MqttPubSub<MicState> _mqttPubSub;
    WebSocketServer<MicState> _webSocketServer;
    PsychicMqttClient *_mqttClient;
    AppSettingsService *_appSettingsService;

    void registerConfig();
    void updateState(
        float dbValue, 
        float pitchValue, 
        int actCountdown,
        int thresholdDb,
        float dbPassRate
    );
};

#endif
