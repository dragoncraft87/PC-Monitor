/**
 * @file hw_identity.h
 * @brief Hardware Identity Management
 *
 * Manages hardware names (CPU, GPU) and identity hash for sync with PC client.
 * Persists to LittleFS: /storage/names.txt, /storage/host.hash
 */

#ifndef HW_IDENTITY_H
#define HW_IDENTITY_H

#include <stdint.h>
#include <stdbool.h>

/* File paths */
#define HW_NAMES_FILE_PATH      "/storage/names.txt"
#define HW_HASH_FILE_PATH       "/storage/host.hash"

/* Hardware names structure */
typedef struct {
    char cpu_name[32];
    char gpu_name[32];
    char identity_hash[16];  /* 8 hex chars + null */
} hw_identity_t;

/**
 * @brief Get pointer to global hardware identity
 * @return Pointer to hw_identity_t structure
 */
hw_identity_t *hw_identity_get(void);

/**
 * @brief Load hardware identity from LittleFS
 *
 * Loads names.txt and host.hash from storage.
 * Uses defaults if files don't exist.
 */
void hw_identity_load(void);

/**
 * @brief Save hardware identity to LittleFS
 *
 * Saves current names and hash to storage.
 */
void hw_identity_save(void);

/**
 * @brief Set CPU name
 * @param name New CPU name (will be truncated if too long)
 */
void hw_identity_set_cpu_name(const char *name);

/**
 * @brief Set GPU name
 * @param name New GPU name (will be truncated if too long)
 */
void hw_identity_set_gpu_name(const char *name);

/**
 * @brief Set identity hash
 * @param hash 8-character hex string
 */
void hw_identity_set_hash(const char *hash);

/**
 * @brief Handle NAME_* commands from serial
 * @param line Command line (e.g., "NAME_CPU=i9-7980XE")
 * @return true if command was handled, false otherwise
 */
bool hw_identity_handle_command(const char *line);

#endif /* HW_IDENTITY_H */
