#include "ble_lib/ble_crypto.h"
#include <stdio.h>

#if defined(CH5xx)
#define AES_BASE ((uint32_t)0x4000c300)
#else
#define AES_BASE ((uint32_t)0x40024300) // For CH32V208
#endif

typedef struct {
	volatile uint32_t CFG;
	volatile uint32_t STA; // Completely (?) useless
	volatile uint32_t some_reg1; // Don't know yet what are these 4 for
	volatile uint32_t some_reg2;
	volatile uint32_t some_reg3;
	volatile uint32_t some_reg4;
	volatile uint32_t data[4];
	volatile uint32_t key[4];
} AES_Type;

#define AES ((AES_Type *) AES_BASE)

void doAES(const uint8_t *key, const uint8_t *in, uint8_t *out, uint8_t enc) {
	uint32_t word;

	// Serialize shared AES hardware against GAP address resolution in the RX ISR.
	uint32_t irq_state = __get_MSTATUS();
	__disable_irq();
	AES->CFG = 0x100; // Don't know what it does, maybe enables the peripheral or clock for it
	AES->CFG = enc?0:2; // Type of operation
	memcpy(&word, key, sizeof(word));
	AES->key[0] = word;
	memcpy(&word, key + 4, sizeof(word));
	AES->key[1] = word;
	memcpy(&word, key + 8, sizeof(word));
	AES->key[2] = word;
	memcpy(&word, key + 12, sizeof(word));
	AES->key[3] = word;
	memcpy(&word, in, sizeof(word));
	AES->data[0] = word;
	memcpy(&word, in + 4, sizeof(word));
	AES->data[1] = word;
	memcpy(&word, in + 8, sizeof(word));
	AES->data[2] = word;
	memcpy(&word, in + 12, sizeof(word));
	AES->data[3] = word;
	// AES->STA &= 0xfffffffd;
	// AES->STA |= 1;
	AES->CFG |= 1; // Start process

	while(AES->CFG & 1); // Wait for it to finish

	word = AES->data[0];
	memcpy(out, &word, sizeof(word));
	word = AES->data[1];
	memcpy(out + 4, &word, sizeof(word));
	word = AES->data[2];
	memcpy(out + 8, &word, sizeof(word));
	word = AES->data[3];
	memcpy(out + 12, &word, sizeof(word));
	__set_MSTATUS(irq_state);
}

void AES_ENCRYPT_BLOCK(const uint8_t *key, const uint8_t *in, uint8_t *out) {
      doAES(key, in, out, 1);
}


void test_ccm(void) {
	const uint8_t key[16] = {
		0x40, 0x41, 0x42, 0x43,
		0x44, 0x45, 0x46, 0x47,
		0x48, 0x49, 0x4A, 0x4B,
		0x4C, 0x4D, 0x4E, 0x4F
	};

	const uint8_t nonce[13] = {
		0x10, 0x11, 0x12, 0x13, 0x14,
		0x15, 0x16, 0x17, 0x18, 0x19,
		0x1A, 0x1B, 0x1C
	};

	const uint8_t aad[] = {
		0x01, 0x02, 0x03, 0x04
	};

	const uint8_t plaintext[] = "CCM test message";
	const size_t plaintext_len = sizeof(plaintext) - 1;

	uint8_t ciphertext[sizeof(plaintext) - 1];
	uint8_t decrypted[sizeof(plaintext) - 1];
	uint8_t tag[4];

	int result = ccm_encrypt_and_tag(
		key,
		nonce, sizeof(nonce),
		aad, sizeof(aad),
		plaintext, plaintext_len,
		ciphertext,
		tag, sizeof(tag)
	);

	if (result != CCM_OK) {
		printf("CCM encrypt: FAIL\n");
		return;
	}

	result = ccm_auth_decrypt(
		key,
		nonce, sizeof(nonce),
		aad, sizeof(aad),
		ciphertext, plaintext_len,
		tag, sizeof(tag),
		decrypted
	);

	printf("CCM decrypt: %s\n",
			result == CCM_OK &&
			memcmp(plaintext, decrypted, plaintext_len) == 0
				? "PASS" : "FAIL");

	// Tamper with the tag. Authentication must fail.
	tag[0] ^= 0x01;

	result = ccm_auth_decrypt(
		key,
		nonce, sizeof(nonce),
		aad, sizeof(aad),
		ciphertext, plaintext_len,
		tag, sizeof(tag),
		decrypted
	);

	printf("CCM tamper test: %s\n",
			result == CCM_ERR_AUTH ? "PASS" : "FAIL");
}
