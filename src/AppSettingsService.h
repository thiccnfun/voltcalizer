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
#include <ZapMe.h>
#include <vector>

#define APP_SETTINGS_FILE "/config/appSettings.json"
#define APP_SETTINGS_ENDPOINT_PATH "/rest/appSettings"
#define TEST_COLLAR_ENDPOINT_PATH "/rest/testCollar"
#define APP_SETTINGS_SOCKET_PATH "/ws/appSettings"

enum class EventType {
    COLLAR_BEEP,
    COLLAR_VIBRATION,
    COLLAR_SHOCK,
};

enum class RangeType {
    FIXED,
    RANDOM,
    PROGRESSIVE,
    REDEEMAABLE,
    GRADED,
};

struct EventStep {
    EventType type;
    int start_delay;
    int end_delay;
    RangeType time_range_type;
    std::vector<double> time_range;
    RangeType strength_range_type;
    std::vector<double> strength_range;
};

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

    int collarMinShock = 5;
    int collarMaxShock = 75;
    int collarMinVibe = 5;
    int collarMaxVibe = 100;

    std::vector<EventStep> correctionSteps;
    std::vector<EventStep> affirmationSteps;

    static void read(AppSettings &settings, JsonObject &root)
    {
        root["idle_period_min_ms"] = settings.idlePeriodMinMs;
        root["idle_period_max_ms"] = settings.idlePeriodMaxMs;
        root["action_period_min_ms"] = settings.actionPeriodMinMs;
        root["action_period_max_ms"] = settings.actionPeriodMaxMs;
        root["decibel_threshold_min"] = settings.decibelThresholdMin;
        root["decibel_threshold_max"] = settings.decibelThresholdMax;
        root["mic_sensitivity"] = settings.micSensitivity;
        root["collar_min_shock"] = settings.collarMinShock;
        root["collar_max_shock"] = settings.collarMaxShock;

      

        JsonArray correctionStepsArray = root.createNestedArray("correction_steps");
        for (const auto &step : settings.correctionSteps)
        {
            JsonObject stepObject = correctionStepsArray.createNestedObject();
            AppSettings::mapStepToJson(step, stepObject);
        }

        // Do the same for affirmationSteps
        JsonArray affirmationStepsArray = root.createNestedArray("affirmation_steps");
        for (const auto &step : settings.affirmationSteps)
        {
            JsonObject stepObject = affirmationStepsArray.createNestedObject();
            AppSettings::mapStepToJson(step, stepObject);
        }

        // root["correction_steps"] = correctionStepsArray;
        // root["affirmation_steps"] = affirmationStepsArray;
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
        settings.collarMinShock = root["collar_min_shock"] | settings.collarMinShock;
        settings.collarMaxShock = root["collar_max_shock"] | settings.collarMaxShock;
        settings.collarMinVibe = root["collar_min_vibe"] | settings.collarMinVibe;
        settings.collarMaxVibe = root["collar_max_vibe"] | settings.collarMaxVibe;

        JsonArray correctionStepsArray = root["correction_steps"];
        settings.correctionSteps.clear();
        for (JsonObject stepObject : correctionStepsArray) {
            AppSettings::mapStepFromJson(stepObject, settings.correctionSteps);
        }

        JsonArray affirmationStepsArray = root["affirmation_steps"];
        settings.affirmationSteps.clear();
        for (JsonObject stepObject : affirmationStepsArray) {
            AppSettings::mapStepFromJson(stepObject, settings.affirmationSteps);
        }

        return StateUpdateResult::CHANGED;
    }

    static void mapStepFromJson(JsonObject &stepObject, std::vector<EventStep> &destination) {
        EventStep step;
        step.type = static_cast<EventType>(stepObject["type"].as<int>());
        step.start_delay = stepObject["start_delay"];
        step.end_delay = stepObject["end_delay"];
        step.time_range_type = static_cast<RangeType>(stepObject["time_range_type"].as<int>());
        JsonArray timeRangeArray = stepObject["time_range"];
        for (double time : timeRangeArray) {
            step.time_range.push_back(time);
        }
        step.strength_range_type = static_cast<RangeType>(stepObject["strength_range_type"].as<int>());
        JsonArray strengthRangeArray = stepObject["strength_range"];
        for (double strength : strengthRangeArray) {
            step.strength_range.push_back(strength);
        }
        destination.push_back(step);
    }

    static void mapStepToJson(const EventStep &step, JsonObject &stepObject) {
        // JsonObject stepObject = affirmationStepsArray.createNestedObject();
        stepObject["type"] = static_cast<int>(step.type);
        stepObject["start_delay"] = step.start_delay;
        stepObject["end_delay"] = step.end_delay;
        stepObject["time_range_type"] = static_cast<int>(step.time_range_type);
        JsonArray timeRangeArray = stepObject.createNestedArray("time_range");
        for (const auto &time : step.time_range) {
            timeRangeArray.add(time);
        }
        stepObject["strength_range_type"] = static_cast<int>(step.strength_range_type);
        JsonArray strengthRangeArray = stepObject.createNestedArray("strength_range");
        for (const auto &strength : step.strength_range) {
            strengthRangeArray.add(strength);
        }
    }
};


class AppSettingsService : public StatefulService<AppSettings>
{
public:
    AppSettingsService(PsychicHttpServer *server, FS *fs, SecurityManager *securityManager, CH8803 *collar);
    void begin();

private:
    HttpEndpoint<AppSettings> _httpEndpoint;
    FSPersistence<AppSettings> _fsPersistence;
    WebSocketServer<AppSettings> _webSocketServer;
    SecurityManager *_securityManager;
    PsychicHttpServer *_server;
    CH8803 *_collar;
};

#endif // end AppSettingsService_h
