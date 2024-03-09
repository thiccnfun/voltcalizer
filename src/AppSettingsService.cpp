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
}

void AppSettingsService::begin()
{
    _httpEndpoint.begin();
    _webSocketServer.begin();
    _fsPersistence.readFromFS();
}
