#include "os.h"
#include "cx.h"
#include <stdbool.h>
#include <stdlib.h>
#include "utils.h"
#include "menu.h"

void getPublicKey(
    uint32_t *derivationPath,
    uint8_t *publicKeyArray,
    uint8_t pathLength
) {
    cx_ecfp_private_key_t privateKey;
    cx_ecfp_public_key_t publicKey;

    getPrivateKey(derivationPath, &privateKey, pathLength);
    cx_ecfp_generate_pair(CX_CURVE_Ed25519, &publicKey, &privateKey, 1);
    explicit_bzero(&privateKey, sizeof(privateKey));

    for (int i = 0; i < 32; i++) {
        publicKeyArray[i] = publicKey.W[64 - i];
    }
    if ((publicKey.W[32] & 1) != 0) {
        publicKeyArray[31] |= 0x80;
    }
}

uint32_t readUint32BE(uint8_t *buffer) {
    return (
        (buffer[0] << 24) | (buffer[1] << 16) |
        (buffer[2] << 8) | (buffer[3])
    );
}

void getPrivateKey(
    uint32_t *derivationPath,
    cx_ecfp_private_key_t *privateKey,
    uint8_t pathLength
) {
    uint8_t privateKeyData[32];

    os_perso_derive_node_bip32_seed_key(
        HDW_ED25519_SLIP10,
        CX_CURVE_Ed25519,
        derivationPath,
        pathLength,
        privateKeyData,
        NULL,
        NULL,
        0
    );
    cx_ecfp_init_private_key(CX_CURVE_Ed25519, privateKeyData, 32, privateKey);
    explicit_bzero(privateKeyData, sizeof(privateKeyData));
}

void sendResponse(uint8_t tx, bool approve) {
    G_io_apdu_buffer[tx++] = approve? 0x90 : 0x69;
    G_io_apdu_buffer[tx++] = approve? 0x00 : 0x85;
    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, tx);
    // Display back the original UX
    ui_idle();
}

unsigned int ui_prepro(const bagl_element_t *element) {
    unsigned int display = 1;
    if (element->component.userid > 0) {
        display = (ux_step == element->component.userid - 1);
        if (display) {
            if (element->component.userid == 1) {
                UX_CALLBACK_SET_INTERVAL(2000);
            }
            else {
                UX_CALLBACK_SET_INTERVAL(
                    MAX(
                        3000,
                        1000 + bagl_label_roundtrip_duration_ms(element, 7)
                ));
            }
        }
    }
    return display;
}
