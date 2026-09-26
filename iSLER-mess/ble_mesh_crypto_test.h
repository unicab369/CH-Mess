#ifndef ISLER_BLE_MESH_CRYPTO_TEST_H
#define ISLER_BLE_MESH_CRYPTO_TEST_H

#include <stdio.h>
#include "ble_mesh_crypto.h"
#include "micro-ecc/uECC.h"

static int ble_mesh_test_ecc(void) {
    uint8_t private_a[32], public_a[64], secret_a[32];
    uint8_t private_b[32], public_b[64], secret_b[32];
    uECC_RNG_Function previous_rng = uECC_get_rng();
    uECC_Curve curve = uECC_secp256r1();
    uECC_set_rng(GET_RANDOM_BYTES);

    int ok1 = uECC_make_key(public_a, private_a, curve) &&
              uECC_make_key(public_b, private_b, curve);
    printf("ECC key generation: %s\n", ok1 ? "PASS" : "FAIL");
    int ok2 = ok1 &&
              uECC_shared_secret(public_b, private_a, secret_a, curve) &&
              uECC_shared_secret(public_a, private_b, secret_b, curve);
    printf("ECC shared secret: %s\n", ok2 ? "PASS" : "FAIL");

    int success = ok1 && ok2 &&
                  memcmp(secret_a, secret_b, sizeof(secret_a)) == 0;
    uECC_set_rng(previous_rng);
    return success ? 0 : -1;
}

static int aes_cmac_test(void) {
    // NIST SP 800-38B Appendix D.1, AES-128 example 2.
    const uint8_t key[16] = {
        0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
        0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
    };
    const uint8_t message[16] = {
        0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
        0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a
    };
    const uint8_t expected[16] = {
        0x07, 0x0a, 0x16, 0xb4, 0x6b, 0x4d, 0x41, 0x44,
        0xf7, 0x9b, 0xdd, 0x9d, 0xd0, 0x4a, 0x28, 0x7c
    };
    uint8_t actual[16];
    aes_cmac(key, message, sizeof(message), actual);
    return memcmp(actual, expected, sizeof(expected)) == 0 ? 0 : -1;
}

#endif // ISLER_BLE_MESH_CRYPTO_TEST_H
