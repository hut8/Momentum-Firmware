#pragma once

#include <stdint.h>
#include <stdbool.h>

#define DESKTOP_TAG_KEY_DATA_MAX_LEN (16)

typedef enum {
    DesktopTagKeyTypeNone,
    DesktopTagKeyTypeNfc,
    DesktopTagKeyTypeRfid,
} DesktopTagKeyType;

typedef struct {
    DesktopTagKeyType type;
    uint8_t data[DESKTOP_TAG_KEY_DATA_MAX_LEN];
    uint8_t data_length;
    uint8_t rfid_protocol; // LFRFIDProtocol enum value (RFID only)
} DesktopTagKey;

bool desktop_tag_key_is_set(void);
bool desktop_tag_key_load(DesktopTagKey* tag_key);
void desktop_tag_key_save(const DesktopTagKey* tag_key);
void desktop_tag_key_reset(void);
bool desktop_tag_key_check_nfc_uid(
    const DesktopTagKey* tag_key,
    const uint8_t* uid,
    uint8_t uid_len);
bool desktop_tag_key_check_rfid(
    const DesktopTagKey* tag_key,
    uint8_t protocol,
    const uint8_t* data,
    uint8_t data_len);
