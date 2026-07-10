/**
 * @file fw_update.h
 * @brief Serial Firmware Update (OTA over USB) - Desert-Spec v2.4
 *
 * Allows the companion app to push a new firmware image over the existing
 * USB serial link. The image is written to the inactive OTA slot, verified
 * (CRC32 + ESP-IDF image validation), then the boot partition is switched
 * and the device restarts. No cable re-flash needed.
 *
 * Protocol (ASCII lines, mirrors the IMG_* image upload protocol):
 *   PC  -> ESP: FW_BEGIN:<size>            (size = .bin file size in bytes)
 *   ESP -> PC:  FW_OK:BEGIN                or FW_ERR:<reason>
 *   PC  -> ESP: FW_DATA:<offset>:<hex>     (chunks, offset must be sequential)
 *   ESP -> PC:  FW_OK:DATA:<received>      or FW_ERR:OFFSET:<expected> (resync)
 *   PC  -> ESP: FW_END:<crc32-hex>
 *   ESP -> PC:  FW_OK:COMPLETE             then reboots into the new firmware
 *   PC  -> ESP: FW_ABORT                   (cancel at any time)
 *   PC  -> ESP: GET_FW_VER
 *   ESP -> PC:  FW_VER:<version>:<running-partition>
 */

#ifndef FW_UPDATE_H
#define FW_UPDATE_H

#include <stdbool.h>

/**
 * @brief Handle FW_* / GET_FW_VER commands from serial
 * @param line Command line
 * @return true if the command was handled
 */
bool fw_update_handle_command(const char *line);

#endif /* FW_UPDATE_H */
