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

AppSettingsService::AppSettingsService(PsychicHttpServer *server, FS *fs, SecurityManager *securityManager) : _httpEndpoint(AppSettings::read,
                                                                                                                                        AppSettings::update,
                                                                                                                                        this,
                                                                                                                                        server,
                                                                                                                                        APP_SETTINGS_ENDPOINT_PATH,
                                                                                                                                        securityManager,
                                                                                                                                        AuthenticationPredicates::IS_AUTHENTICATED),
                                                                                                                          _fsPersistence(AppSettings::read, AppSettings::update, this, fs, APP_SETTINGS_FILE),
                                                                                           _webSocketServer(AppSettings::read,
                                                                                                            AppSettings::update,
                                                                                                            this,
                                                                                                            server,
                                                                                                            APP_SETTINGS_SOCKET_PATH,
                                                                                                            securityManager,
                                                                                                            AuthenticationPredicates::IS_AUTHENTICATED)
{
    _server = server;
    _securityManager = securityManager;
}

void AppSettingsService::begin()
{

// OPTIONS (for CORS preflight)
#ifdef ENABLE_CORS
        _server->on(TEST_COLLAR_ENDPOINT_PATH,
                    HTTP_OPTIONS,
                    _securityManager->wrapRequest(
                        [this](PsychicRequest *request)
                        {
                            return request->reply(200);
                        },
                        AuthenticationPredicates::IS_AUTHENTICATED));
#endif
    _server->on(TEST_COLLAR_ENDPOINT_PATH,
        HTTP_POST,
        _securityManager->wrapCallback(
            [this](PsychicRequest *request, JsonVariant &json)
            {
                if (!json.is<JsonObject>())
                {
                    return request->reply(400);
                }

                std::map<String, OpenShock::ShockerCommandType> actionMap = {
                    {"shock", OpenShock::ShockerCommandType::Shock},
                    {"vibration", OpenShock::ShockerCommandType::Vibrate},
                    {"beep", OpenShock::ShockerCommandType::Sound}
                };

                JsonObject jsonObject = json.as<JsonObject>();
                PsychicJsonResponse response = PsychicJsonResponse(request, false, DEFAULT_BUFFER_SIZE);
                JsonObject responseObject = response.getRoot();
                String type = jsonObject["type"].as<String>();
                OpenShock::ShockerCommandType action = 
                    actionMap.count(type) > 0 ? actionMap[type] : OpenShock::ShockerCommandType::Sound;
                int intensity = jsonObject["value"].as<int>();
                int duration = jsonObject["duration"].as<int>();

                if (!OpenShock::CommandHandler::Ok())
                {
                    responseObject["res"] = "collar not initialized";
                    return response.send();
                }

                responseObject["res"] = 
                    OpenShock::CommandHandler::HandleCommand(
                        OpenShock::ShockerModelType::CaiXianlin, 0, action, intensity, duration
                    ) ? "ok" : "failed";

                return response.send();
            },
            AuthenticationPredicates::IS_AUTHENTICATED
        )
    );

    _httpEndpoint.begin();
    _webSocketServer.begin();
    _fsPersistence.readFromFS();
}
