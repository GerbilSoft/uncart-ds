/**
 * Encryption keys for the Nintendo DS cartridge protocol.
 * Dumped from 3DS itcm memory.
 *
 * References:
 * - http://4dsdev.org/thread.php?id=151
 * - https://www.3dbrew.org/wiki/Memory_layout#ARM9_ITCM
 */

#ifndef UNCART_GAMECART_KEYS_H
#define UNCART_GAMECART_KEYS_H

#ifdef __cplusplus
extern "C" {
#endif

// NTR Blowfish Key1
// itcm.mem: 0x6428
extern const unsigned char NTR_BF_Key1[1048];

// TWL Blowfish Key1
// itcm.mem: 0x53E0
extern const unsigned char TWL_BF_Key1[1048];

#ifdef __cplusplus
}
#endif

#endif /* UNCART_GAMECART_KEYS_H */
