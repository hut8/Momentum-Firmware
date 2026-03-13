#include "tag_key.h"

#include <saved_struct.h>
#include <storage/storage.h>
#include <string.h>

#define TAG "DesktopTagKey"

#define DESKTOP_TAG_KEY_VER   (1)
#define DESKTOP_TAG_KEY_MAGIC (0x42)
#define DESKTOP_TAG_KEY_PATH  INT_PATH(".desktop_tag_key")

bool desktop_tag_key_is_set(void) {
    DesktopTagKey tag_key;
    if(!desktop_tag_key_load(&tag_key)) {
        return false;
    }
    return tag_key.type != DesktopTagKeyTypeNone;
}

bool desktop_tag_key_load(DesktopTagKey* tag_key) {
    furi_assert(tag_key);

    const bool success = saved_struct_load(
        DESKTOP_TAG_KEY_PATH,
        tag_key,
        sizeof(DesktopTagKey),
        DESKTOP_TAG_KEY_MAGIC,
        DESKTOP_TAG_KEY_VER);

    if(!success) {
        memset(tag_key, 0, sizeof(DesktopTagKey));
        return false;
    }

    return true;
}

void desktop_tag_key_save(const DesktopTagKey* tag_key) {
    furi_assert(tag_key);

    const bool success = saved_struct_save(
        DESKTOP_TAG_KEY_PATH,
        tag_key,
        sizeof(DesktopTagKey),
        DESKTOP_TAG_KEY_MAGIC,
        DESKTOP_TAG_KEY_VER);

    if(!success) {
        FURI_LOG_E(TAG, "Failed to save tag key");
    }
}

void desktop_tag_key_reset(void) {
    DesktopTagKey tag_key;
    memset(&tag_key, 0, sizeof(DesktopTagKey));
    desktop_tag_key_save(&tag_key);
}

bool desktop_tag_key_check_nfc_uid(
    const DesktopTagKey* tag_key,
    const uint8_t* uid,
    uint8_t uid_len) {
    furi_assert(tag_key);
    furi_assert(uid);

    if(tag_key->type != DesktopTagKeyTypeNfc) return false;
    if(tag_key->data_length != uid_len) return false;
    return memcmp(tag_key->data, uid, uid_len) == 0;
}

bool desktop_tag_key_check_rfid(
    const DesktopTagKey* tag_key,
    uint8_t protocol,
    const uint8_t* data,
    uint8_t data_len) {
    furi_assert(tag_key);
    furi_assert(data);

    if(tag_key->type != DesktopTagKeyTypeRfid) return false;
    if(tag_key->rfid_protocol != protocol) return false;
    if(tag_key->data_length != data_len) return false;
    return memcmp(tag_key->data, data, data_len) == 0;
}
