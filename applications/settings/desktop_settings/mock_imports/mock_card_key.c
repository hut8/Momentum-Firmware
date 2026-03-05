// Since settings app is external, it cannot access firmware functions
// For simple utils like this, easier to include C code in app rather than exposing to API
// Instead of copying the file, can (ab)use the preprocessor to insert the source code here
// Then, we still use the Header from original code as if nothing happened

// desktop_card_key_is_set(), desktop_card_key_load(), desktop_card_key_save(), desktop_card_key_reset(), desktop_card_key_check_nfc_uid(), desktop_card_key_check_rfid()
#include <applications/services/desktop/helpers/card_key.c>
