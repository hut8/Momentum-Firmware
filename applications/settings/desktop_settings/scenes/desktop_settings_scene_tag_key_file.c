#include <furi.h>
#include <gui/scene_manager.h>
#include <gui/modules/popup.h>
#include <dialogs/dialogs.h>
#include <nfc/nfc_device.h>
#include <lfrfid/lfrfid_dict_file.h>
#include <lfrfid/protocols/lfrfid_protocols.h>
#include <toolbox/protocols/protocol_dict.h>
#include <storage/storage.h>

#include "../desktop_settings_app.h"
#include "desktop_settings_scene.h"
#include "../desktop_settings_custom_event.h"

static bool desktop_settings_tag_key_file_load_nfc(DesktopSettingsApp* app, const char* path) {
    NfcDevice* dev = nfc_device_alloc();
    bool success = false;

    if(nfc_device_load(dev, path)) {
        size_t uid_len;
        const uint8_t* uid = nfc_device_get_uid(dev, &uid_len);

        if(uid && uid_len > 0) {
            DesktopTagKey* tag_key = &app->tag_key_buffer;
            tag_key->type = DesktopTagKeyTypeNfc;
            tag_key->data_length =
                uid_len > DESKTOP_TAG_KEY_DATA_MAX_LEN ? DESKTOP_TAG_KEY_DATA_MAX_LEN : uid_len;
            memcpy(tag_key->data, uid, tag_key->data_length);
            tag_key->rfid_protocol = 0;
            success = true;
        }
    }

    nfc_device_free(dev);
    return success;
}

static bool desktop_settings_tag_key_file_load_rfid(DesktopSettingsApp* app, const char* path) {
    ProtocolDict* dict = protocol_dict_alloc(lfrfid_protocols, LFRFIDProtocolMax);
    bool success = false;

    ProtocolId protocol = lfrfid_dict_file_load(dict, path);
    if(protocol != PROTOCOL_NO) {
        size_t data_size = protocol_dict_get_data_size(dict, protocol);
        uint8_t* temp_data = malloc(data_size);
        protocol_dict_get_data(dict, protocol, temp_data, data_size);

        DesktopTagKey* tag_key = &app->tag_key_buffer;
        tag_key->type = DesktopTagKeyTypeRfid;
        tag_key->rfid_protocol = (uint8_t)protocol;
        tag_key->data_length =
            data_size > DESKTOP_TAG_KEY_DATA_MAX_LEN ? DESKTOP_TAG_KEY_DATA_MAX_LEN : data_size;
        memcpy(tag_key->data, temp_data, tag_key->data_length);
        free(temp_data);
        success = true;
    }

    protocol_dict_free(dict);
    return success;
}

void desktop_settings_scene_tag_key_file_on_enter(void* context) {
    DesktopSettingsApp* app = context;

    uint32_t tag_type = scene_manager_get_scene_state(
        app->scene_manager, DesktopSettingsAppSceneTagKeyFile);

    DialogsFileBrowserOptions browser_options;
    FuriString* path = furi_string_alloc();
    bool file_selected = false;

    if(tag_type == DesktopTagKeyTypeNfc) {
        dialog_file_browser_set_basic_options(&browser_options, ".nfc", NULL);
        browser_options.base_path = EXT_PATH("nfc");
        furi_string_set_str(path, browser_options.base_path);
        file_selected = dialog_file_browser_show(app->dialogs, path, path, &browser_options);
    } else if(tag_type == DesktopTagKeyTypeRfid) {
        dialog_file_browser_set_basic_options(&browser_options, ".rfid", NULL);
        browser_options.base_path = EXT_PATH("lfrfid");
        furi_string_set_str(path, browser_options.base_path);
        file_selected = dialog_file_browser_show(app->dialogs, path, path, &browser_options);
    }

    if(file_selected) {
        bool load_success = false;
        if(tag_type == DesktopTagKeyTypeNfc) {
            load_success =
                desktop_settings_tag_key_file_load_nfc(app, furi_string_get_cstr(path));
        } else if(tag_type == DesktopTagKeyTypeRfid) {
            load_success =
                desktop_settings_tag_key_file_load_rfid(app, furi_string_get_cstr(path));
        }

        if(load_success) {
            desktop_tag_key_save(&app->tag_key_buffer);
            popup_set_header(app->popup, "Tag Saved!", 64, 20, AlignCenter, AlignCenter);
            popup_set_text(
                app->popup,
                tag_type == DesktopTagKeyTypeNfc ? "NFC tag set\nas unlock key" :
                                                     "RFID tag set\nas unlock key",
                64,
                40,
                AlignCenter,
                AlignCenter);
            view_dispatcher_switch_to_view(app->view_dispatcher, DesktopSettingsAppViewIdPopup);
        } else {
            popup_set_header(app->popup, "Error!", 64, 20, AlignCenter, AlignCenter);
            popup_set_text(
                app->popup, "Failed to load\ntag data", 64, 40, AlignCenter, AlignCenter);
            view_dispatcher_switch_to_view(app->view_dispatcher, DesktopSettingsAppViewIdPopup);
        }
    } else {
        scene_manager_previous_scene(app->scene_manager);
    }

    furi_string_free(path);
}

bool desktop_settings_scene_tag_key_file_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        DesktopSettingsApp* app = context;
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void desktop_settings_scene_tag_key_file_on_exit(void* context) {
    DesktopSettingsApp* app = context;
    popup_reset(app->popup);
}
