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

// #include <ArduinoFFT.h>

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
{}

void MicStateService::begin()
{
    _httpEndpoint.begin();
    _webSocketServer.begin();

    _audioAnalyzer = new AudioAnalyzer();
    _audioAnalyzer->begin();

    _evaluator = new Evaluator(_appSettingsService);
    _evaluator->begin();

    setupReader();
}

void MicStateService::setupReader() {

    sum_queue_t q;
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
    double decibels = -1;

    // Read sum of samaples, calculated by 'i2s_reader_task'
    while (xQueueReceive(_audioAnalyzer->samplesQueue, &q, portMAX_DELAY)) {
        decibels = _audioAnalyzer->getDecibels(q);

        // When we gather enough samples, calculate new Leq value
        if (decibels != -1) {

            // Serial.printf(">decibel:%.1f", decibels);
            // Serial.println();
            
            // Serial output, customize (or remove) as needed
            // Serial.printf("%.1fdB\n", decibels);
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

                if (_evaluator->evaluateConditions(decibels, thresholdDb) == ConditionState::REACHED) {
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
              decibels, 
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
