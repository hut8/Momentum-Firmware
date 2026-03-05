#include <furi.h>
#include <gui/scene_manager.h>
#include <gui/modules/popup.h>
#include <notification/notification_messages.h>
#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/nfc_protocol.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <lfrfid/lfrfid_worker.h>
#include <lfrfid/protocols/lfrfid_protocols.h>
#include <toolbox/protocols/protocol_dict.h>

#include "../desktop_i.h"
#include "../helpers/card_key.h"
#include "desktop_scene.h"

#define TAG "DesktopCardScan"

#define CARD_SCAN_TIMEOUT_MS (15000)

typedef struct {
    Nfc* nfc;
    NfcPoller* poller;
    ProtocolDict* rfid_dict;
    LFRFIDWorker* rfid_worker;
    DesktopCardKey card_key;
    ProtocolId rfid_protocol_id_next; // Written by worker thread
    ProtocolId rfid_protocol_id; // Read by main thread after event
    FuriTimer* timeout_timer;
} DesktopSceneCardScanState;

static void desktop_scene_card_scan_timeout_callback(void* context) {
    Desktop* desktop = context;
    view_dispatcher_send_custom_event(desktop->view_dispatcher, DesktopCardScanEventTimeout);
}

static NfcCommand desktop_scene_card_scan_nfc_callback(NfcGenericEvent event, void* context) {
    Desktop* desktop = context;
    NfcCommand command = NfcCommandContinue;

    const Iso14443_3aPollerEvent* iso14443_3a_event = event.event_data;
    if(iso14443_3a_event->type == Iso14443_3aPollerEventTypeReady) {
        view_dispatcher_send_custom_event(
            desktop->view_dispatcher, DesktopCardScanEventNfcDetected);
        command = NfcCommandStop;
    } else if(iso14443_3a_event->type == Iso14443_3aPollerEventTypeError) {
        command = NfcCommandReset;
    }

    return command;
}

static void desktop_scene_card_scan_rfid_callback(
    LFRFIDWorkerReadResult result,
    ProtocolId protocol,
    void* context) {
    Desktop* desktop = context;

    if(result == LFRFIDWorkerReadDone) {
        DesktopSceneCardScanState* state = (DesktopSceneCardScanState*)(uintptr_t)
            scene_manager_get_scene_state(desktop->scene_manager, DesktopSceneCardScan);
        if(state) {
            state->rfid_protocol_id_next = protocol;
        }
        view_dispatcher_send_custom_event(
            desktop->view_dispatcher, DesktopCardScanEventRfidDetected);
    }
}

void desktop_scene_card_scan_on_enter(void* context) {
    Desktop* desktop = context;

    DesktopSceneCardScanState* state = malloc(sizeof(DesktopSceneCardScanState));
    memset(state, 0, sizeof(DesktopSceneCardScanState));
    state->rfid_protocol_id_next = PROTOCOL_NO;
    state->rfid_protocol_id = PROTOCOL_NO;

    scene_manager_set_scene_state(
        desktop->scene_manager, DesktopSceneCardScan, (uint32_t)(uintptr_t)state);

    if(!desktop_card_key_load(&state->card_key)) {
        popup_set_header(desktop->popup, "No tag set", 64, 20, AlignCenter, AlignCenter);
        popup_set_text(desktop->popup, "Set tag in\nDesktop Settings", 64, 40, AlignCenter, AlignCenter);
        view_dispatcher_switch_to_view(desktop->view_dispatcher, DesktopViewIdPopup);
        return;
    }

    popup_set_header(desktop->popup, "Present Tag", 64, 20, AlignCenter, AlignCenter);
    popup_set_text(desktop->popup, "Waiting for tag...", 64, 40, AlignCenter, AlignCenter);
    view_dispatcher_switch_to_view(desktop->view_dispatcher, DesktopViewIdPopup);

    state->timeout_timer = furi_timer_alloc(
        desktop_scene_card_scan_timeout_callback, FuriTimerTypeOnce, desktop);
    furi_timer_start(state->timeout_timer, furi_ms_to_ticks(CARD_SCAN_TIMEOUT_MS));

    NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);

    if(state->card_key.type == DesktopCardKeyTypeNfc) {
        notification_message(notifications, &sequence_blink_start_blue);
        state->nfc = nfc_alloc();
        state->poller = nfc_poller_alloc(state->nfc, NfcProtocolIso14443_3a);
        nfc_poller_start(state->poller, desktop_scene_card_scan_nfc_callback, desktop);
    } else if(state->card_key.type == DesktopCardKeyTypeRfid) {
        notification_message(notifications, &sequence_blink_start_cyan);
        state->rfid_dict = protocol_dict_alloc(lfrfid_protocols, LFRFIDProtocolMax);
        state->rfid_worker = lfrfid_worker_alloc(state->rfid_dict);
        lfrfid_worker_start_thread(state->rfid_worker);
        lfrfid_worker_read_start(
            state->rfid_worker,
            LFRFIDWorkerReadTypeAuto,
            desktop_scene_card_scan_rfid_callback,
            desktop);
    }

    furi_record_close(RECORD_NOTIFICATION);
}

