#include <furi.h>
#include <gui/scene_manager.h>
#include <gui/modules/popup.h>
#include <notification/notification_messages.h>
#include <lfrfid/lfrfid_worker.h>
#include <lfrfid/protocols/lfrfid_protocols.h>
#include <toolbox/protocols/protocol_dict.h>

#include "../desktop_settings_app.h"
#include "desktop_settings_scene.h"
#include "../desktop_settings_custom_event.h"

typedef struct {
    ProtocolDict* dict;
    LFRFIDWorker* worker;
    ProtocolId protocol_id_next; // Written by worker thread
    ProtocolId protocol_id; // Read by main thread after event
} CardKeyScanRfidState;

static void desktop_settings_card_key_scan_rfid_callback(
    LFRFIDWorkerReadResult result,
    ProtocolId protocol,
    void* context) {
    DesktopSettingsApp* app = context;
    uint32_t event = 0;

    if(result == LFRFIDWorkerReadSenseStart) {
        event = DesktopSettingsCustomEventRfidSenseStart;
    } else if(result == LFRFIDWorkerReadSenseEnd) {
        event = DesktopSettingsCustomEventRfidSenseEnd;
    } else if(result == LFRFIDWorkerReadSenseCardStart) {
        event = DesktopSettingsCustomEventRfidSenseCardStart;
    } else if(result == LFRFIDWorkerReadSenseCardEnd) {
        event = DesktopSettingsCustomEventRfidSenseCardEnd;
    } else if(result == LFRFIDWorkerReadDone) {
        // Store protocol_id_next from worker thread — main thread reads it after event
        CardKeyScanRfidState* state =
            (CardKeyScanRfidState*)(uintptr_t)scene_manager_get_scene_state(
                app->scene_manager, DesktopSettingsAppSceneCardKeyScanRfid);
        if(state) {
            state->protocol_id_next = protocol;
        }
        event = DesktopSettingsCustomEventRfidReadDone;
    } else if(result == LFRFIDWorkerReadStartASK) {
        event = DesktopSettingsCustomEventRfidReadStartASK;
    } else if(result == LFRFIDWorkerReadStartPSK) {
        event = DesktopSettingsCustomEventRfidReadStartPSK;
    } else {
        return;
    }

    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

void desktop_settings_scene_card_key_scan_rfid_on_enter(void* context) {
    DesktopSettingsApp* app = context;

    CardKeyScanRfidState* state = malloc(sizeof(CardKeyScanRfidState));
    state->protocol_id_next = PROTOCOL_NO;
    state->protocol_id = PROTOCOL_NO;
    scene_manager_set_scene_state(
        app->scene_manager, DesktopSettingsAppSceneCardKeyScanRfid, (uint32_t)(uintptr_t)state);

    popup_set_header(app->popup, "Scanning RFID", 64, 14, AlignCenter, AlignCenter);
    popup_set_text(
        app->popup,
        "Place tag on\nFlipper's back",
        64,
        38,
        AlignCenter,
        AlignCenter);
    view_dispatcher_switch_to_view(app->view_dispatcher, DesktopSettingsAppViewIdPopup);

    NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
    notification_message(notifications, &sequence_blink_start_cyan);
    furi_record_close(RECORD_NOTIFICATION);

    state->dict = protocol_dict_alloc(lfrfid_protocols, LFRFIDProtocolMax);
    state->worker = lfrfid_worker_alloc(state->dict);
    lfrfid_worker_start_thread(state->worker);
    lfrfid_worker_read_start(
        state->worker,
        LFRFIDWorkerReadTypeAuto,
        desktop_settings_card_key_scan_rfid_callback,
        app);
}

bool desktop_settings_scene_card_key_scan_rfid_on_event(void* context, SceneManagerEvent event) {
    DesktopSettingsApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        CardKeyScanRfidState* state =
            (CardKeyScanRfidState*)(uintptr_t)scene_manager_get_scene_state(
                app->scene_manager, DesktopSettingsAppSceneCardKeyScanRfid);

        NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);

        switch(event.event) {
        case DesktopSettingsCustomEventRfidSenseStart:
            notification_message(notifications, &sequence_blink_start_yellow);
            popup_set_text(
                app->popup,
                "Sensing tag...",
                64,
                38,
                AlignCenter,
                AlignCenter);
            consumed = true;
            break;
        case DesktopSettingsCustomEventRfidSenseCardStart:
            notification_message(notifications, &sequence_blink_start_green);
            popup_set_text(
                app->popup,
                "Tag detected!\nReading...",
                64,
                38,
                AlignCenter,
                AlignCenter);
            consumed = true;
            break;
        case DesktopSettingsCustomEventRfidSenseEnd:
        case DesktopSettingsCustomEventRfidSenseCardEnd:
            notification_message(notifications, &sequence_blink_start_cyan);
            popup_set_text(
                app->popup,
                "Place tag on\nFlipper's back",
                64,
                38,
                AlignCenter,
                AlignCenter);
            consumed = true;
            break;
        case DesktopSettingsCustomEventRfidReadStartASK:
            popup_set_text(
                app->popup,
                "Reading ASK...\nKeep tag still",
                64,
                38,
                AlignCenter,
                AlignCenter);
            consumed = true;
            break;
        case DesktopSettingsCustomEventRfidReadStartPSK:
            popup_set_text(
                app->popup,
                "Reading PSK...\nKeep tag still",
                64,
                38,
                AlignCenter,
                AlignCenter);
            consumed = true;
            break;
        case DesktopSettingsCustomEventRfidReadDone: {
            ProtocolId protocol = state->protocol_id_next;
            state->protocol_id = protocol;

            if(protocol != PROTOCOL_NO) {
                lfrfid_worker_stop(state->worker);

                size_t data_size = protocol_dict_get_data_size(state->dict, protocol);
                DesktopCardKey* card_key = &app->card_key_buffer;
                card_key->type = DesktopCardKeyTypeRfid;
                card_key->rfid_protocol = (uint8_t)protocol;
                card_key->data_length = data_size > DESKTOP_CARD_KEY_DATA_MAX_LEN ?
                                            DESKTOP_CARD_KEY_DATA_MAX_LEN :
                                            data_size;
                protocol_dict_get_data(
                    state->dict, protocol, card_key->data, card_key->data_length);

                desktop_card_key_save(card_key);

                const char* protocol_name =
                    protocol_dict_get_name(state->dict, protocol);

                NotificationApp* notifications =
                    furi_record_open(RECORD_NOTIFICATION);
                notification_message(notifications, &sequence_success);
                furi_record_close(RECORD_NOTIFICATION);

                popup_set_header(
                    app->popup, "Tag Saved!", 64, 14, AlignCenter, AlignCenter);

                // Show the protocol name
                static char detail_text[64];
                snprintf(
                    detail_text,
                    sizeof(detail_text),
                    "%s tag set\nas unlock key",
                    protocol_name ? protocol_name : "RFID");
                popup_set_text(
                    app->popup, detail_text, 64, 38, AlignCenter, AlignCenter);
            }
            consumed = true;
            break;
        }
        default:
            break;
        }

        furi_record_close(RECORD_NOTIFICATION);
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void desktop_settings_scene_card_key_scan_rfid_on_exit(void* context) {
    DesktopSettingsApp* app = context;

    CardKeyScanRfidState* state =
        (CardKeyScanRfidState*)(uintptr_t)scene_manager_get_scene_state(
            app->scene_manager, DesktopSettingsAppSceneCardKeyScanRfid);

    if(state) {
        if(state->worker) {
            lfrfid_worker_stop(state->worker);
            lfrfid_worker_stop_thread(state->worker);
            lfrfid_worker_free(state->worker);
        }
        if(state->dict) {
            protocol_dict_free(state->dict);
        }
        free(state);
        scene_manager_set_scene_state(
            app->scene_manager, DesktopSettingsAppSceneCardKeyScanRfid, 0);
    }

    NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
    notification_message(notifications, &sequence_blink_stop);
    furi_record_close(RECORD_NOTIFICATION);

    popup_reset(app->popup);
}
