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

AppSettingsService::AppSettingsService(PsychicHttpServer *server, FS *fs, SecurityManager *securityManager, CH8803 *collar) : _httpEndpoint(AppSettings::read,
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
                                                                                                            AuthenticationPredicates::IS_AUTHENTICATED),
                                                                                             _collar(collar)
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

                std::map<String, EventType> actionMap = {
                    {"shock", EventType::COLLAR_SHOCK},
                    {"vibration", EventType::COLLAR_VIBRATION},
                    {"beep", EventType::COLLAR_BEEP}
                };

                JsonObject jsonObject = json.as<JsonObject>();
                PsychicJsonResponse response = PsychicJsonResponse(request, false, DEFAULT_BUFFER_SIZE);
                JsonObject responseObject = response.getRoot();
                String type = jsonObject["type"].as<String>();
                EventType action = actionMap.count(type) > 0 ? actionMap[type] : EventType::COLLAR_BEEP;
                int value = jsonObject["value"].as<int>();
                int duration = jsonObject["duration"].as<int>();

                switch (action)
                {
                    case EventType::COLLAR_SHOCK:
                        _collar->sendShock(value, duration);
                        responseObject["res"] = "ok";
                        break;
                    case EventType::COLLAR_VIBRATION:
                        _collar->sendVibration(value, duration);
                        responseObject["res"] = "ok";
                        break;
                    case EventType::COLLAR_BEEP:
                        _collar->sendAudio(0, duration);
                        responseObject["res"] = "ok";
                        break;
                    default:
                        return request->reply(400);
                }

                return response.send();
            },
            AuthenticationPredicates::IS_AUTHENTICATED
        )
    );

    _httpEndpoint.begin();
    _webSocketServer.begin();
    _fsPersistence.readFromFS();
}
