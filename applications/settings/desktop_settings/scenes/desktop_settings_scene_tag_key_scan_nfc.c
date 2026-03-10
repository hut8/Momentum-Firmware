#include <furi.h>
#include <gui/scene_manager.h>
#include <gui/modules/popup.h>
#include <notification/notification_messages.h>
#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/nfc_protocol.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>

#include "../desktop_settings_app.h"
#include "desktop_settings_scene.h"
#include "../desktop_settings_custom_event.h"

typedef struct {
    Nfc* nfc;
    NfcPoller* poller;
} TagKeyScanNfcState;

static NfcCommand desktop_settings_tag_key_scan_nfc_callback(
    NfcGenericEvent event,
    void* context) {
    DesktopSettingsApp* app = context;
    NfcCommand command = NfcCommandContinue;

    const Iso14443_3aPollerEvent* iso_event = event.event_data;
    if(iso_event->type == Iso14443_3aPollerEventTypeReady) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, DesktopSettingsCustomEventNfcDetected);
        command = NfcCommandStop;
    } else if(iso_event->type == Iso14443_3aPollerEventTypeError) {
        command = NfcCommandReset;
    }

    return command;
}

void desktop_settings_scene_tag_key_scan_nfc_on_enter(void* context) {
    DesktopSettingsApp* app = context;

    TagKeyScanNfcState* state = malloc(sizeof(TagKeyScanNfcState));
    scene_manager_set_scene_state(
        app->scene_manager, DesktopSettingsAppSceneTagKeyScanNfc, (uint32_t)(uintptr_t)state);

    popup_set_header(app->popup, "Scanning NFC", 64, 14, AlignCenter, AlignCenter);
    popup_set_text(
        app->popup,
        "Place tag on\nFlipper's back",
        64,
        38,
        AlignCenter,
        AlignCenter);
    view_dispatcher_switch_to_view(app->view_dispatcher, DesktopSettingsAppViewIdPopup);

    NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
    notification_message(notifications, &sequence_blink_start_blue);
    furi_record_close(RECORD_NOTIFICATION);

    state->nfc = nfc_alloc();
    state->poller = nfc_poller_alloc(state->nfc, NfcProtocolIso14443_3a);
    nfc_poller_start(state->poller, desktop_settings_tag_key_scan_nfc_callback, app);
}

bool desktop_settings_scene_tag_key_scan_nfc_on_event(void* context, SceneManagerEvent event) {
    DesktopSettingsApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == DesktopSettingsCustomEventNfcDetected) {
            TagKeyScanNfcState* state =
                (TagKeyScanNfcState*)(uintptr_t)scene_manager_get_scene_state(
                    app->scene_manager, DesktopSettingsAppSceneTagKeyScanNfc);

            // Extract UID — poller has stopped, safe to read from main thread
            const NfcDeviceData* nfc_data = nfc_poller_get_data(state->poller);
            const Iso14443_3aData* iso_data = nfc_data;
            size_t uid_len;
            const uint8_t* uid = iso14443_3a_get_uid(iso_data, &uid_len);

            DesktopTagKey* tag_key = &app->tag_key_buffer;
            tag_key->type = DesktopTagKeyTypeNfc;
            tag_key->data_length =
                uid_len > DESKTOP_TAG_KEY_DATA_MAX_LEN ? DESKTOP_TAG_KEY_DATA_MAX_LEN : uid_len;
            memcpy(tag_key->data, uid, tag_key->data_length);
            tag_key->rfid_protocol = 0;

            desktop_tag_key_save(tag_key);

            NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
            notification_message(notifications, &sequence_success);
            furi_record_close(RECORD_NOTIFICATION);

            // Format UID for display
            static char uid_str[48];
            size_t offset = 0;
            for(size_t i = 0; i < tag_key->data_length && offset < sizeof(uid_str) - 4; i++) {
                if(i > 0) {
                    uid_str[offset++] = ':';
                }
                offset += snprintf(
                    uid_str + offset, sizeof(uid_str) - offset, "%02X", tag_key->data[i]);
            }

            static char detail_text[80];
            snprintf(detail_text, sizeof(detail_text), "UID: %s", uid_str);

            popup_set_header(app->popup, "NFC Tag Saved!", 64, 14, AlignCenter, AlignCenter);
            popup_set_text(app->popup, detail_text, 64, 38, AlignCenter, AlignCenter);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void desktop_settings_scene_tag_key_scan_nfc_on_exit(void* context) {
    DesktopSettingsApp* app = context;

    TagKeyScanNfcState* state = (TagKeyScanNfcState*)(uintptr_t)scene_manager_get_scene_state(
        app->scene_manager, DesktopSettingsAppSceneTagKeyScanNfc);

    if(state) {
        if(state->poller) {
            nfc_poller_stop(state->poller);
            nfc_poller_free(state->poller);
        }
        if(state->nfc) {
            nfc_free(state->nfc);
        }
        free(state);
        scene_manager_set_scene_state(
            app->scene_manager, DesktopSettingsAppSceneTagKeyScanNfc, 0);
    }

    NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
    notification_message(notifications, &sequence_blink_stop);
    furi_record_close(RECORD_NOTIFICATION);

    popup_reset(app->popup);
}
