#ifndef AppSettingsService_h
#define AppSettingsService_h

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

#include <HttpEndpoint.h>
#include <WebSocketServer.h>
#include <FSPersistence.h>
// #include <SettingValue.h>

#define APP_SETTINGS_FILE "/config/appSettings.json"
#define APP_SETTINGS_ENDPOINT_PATH "/rest/appSettings"
#define APP_SETTINGS_SOCKET_PATH "/ws/appSettings"

class AppSettings
{
public:
    int idlePeriodMinMs = 1000 * 10;
    int idlePeriodMaxMs = 1000 * 10;
    int actionPeriodMinMs = 1000;
    int actionPeriodMaxMs = 1000;
    int decibelThresholdMin = 80;
    int decibelThresholdMax = 80;
    int micSensitivity = 26; // 26-29 per the datasheet

    static void read(AppSettings &settings, JsonObject &root)
    {
        root["idle_period_min_ms"] = settings.idlePeriodMinMs;
        root["idle_period_max_ms"] = settings.idlePeriodMaxMs;
        root["action_period_min_ms"] = settings.actionPeriodMinMs;
        root["action_period_max_ms"] = settings.actionPeriodMaxMs;
        root["decibel_threshold_min"] = settings.decibelThresholdMin;
        root["decibel_threshold_max"] = settings.decibelThresholdMax;
        root["mic_sensitivity"] = settings.micSensitivity;
    }

    static StateUpdateResult update(JsonObject &root, AppSettings &settings)
    {
        settings.idlePeriodMinMs = root["idle_period_min_ms"] | settings.idlePeriodMinMs;
        settings.idlePeriodMaxMs = root["idle_period_max_ms"] | settings.idlePeriodMaxMs;
        settings.actionPeriodMinMs = root["action_period_min_ms"] | settings.actionPeriodMinMs;
        settings.actionPeriodMaxMs = root["action_period_max_ms"] | settings.actionPeriodMaxMs;
        settings.decibelThresholdMin = root["decibel_threshold_min"] | settings.decibelThresholdMin;
        settings.decibelThresholdMax = root["decibel_threshold_max"] | settings.decibelThresholdMax;
        settings.micSensitivity = root["mic_sensitivity"] | settings.micSensitivity;
        return StateUpdateResult::CHANGED;
    }
};

class AppSettingsService : public StatefulService<AppSettings>
{
public:
    AppSettingsService(PsychicHttpServer *server, FS *fs, SecurityManager *securityManager);
    void begin();

private:
    HttpEndpoint<AppSettings> _httpEndpoint;
    FSPersistence<AppSettings> _fsPersistence;
    WebSocketServer<AppSettings> _webSocketServer;
};

#endif // end AppSettingsService_h
