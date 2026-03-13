#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/popup.h>
#include <notification/notification_messages.h>
#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <lfrfid/lfrfid_worker.h>
#include <lfrfid/protocols/lfrfid_protocols.h>
#include <toolbox/protocols/protocol_dict.h>
#include <desktop/desktop.h>
#include <desktop/helpers/tag_key.h>

#define TAG_SCAN_TIMEOUT_MS 15000

typedef enum {
    TagScannerViewPopup,
} TagScannerView;

typedef enum {
    TagScannerEventNfcDetected,
    TagScannerEventRfidDetected,
    TagScannerEventTimeout,
} TagScannerEvent;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Popup* popup;

    Nfc* nfc;
    NfcPoller* poller;
    ProtocolDict* rfid_dict;
    LFRFIDWorker* rfid_worker;

    DesktopTagKey tag_key;
    ProtocolId rfid_protocol_id_next; // Written by worker thread
    ProtocolId rfid_protocol_id; // Read by main thread after event
    FuriTimer* timeout_timer;
} TagScanner;

static void tag_scanner_timeout_callback(void* context) {
    TagScanner* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, TagScannerEventTimeout);
}

static NfcCommand tag_scanner_nfc_callback(NfcGenericEvent event, void* context) {
    TagScanner* app = context;
    const Iso14443_3aPollerEvent* iso_event = event.event_data;

    if(iso_event->type == Iso14443_3aPollerEventTypeReady) {
        view_dispatcher_send_custom_event(app->view_dispatcher, TagScannerEventNfcDetected);
        return NfcCommandStop;
    } else if(iso_event->type == Iso14443_3aPollerEventTypeError) {
        return NfcCommandReset;
    }

    return NfcCommandContinue;
}

static void tag_scanner_rfid_callback(
    LFRFIDWorkerReadResult result,
    ProtocolId protocol,
    void* context) {
    TagScanner* app = context;

    if(result == LFRFIDWorkerReadDone) {
        app->rfid_protocol_id_next = protocol;
        view_dispatcher_send_custom_event(app->view_dispatcher, TagScannerEventRfidDetected);
    }
}

static bool tag_scanner_custom_event_callback(void* context, uint32_t event) {
    TagScanner* app = context;

    switch(event) {
    case TagScannerEventNfcDetected: {
        const NfcDeviceData* nfc_data = nfc_poller_get_data(app->poller);
        const Iso14443_3aData* iso_data = nfc_data;
        size_t uid_len;
        const uint8_t* uid = iso14443_3a_get_uid(iso_data, &uid_len);

        NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
        if(desktop_tag_key_check_nfc_uid(&app->tag_key, uid, uid_len)) {
            notification_message(notifications, &sequence_success);
            furi_record_close(RECORD_NOTIFICATION);

            nfc_poller_stop(app->poller);

            Desktop* desktop = furi_record_open(RECORD_DESKTOP);
            desktop_api_unlock(desktop);
            furi_record_close(RECORD_DESKTOP);

            view_dispatcher_stop(app->view_dispatcher);
        } else {
            notification_message(notifications, &sequence_error);
            furi_record_close(RECORD_NOTIFICATION);

            popup_set_header(app->popup, "Wrong Tag!", 64, 20, AlignCenter, AlignCenter);
            popup_set_text(
                app->popup, "Tag does not match", 64, 40, AlignCenter, AlignCenter);

            // Restart NFC poller for next attempt
            nfc_poller_stop(app->poller);
            nfc_poller_start(app->poller, tag_scanner_nfc_callback, app);
        }
        return true;
    }
    case TagScannerEventRfidDetected: {
        app->rfid_protocol_id = app->rfid_protocol_id_next;
        ProtocolId protocol = app->rfid_protocol_id;
        if(protocol == PROTOCOL_NO) return true;

        lfrfid_worker_stop(app->rfid_worker);

        size_t data_size = protocol_dict_get_data_size(app->rfid_dict, protocol);
        uint8_t* data_buf = malloc(data_size);
        protocol_dict_get_data(app->rfid_dict, protocol, data_buf, data_size);

        size_t cmp_size =
            data_size > DESKTOP_TAG_KEY_DATA_MAX_LEN ? DESKTOP_TAG_KEY_DATA_MAX_LEN : data_size;

        NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
        if(desktop_tag_key_check_rfid(&app->tag_key, (uint8_t)protocol, data_buf, cmp_size)) {
            notification_message(notifications, &sequence_success);
            furi_record_close(RECORD_NOTIFICATION);
            free(data_buf);

            Desktop* desktop = furi_record_open(RECORD_DESKTOP);
            desktop_api_unlock(desktop);
            furi_record_close(RECORD_DESKTOP);

            view_dispatcher_stop(app->view_dispatcher);
        } else {
            notification_message(notifications, &sequence_error);
            furi_record_close(RECORD_NOTIFICATION);
            free(data_buf);

            popup_set_header(app->popup, "Wrong Tag!", 64, 20, AlignCenter, AlignCenter);
            popup_set_text(
                app->popup, "Tag does not match", 64, 40, AlignCenter, AlignCenter);

            // Restart RFID reading for next attempt
            lfrfid_worker_read_start(
                app->rfid_worker, LFRFIDWorkerReadTypeAuto, tag_scanner_rfid_callback, app);
        }
        return true;
    }
    case TagScannerEventTimeout:
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    default:
        return false;
    }
}

