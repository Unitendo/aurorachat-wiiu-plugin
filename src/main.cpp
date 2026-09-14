#include "client.h"
#include "config.h"
#include "utils/logger.h"

#include <notifications/notifications.h>
#include <nsysnet/_socket.h>
#include <wups.h>


WUPS_PLUGIN_NAME("AuroraChat Plugin");
WUPS_PLUGIN_DESCRIPTION("Receive messages from AuroraChat");
WUPS_PLUGIN_VERSION("v7.0");
WUPS_PLUGIN_AUTHOR("Startendo");
WUPS_PLUGIN_LICENSE("MIT");


WUPS_USE_WUT_DEVOPTAB();
WUPS_USE_STORAGE("aurorachat");


namespace {
AuroraChat::Config BuildConfig(const std::string &login, const std::string &password) {
    AuroraChat::Config config;
    config.host        = "104.236.25.60";
    config.port        = 7070;
    config.login       = login;
    config.password    = password;
    config.room        = "general";
    config.historySize = 1024;
    return config;
}


constexpr double kInfoNotificationSeconds  = 5.0;
constexpr double kErrorNotificationSeconds = 7.0;

void ConfigureNotificationDefaults() {
    NotificationModuleStatus status = NotificationModule_SetDefaultValue(NOTIFICATION_MODULE_NOTIFICATION_TYPE_INFO, NOTIFICATION_MODULE_DEFAULT_OPTION_DURATION_BEFORE_FADE_OUT, kInfoNotificationSeconds);
    if (status != NOTIFICATION_MODULE_RESULT_SUCCESS) {
        DEBUG_FUNCTION_LINE("Failed to set info notification duration: %s", NotificationModule_GetStatusStr(status));
    }

    status = NotificationModule_SetDefaultValue(NOTIFICATION_MODULE_NOTIFICATION_TYPE_ERROR, NOTIFICATION_MODULE_DEFAULT_OPTION_DURATION_BEFORE_FADE_OUT, kErrorNotificationSeconds);
    if (status != NOTIFICATION_MODULE_RESULT_SUCCESS) {
        DEBUG_FUNCTION_LINE("Failed to set error notification duration: %s", NotificationModule_GetStatusStr(status));
    }
}
} // namespace


INITIALIZE_PLUGIN() {
    initLogging();

    NotificationModuleStatus notifStatus = NotificationModule_InitLibrary();
    if (notifStatus == NOTIFICATION_MODULE_RESULT_SUCCESS) {
        ConfigureNotificationDefaults();
    } else {
        DEBUG_FUNCTION_LINE("Failed to init NotificationModule: %s", NotificationModule_GetStatusStr(notifStatus));
    }

    socket_lib_init();

    DEBUG_FUNCTION_LINE("AuroraChat Plugin initialized");

    deinitLogging();
}

DEINITIALIZE_PLUGIN() {
    AuroraChat::Client::Instance().Stop();

    socket_lib_finish();
    NotificationModule_DeInitLibrary();
}

ON_APPLICATION_START() {
    initLogging();

    std::string login, password;
    if (AuroraChat::LoadLogin(login, password)) {
        AuroraChat::Client::Instance().Start(BuildConfig(login, password));
    } else {
        DEBUG_FUNCTION_LINE("No saved AuroraChat account found, not starting client");
        NotificationModule_AddInfoNotification("AuroraChat: log in via the AuroraChat app first");
    }
}

ON_APPLICATION_ENDS() {
    AuroraChat::Client::Instance().Stop();

    deinitLogging();
}

ON_APPLICATION_REQUESTS_EXIT() {
    DEBUG_FUNCTION_LINE("Application exiting");
    AuroraChat::Client::Instance().Stop();
}