bool desktop_scene_card_scan_on_event(void* context, SceneManagerEvent event) {
    Desktop* desktop = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        DesktopSceneCardScanState* state = (DesktopSceneCardScanState*)(uintptr_t)
            scene_manager_get_scene_state(desktop->scene_manager, DesktopSceneCardScan);

        switch(event.event) {
        case DesktopCardScanEventNfcDetected: {
            const NfcDeviceData* nfc_data = nfc_poller_get_data(state->poller);
            const Iso14443_3aData* iso_data = nfc_data;
            size_t uid_len;
            const uint8_t* uid = iso14443_3a_get_uid(iso_data, &uid_len);

            NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
            if(desktop_card_key_check_nfc_uid(&state->card_key, uid, uid_len)) {
                notification_message(notifications, &sequence_success);
                furi_record_close(RECORD_NOTIFICATION);
                desktop_unlock(desktop);
            } else {
                notification_message(notifications, &sequence_error);
                furi_record_close(RECORD_NOTIFICATION);
                popup_set_header(desktop->popup, "Wrong Tag!", 64, 20, AlignCenter, AlignCenter);
                popup_set_text(desktop->popup, "Tag does not match", 64, 40, AlignCenter, AlignCenter);
            }
            consumed = true;
            break;
        }
        case DesktopCardScanEventRfidDetected: {
            state->rfid_protocol_id = state->rfid_protocol_id_next;
            ProtocolId protocol = state->rfid_protocol_id;
            if(protocol != PROTOCOL_NO) {
                lfrfid_worker_stop(state->rfid_worker);

                size_t data_size = protocol_dict_get_data_size(state->rfid_dict, protocol);
                uint8_t* data_buf = malloc(data_size);
                protocol_dict_get_data(
                    state->rfid_dict, protocol, data_buf, data_size);

                size_t cmp_size = data_size > DESKTOP_CARD_KEY_DATA_MAX_LEN ?
                                      DESKTOP_CARD_KEY_DATA_MAX_LEN :
                                      data_size;

                NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
                if(desktop_card_key_check_rfid(
                       &state->card_key, (uint8_t)protocol, data_buf, cmp_size)) {
                    notification_message(notifications, &sequence_success);
                    furi_record_close(RECORD_NOTIFICATION);
                    desktop_unlock(desktop);
                } else {
                    notification_message(notifications, &sequence_error);
                    furi_record_close(RECORD_NOTIFICATION);
                    popup_set_header(
                        desktop->popup, "Wrong Tag!", 64, 20, AlignCenter, AlignCenter);
                    popup_set_text(
                        desktop->popup,
                        "Tag does not match",
                        64,
                        40,
                        AlignCenter,
                        AlignCenter);
                }
                free(data_buf);
            }
            consumed = true;
            break;
        }
        case DesktopCardScanEventTimeout:
            scene_manager_previous_scene(desktop->scene_manager);
            consumed = true;
            break;
        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(desktop->scene_manager);
        consumed = true;
    }

    return consumed;
}

void desktop_scene_card_scan_on_exit(void* context) {
    Desktop* desktop = context;

    DesktopSceneCardScanState* state = (DesktopSceneCardScanState*)(uintptr_t)
        scene_manager_get_scene_state(desktop->scene_manager, DesktopSceneCardScan);

    if(state) {
        if(state->timeout_timer) {
            furi_timer_stop(state->timeout_timer);
            furi_timer_free(state->timeout_timer);
        }

        if(state->poller) {
            nfc_poller_stop(state->poller);
            nfc_poller_free(state->poller);
        }
        if(state->nfc) {
            nfc_free(state->nfc);
        }

        if(state->rfid_worker) {
            lfrfid_worker_stop(state->rfid_worker);
            lfrfid_worker_stop_thread(state->rfid_worker);
            lfrfid_worker_free(state->rfid_worker);
        }
        if(state->rfid_dict) {
            protocol_dict_free(state->rfid_dict);
        }

        free(state);
        scene_manager_set_scene_state(desktop->scene_manager, DesktopSceneCardScan, 0);
    }

    NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
    notification_message(notifications, &sequence_blink_stop);
    furi_record_close(RECORD_NOTIFICATION);

    popup_reset(desktop->popup);
}