static bool tag_scanner_navigation_event_callback(void* context) {
    UNUSED(context);
    return false; // Return false to exit app on back button
}

int32_t tag_scanner_app(void* p) {
    UNUSED(p);

    TagScanner* app = malloc(sizeof(TagScanner));
    memset(app, 0, sizeof(TagScanner));
    app->rfid_protocol_id_next = PROTOCOL_NO;
    app->rfid_protocol_id = PROTOCOL_NO;

    // Setup GUI
    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, tag_scanner_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, tag_scanner_navigation_event_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->popup = popup_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TagScannerViewPopup, popup_get_view(app->popup));

    // Load tag key
    if(!desktop_tag_key_load(&app->tag_key)) {
        popup_set_header(app->popup, "No Tag Set", 64, 20, AlignCenter, AlignCenter);
        popup_set_text(
            app->popup, "Set tag in\nDesktop Settings", 64, 40, AlignCenter, AlignCenter);
        view_dispatcher_switch_to_view(app->view_dispatcher, TagScannerViewPopup);
        view_dispatcher_run(app->view_dispatcher);

        view_dispatcher_remove_view(app->view_dispatcher, TagScannerViewPopup);
        popup_free(app->popup);
        view_dispatcher_free(app->view_dispatcher);
        furi_record_close(RECORD_GUI);
        free(app);
        return 0;
    }

    // Show scanning popup
    popup_set_header(app->popup, "Present Tag", 64, 20, AlignCenter, AlignCenter);
    popup_set_text(app->popup, "Waiting for tag...", 64, 40, AlignCenter, AlignCenter);
    view_dispatcher_switch_to_view(app->view_dispatcher, TagScannerViewPopup);

    // Start timeout timer
    app->timeout_timer =
        furi_timer_alloc(tag_scanner_timeout_callback, FuriTimerTypeOnce, app);
    furi_timer_start(app->timeout_timer, furi_ms_to_ticks(TAG_SCAN_TIMEOUT_MS));

    // Start scanning based on tag type
    NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);

    if(app->tag_key.type == DesktopTagKeyTypeNfc) {
        notification_message(notifications, &sequence_blink_start_blue);
        app->nfc = nfc_alloc();
        app->poller = nfc_poller_alloc(app->nfc, NfcProtocolIso14443_3a);
        nfc_poller_start(app->poller, tag_scanner_nfc_callback, app);
    } else if(app->tag_key.type == DesktopTagKeyTypeRfid) {
        notification_message(notifications, &sequence_blink_start_cyan);
        app->rfid_dict = protocol_dict_alloc(lfrfid_protocols, LFRFIDProtocolMax);
        app->rfid_worker = lfrfid_worker_alloc(app->rfid_dict);
        lfrfid_worker_start_thread(app->rfid_worker);
        lfrfid_worker_read_start(
            app->rfid_worker, LFRFIDWorkerReadTypeAuto, tag_scanner_rfid_callback, app);
    }

    furi_record_close(RECORD_NOTIFICATION);

    // Run main loop (blocks until view_dispatcher_stop)
    view_dispatcher_run(app->view_dispatcher);

    // Cleanup
    if(app->timeout_timer) {
        furi_timer_stop(app->timeout_timer);
        furi_timer_free(app->timeout_timer);
    }

    if(app->poller) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
    }
    if(app->nfc) {
        nfc_free(app->nfc);
    }

    if(app->rfid_worker) {
        lfrfid_worker_stop(app->rfid_worker);
        lfrfid_worker_stop_thread(app->rfid_worker);
        lfrfid_worker_free(app->rfid_worker);
    }
    if(app->rfid_dict) {
        protocol_dict_free(app->rfid_dict);
    }

    notifications = furi_record_open(RECORD_NOTIFICATION);
    notification_message(notifications, &sequence_blink_stop);
    furi_record_close(RECORD_NOTIFICATION);

    view_dispatcher_remove_view(app->view_dispatcher, TagScannerViewPopup);
    popup_free(app->popup);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);
    free(app);

    return 0;
}
