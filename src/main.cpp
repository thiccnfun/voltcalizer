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

#include <ESP32SvelteKit.h>
#include <AppSettingsService.h>
#include <MicStateService.h>
// #include <PsychicHttpServer.h>

#define RF_PIN            21

// -------------
PsychicHttpServer server;

ESP32SvelteKit esp32sveltekit(&server, 115);

AppSettingsService appSettingsService =
    AppSettingsService(&server, esp32sveltekit.getFS(), esp32sveltekit.getSecurityManager());

MicStateService micStateService = MicStateService(&server,
                                                        esp32sveltekit.getSecurityManager(),
                                                        esp32sveltekit.getMqttClient(),
                                                        &appSettingsService,
                                                        esp32sveltekit.getNotificationEvents());




#define SERIAL_BAUD_RATE 115200
void setup()
{
  // start serial and filesystem
  Serial.begin(SERIAL_BAUD_RATE);

  // set output pins for collar RF
  pinMode(RF_PIN, OUTPUT);

  delay(1000); // Safety
    
  if (!OpenShock::CommandHandler::Init()) {
    ESP_LOGW(TAG, "Unable to initialize OpenShock");
  } else {
    OpenShock::CommandHandler::SetRfTxPin(RF_PIN);
  }

  esp32sveltekit.begin();
  appSettingsService.begin();
  micStateService.begin();
}

void loop()
{
  vTaskDelete(NULL);
}
