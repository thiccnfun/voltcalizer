#ifndef Evaluator_h
#define Evaluator_h

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

struct event_queue_t {
  AlertType alertType;
  int alertDuration;
  int alertStrength;
  float dbPassRate;
};

enum ConditionState {
  NOT_EVALUATED,
  REACHED,
  NOT_REACHED
};

class Evaluator
{
public:
    Evaluator(AppSettingsService *appSettingsService);
    ConditionState evaluateConditions(double currentDb, int thresholdDb);
    bool vibrateCollar(int strength, int duration);
    bool shockCollar(int strength, int duration);
    bool beepCollar(int duration);
    bool stopCollar();
    QueueHandle_t eventsQueue;
    void begin();
    void queueEvaluation(float dbPassRate);
    void queueAlert(AlertType alertType, int alertDuration, int alertStrength);

protected:
    void task();
    static void _taskRunner(void *_this) { static_cast<Evaluator *>(_this)->task(); }

private:
    AppSettingsService *_appSettingsService;
    void assignPassDetails(double &passThreshold);
    bool evaluatePassed(float passRate);
    void assignAffirmationSteps(
        std::vector<EventStep> &affirmationSteps
    );
    void assignCorrectionSteps(
        std::vector<EventStep> &correctionSteps
    );
    void processStep(EventStep step, float passRate);
    void processCollarStep(EventStep step, float passRate);
    double valueFromRangeType(RangeType rangeType, std::vector<double> range, float passRate);
};

#endif
