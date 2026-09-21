#include "ChatComposer.h"
#include "client.h"
#include "config.h"
#include "function_patches.h"
#include "utils/logger.h"

#include <notifications/notifications.h>
#include <nsysnet/_socket.h>
#include <wups.h>
#include <wups/button_combo/api.h>

#include <optional>


WUPS_PLUGIN_NAME("AuroraChat Plugin");
WUPS_PLUGIN_DESCRIPTION("Receive messages from AuroraChat");
WUPS_PLUGIN_VERSION("v7.0");
WUPS_PLUGIN_AUTHOR("Startendo");
WUPS_PLUGIN_LICENSE("GPLv3+");


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

constexpr WUPSButtonCombo_Buttons OPEN_COMPOSER_COMBO = WUPS_BUTTON_COMBO_BUTTON_L | WUPS_BUTTON_COMBO_BUTTON_DOWN | WUPS_BUTTON_COMBO_BUTTON_PLUS;

std::optional<WUPSButtonComboAPI::ButtonCombo> sOpenComposerCombo;

void OnComposerSubmit(const std::string &text) {
    if (!AuroraChat::Client::Instance().SendMessage(text)) {
        NotificationModule_AddErrorNotification("AuroraChat: not connected, message not sent");
    }
}

void OpenComposerCallback(WUPSButtonCombo_ControllerTypes, WUPSButtonCombo_ComboHandle, void *) {
    if (AuroraChat::ChatComposer::Instance().IsActive()) {
        AuroraChat::ChatComposer::Instance().ForceCloseNow();
        return;
    }

    AuroraChat::ChatComposer::Instance().Open(OnComposerSubmit);
}

void InitComposerCombo() {
    WUPSButtonCombo_ComboStatus status = WUPS_BUTTON_COMBO_COMBO_STATUS_INVALID_STATUS;
    WUPSButtonCombo_Error error = WUPS_BUTTON_COMBO_ERROR_UNKNOWN_ERROR;

    auto result = WUPSButtonComboAPI::CreateComboPressDown("AuroraChat: Open Composer", OPEN_COMPOSER_COMBO, OpenComposerCallback, nullptr, status, error);

    if (result && error == WUPS_BUTTON_COMBO_ERROR_SUCCESS) {
        sOpenComposerCombo = std::move(*result); // keep it alive
        if (status == WUPS_BUTTON_COMBO_COMBO_STATUS_CONFLICT) {
            DEBUG_FUNCTION_LINE("AuroraChat composer combo has a CONFLICT and is INACTIVE");
        }
    } else {
        DEBUG_FUNCTION_LINE("Failed to register composer combo: error=%d status=%d", error, status);
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

    if (!AuroraChat::ChatComposer::Instance().Init()) {
        DEBUG_FUNCTION_LINE("Failed to init ChatComposer, composer will be unavailable");
    }
    InitComposerCombo();

    Renderer_AllocateContextState();

    DEBUG_FUNCTION_LINE("AuroraChat Plugin initialized");

    deinitLogging();
}

DEINITIALIZE_PLUGIN() {
    AuroraChat::Client::Instance().Stop();

    AuroraChat::ChatComposer::Instance().Shutdown();

    socket_lib_finish();
    NotificationModule_DeInitLibrary();

    Renderer_Deinitialize();
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
