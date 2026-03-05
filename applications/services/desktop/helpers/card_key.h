#pragma once

#include <stdint.h>
#include <stdbool.h>

#define DESKTOP_CARD_KEY_DATA_MAX_LEN (16)

typedef enum {
    DesktopCardKeyTypeNone,
    DesktopCardKeyTypeNfc,
    DesktopCardKeyTypeRfid,
} DesktopCardKeyType;

typedef struct {
    DesktopCardKeyType type;
    uint8_t data[DESKTOP_CARD_KEY_DATA_MAX_LEN];
    uint8_t data_length;
    uint8_t rfid_protocol; // LFRFIDProtocol enum value (RFID only)
} DesktopCardKey;

bool desktop_card_key_is_set(void);
bool desktop_card_key_load(DesktopCardKey* card_key);
void desktop_card_key_save(const DesktopCardKey* card_key);
void desktop_card_key_reset(void);
bool desktop_card_key_check_nfc_uid(
    const DesktopCardKey* card_key,
    const uint8_t* uid,
    uint8_t uid_len);
bool desktop_card_key_check_rfid(
    const DesktopCardKey* card_key,
    uint8_t protocol,
    const uint8_t* data,
    uint8_t data_len);
