#include "card_key.h"

#include <saved_struct.h>
#include <storage/storage.h>
#include <string.h>

#define TAG "DesktopCardKey"

#define DESKTOP_CARD_KEY_VER   (1)
#define DESKTOP_CARD_KEY_MAGIC (0x42)
#define DESKTOP_CARD_KEY_PATH  INT_PATH(".desktop_card_key")

bool desktop_card_key_is_set(void) {
    DesktopCardKey card_key;
    if(!desktop_card_key_load(&card_key)) {
        return false;
    }
    return card_key.type != DesktopCardKeyTypeNone;
}

bool desktop_card_key_load(DesktopCardKey* card_key) {
    furi_assert(card_key);

    const bool success = saved_struct_load(
        DESKTOP_CARD_KEY_PATH,
        card_key,
        sizeof(DesktopCardKey),
        DESKTOP_CARD_KEY_MAGIC,
        DESKTOP_CARD_KEY_VER);

    if(!success) {
        memset(card_key, 0, sizeof(DesktopCardKey));
        return false;
    }

    return true;
}

void desktop_card_key_save(const DesktopCardKey* card_key) {
    furi_assert(card_key);

    const bool success = saved_struct_save(
        DESKTOP_CARD_KEY_PATH,
        card_key,
        sizeof(DesktopCardKey),
        DESKTOP_CARD_KEY_MAGIC,
        DESKTOP_CARD_KEY_VER);

    if(!success) {
        FURI_LOG_E(TAG, "Failed to save card key");
    }
}

void desktop_card_key_reset(void) {
    DesktopCardKey card_key;
    memset(&card_key, 0, sizeof(DesktopCardKey));
    desktop_card_key_save(&card_key);
}

bool desktop_card_key_check_nfc_uid(
    const DesktopCardKey* card_key,
    const uint8_t* uid,
    uint8_t uid_len) {
    furi_assert(card_key);
    furi_assert(uid);

    if(card_key->type != DesktopCardKeyTypeNfc) return false;
    if(card_key->data_length != uid_len) return false;
    return memcmp(card_key->data, uid, uid_len) == 0;
}

bool desktop_card_key_check_rfid(
    const DesktopCardKey* card_key,
    uint8_t protocol,
    const uint8_t* data,
    uint8_t data_len) {
    furi_assert(card_key);
    furi_assert(data);

    if(card_key->type != DesktopCardKeyTypeRfid) return false;
    if(card_key->rfid_protocol != protocol) return false;
    if(card_key->data_length != data_len) return false;
    return memcmp(card_key->data, data, data_len) == 0;
}
