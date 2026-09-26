#include "ccm_impl.h"
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

void doAES(uint32_t * key, uint32_t * in, uint32_t * out, uint8_t enc) {
	AES->CFG = 0x100; // Don't know what it does, maybe enables the peripheral or clock for it
	AES->CFG = enc?0:2; // Type of operation
	AES->key[0] = key[0];
	AES->key[1] = key[1];
	AES->key[2] = key[2];
	AES->key[3] = key[3];
	AES->data[0] = in[0];
	AES->data[1] = in[1];
	AES->data[2] = in[2];
	AES->data[3] = in[3];
	// AES->STA &= 0xfffffffd;
	// AES->STA |= 1;
	AES->CFG |= 1; // Start process

	while(AES->CFG & 1); // Wait for it to finish

	out[0] = AES->data[0];
	out[1] = AES->data[1];
	out[2] = AES->data[2];
	out[3] = AES->data[3];
}

void AES_ENCRYPT_BLOCK(const uint8_t *key, const uint8_t *in, uint8_t *out) {
      uint32_t key_words[4];
      uint32_t input_words[4];
      uint32_t output_words[4];

      memcpy(key_words, key, 16);
      memcpy(input_words, in, 16);
      doAES(key_words, input_words, output_words, 1);
      memcpy(out, output_words, 16);
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
