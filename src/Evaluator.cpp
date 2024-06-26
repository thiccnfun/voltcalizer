#include <Evaluator.h>

#ifndef RF_PIN
#define RF_PIN 21
#endif

Evaluator::Evaluator(
  AppSettingsService *appSettingsService) : 
    _appSettingsService(appSettingsService)
{
  pinMode(RF_PIN, OUTPUT);
  if (!OpenShock::CommandHandler::Init()) {
    ESP_LOGW(TAG, "Unable to initialize OpenShock");
  } else {
    OpenShock::CommandHandler::SetRfTxPin(RF_PIN);
  }
}

void Evaluator::begin() {
  eventsQueue = xQueueCreate(10, sizeof(event_queue_t));

  xTaskCreatePinnedToCore(
    this->_taskRunner,
    "eventsTask",
    2048,
    this,
    (tskIDLE_PRIORITY),
    NULL,
    ESP32SVELTEKIT_RUNNING_CORE
  );
}

void Evaluator::task() {
  event_queue_t eq;
  while (xQueueReceive(eventsQueue, &eq, portMAX_DELAY)) {

    if (eq.alertType != AlertType::NONE) {
      Serial.println("Alerting user");

      if (eq.alertType == AlertType::COLLAR_VIBRATION) {
        vibrateCollar(eq.alertStrength, eq.alertDuration);
      } else if (eq.alertType == AlertType::COLLAR_BEEP) {
        beepCollar(eq.alertDuration);
      }

      vTaskDelay(eq.alertDuration / portTICK_PERIOD_MS);
      stopCollar(); 

      continue;
    }

    std::vector<EventStep> steps = std::vector<EventStep>();
    if (evaluatePassed(eq.dbPassRate)) {
      assignAffirmationSteps(steps);
    } else {
      assignCorrectionSteps(steps);
    }

    for (EventStep step : steps) {
      processStep(step, eq.dbPassRate);
    }
  }
}

void Evaluator::queueEvaluation(float dbPassRate) {
  event_queue_t eq = {
    alertType: AlertType::NONE,
    alertDuration: 0,
    alertStrength: 0,
    dbPassRate: dbPassRate
  };

  Serial.println("Queueing evaluation");

  xQueueSend(eventsQueue, &eq, portMAX_DELAY);
}

void Evaluator::queueAlert(AlertType alertType, int alertDuration, int alertStrength) {
  event_queue_t eq = {
    alertType: alertType,
    alertDuration: alertDuration,
    alertStrength: alertStrength,
    dbPassRate: 0
  };

  // TODO: limit strength by settings if vibration

  xQueueSend(eventsQueue, &eq, portMAX_DELAY);
}

bool Evaluator::vibrateCollar(int strength, int duration) {
  ESP_LOGI(TAG, "Vibrating collar: %d, %d", strength, duration);
  return OpenShock::CommandHandler::HandleCommand(
    OpenShock::ShockerModelType::CaiXianlin, 
    0, 
    OpenShock::ShockerCommandType::Vibrate,
    strength,
    duration
  );
}

bool Evaluator::beepCollar(int duration) {
  ESP_LOGI(TAG, "Beeping collar: %d", duration);
  return OpenShock::CommandHandler::HandleCommand(
    OpenShock::ShockerModelType::CaiXianlin, 
    0, 
    OpenShock::ShockerCommandType::Sound,
    100, 
    duration
  );
}

bool Evaluator::shockCollar(int strength, int duration) {
  ESP_LOGI(TAG, "Shocking collar: %d", duration);
  return OpenShock::CommandHandler::HandleCommand(
    OpenShock::ShockerModelType::CaiXianlin, 
    0, 
    OpenShock::ShockerCommandType::Shock,
    strength,
    duration
  );
}

bool Evaluator::stopCollar() {
  ESP_LOGI(TAG, "Stopping collar");
  return OpenShock::CommandHandler::HandleCommand(
    OpenShock::ShockerModelType::CaiXianlin, 
    0, 
    OpenShock::ShockerCommandType::Stop,
    0, 
    0
  );
}

ConditionState Evaluator::evaluateConditions(double currentDb, int thresholdDb) {

  // TODO: different evaluation methods
  if (currentDb >= thresholdDb) {
      return ConditionState::REACHED;
  }

  return ConditionState::NOT_REACHED;
}

bool Evaluator::evaluatePassed(float passRate) {
  double passThreshold = 0;
  assignPassDetails(passThreshold);
  return passRate >= passThreshold;
}

void Evaluator::processStep(EventStep step, float passRate) {
  switch (step.type) {
    case EventType::COLLAR_VIBRATION:
    case EventType::COLLAR_SHOCK:
    case EventType::COLLAR_BEEP:
      processCollarStep(step, passRate);
      break;
    default:
      // unknown event type
      break;
  }
}

void Evaluator::processCollarStep(EventStep step, float passRate) {
  int minVal = 0;
  int maxVal = 0;
  _appSettingsService->read([&](AppSettings &settings) {
    maxVal = step.type == EventType::COLLAR_SHOCK ? settings.collarMaxShock : settings.collarMaxVibe;
    minVal = step.type == EventType::COLLAR_SHOCK ? settings.collarMinShock : settings.collarMinVibe;
  });
  int strength = map(
    valueFromRangeType(step.strength_range_type, step.strength_range, passRate) * 100, 0, 100, 
    minVal, maxVal
  );
  double duration = valueFromRangeType(step.time_range_type, step.time_range, passRate) * 1000;

  switch (step.type) {
    case EventType::COLLAR_VIBRATION:
      vibrateCollar(strength, duration);
      break;
    case EventType::COLLAR_SHOCK:
      shockCollar(strength, duration);
      break;
    case EventType::COLLAR_BEEP:
      beepCollar(duration);
      break;
    default:
      // unknown event type
      break;
  }
  
  vTaskDelay(duration / portTICK_PERIOD_MS);

  stopCollar();
}


double Evaluator::valueFromRangeType(RangeType rangeType, std::vector<double> range, float passRate) {

  switch (rangeType) {
    case RangeType::RANDOM:
      // return random(range[0], range[1]);
      return range[0] + (rand() / (RAND_MAX / (range[1] - range[0])));
    case RangeType::PROGRESSIVE:
      // TODO: implement progressive range - increase duration based on pass rate (does not go back down)
      return range[0];
    case RangeType::REDEEMABLE:
      // TODO: implement redeemable range - same as above but can be reduced by pass rate
      return range[0];
    case RangeType::GRADED:
      // TODO: implement graded range - set duration based on pass rate (no accumulation)
      return range[0];
    case RangeType::FIXED:
      return range[0];
  }

  return range[0];
}

void Evaluator::assignPassDetails(double &passThreshold) {
  _appSettingsService->read([&](AppSettings &settings) {
    passThreshold = settings.passThreshold;
  });
}

void Evaluator::assignAffirmationSteps(
    std::vector<EventStep> &affirmationSteps
) {
    _appSettingsService->read([&](AppSettings &settings) {
        affirmationSteps = settings.affirmationSteps;
    });
}

void Evaluator::assignCorrectionSteps(
    std::vector<EventStep> &correctionSteps
) {
    _appSettingsService->read([&](AppSettings &settings) {
        correctionSteps = settings.correctionSteps;
    });
}

