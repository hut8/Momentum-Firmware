#include <furi.h>
#include <gui/scene_manager.h>
#include <gui/modules/popup.h>
#include <lfrfid/lfrfid_worker.h>
#include <lfrfid/protocols/lfrfid_protocols.h>
#include <toolbox/protocols/protocol_dict.h>

#include "../desktop_settings_app.h"
#include "desktop_settings_scene.h"
#include "../desktop_settings_custom_event.h"

typedef struct {
    ProtocolDict* dict;
    LFRFIDWorker* worker;
    ProtocolId protocol_id;
} CardKeyScanRfidState;

static void desktop_settings_card_key_scan_rfid_callback(
    LFRFIDWorkerReadResult result,
    ProtocolId protocol,
    void* context) {
    DesktopSettingsApp* app = context;

    if(result == LFRFIDWorkerReadDone) {
        CardKeyScanRfidState* state =
            (CardKeyScanRfidState*)(uintptr_t)scene_manager_get_scene_state(
                app->scene_manager, DesktopSettingsAppSceneCardKeyScanRfid);
        if(state) {
            state->protocol_id = protocol;
        }
        view_dispatcher_send_custom_event(
            app->view_dispatcher, DesktopSettingsCustomEventCardKeySaved);
    }
}

void desktop_settings_scene_card_key_scan_rfid_on_enter(void* context) {
    DesktopSettingsApp* app = context;

    CardKeyScanRfidState* state = malloc(sizeof(CardKeyScanRfidState));
    state->protocol_id = PROTOCOL_NO;
    scene_manager_set_scene_state(
        app->scene_manager, DesktopSettingsAppSceneCardKeyScanRfid, (uint32_t)(uintptr_t)state);

    popup_set_header(app->popup, "Scan RFID Card", 64, 20, AlignCenter, AlignCenter);
    popup_set_text(
        app->popup, "Present card\nto Flipper's back", 64, 40, AlignCenter, AlignCenter);
    view_dispatcher_switch_to_view(app->view_dispatcher, DesktopSettingsAppViewIdPopup);

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
        if(event.event == DesktopSettingsCustomEventCardKeySaved) {
            CardKeyScanRfidState* state =
                (CardKeyScanRfidState*)(uintptr_t)scene_manager_get_scene_state(
                    app->scene_manager, DesktopSettingsAppSceneCardKeyScanRfid);

            ProtocolId protocol = state->protocol_id;
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

                popup_set_header(app->popup, "Card Saved!", 64, 20, AlignCenter, AlignCenter);
                popup_set_text(
                    app->popup,
                    "RFID card set\nas unlock key",
                    64,
                    40,
                    AlignCenter,
                    AlignCenter);
            }
            consumed = true;
        }
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

    popup_reset(app->popup);
}
