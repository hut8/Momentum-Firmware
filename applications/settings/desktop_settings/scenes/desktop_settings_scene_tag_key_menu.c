#include <gui/scene_manager.h>

#include "../desktop_settings_app.h"
#include "desktop_settings_scene.h"
#include "../desktop_settings_custom_event.h"

static void desktop_settings_scene_tag_key_menu_submenu_callback(void* context, uint32_t index) {
    DesktopSettingsApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void desktop_settings_scene_tag_key_menu_on_enter(void* context) {
    DesktopSettingsApp* app = context;
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);

    submenu_add_item(
        submenu,
        "Scan NFC Tag",
        DesktopSettingsCustomEventScanNfc,
        desktop_settings_scene_tag_key_menu_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "Scan RFID Tag",
        DesktopSettingsCustomEventScanRfid,
        desktop_settings_scene_tag_key_menu_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "From NFC File",
        DesktopSettingsCustomEventFileNfc,
        desktop_settings_scene_tag_key_menu_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "From RFID File",
        DesktopSettingsCustomEventFileRfid,
        desktop_settings_scene_tag_key_menu_submenu_callback,
        app);

    submenu_set_header(submenu, "Set Unlock Tag");
    view_dispatcher_switch_to_view(app->view_dispatcher, DesktopSettingsAppViewMenu);
}

bool desktop_settings_scene_tag_key_menu_on_event(void* context, SceneManagerEvent event) {
    DesktopSettingsApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case DesktopSettingsCustomEventScanNfc:
            scene_manager_next_scene(app->scene_manager, DesktopSettingsAppSceneTagKeyScanNfc);
            consumed = true;
            break;
        case DesktopSettingsCustomEventScanRfid:
            scene_manager_next_scene(app->scene_manager, DesktopSettingsAppSceneTagKeyScanRfid);
            consumed = true;
            break;
        case DesktopSettingsCustomEventFileNfc:
            scene_manager_set_scene_state(
                app->scene_manager, DesktopSettingsAppSceneTagKeyFile, DesktopTagKeyTypeNfc);
            scene_manager_next_scene(app->scene_manager, DesktopSettingsAppSceneTagKeyFile);
            consumed = true;
            break;
        case DesktopSettingsCustomEventFileRfid:
            scene_manager_set_scene_state(
                app->scene_manager, DesktopSettingsAppSceneTagKeyFile, DesktopTagKeyTypeRfid);
            scene_manager_next_scene(app->scene_manager, DesktopSettingsAppSceneTagKeyFile);
            consumed = true;
            break;
        default:
            consumed = true;
            break;
        }
    }

    return consumed;
}

void desktop_settings_scene_tag_key_menu_on_exit(void* context) {
    DesktopSettingsApp* app = context;
    submenu_reset(app->submenu);
}
