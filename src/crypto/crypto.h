#pragma once

#include "../common.h"
#include <stdbool.h>
#include <stddef.h>

#define ROOM_KEY_LEN 32

typedef struct {
    unsigned char secret_key[64];
    unsigned char public_key[32];
    char public_key_b64[BASE64_PK_LEN];
    bool loaded;
} identity_t;

extern identity_t g_identity;

int  crypto_init(void);
void crypto_cleanup(void);

int  crypto_generate_identity(void);
int  crypto_load_identity(void);
int  crypto_save_identity(void);

int  crypto_encrypt_dm(const char* plaintext, size_t ptlen,
                       const unsigned char* recipient_pk,
                       char* out_ciphertext_b64, size_t ct_b64_size,
                       char* out_nonce_b64, size_t nonce_b64_size,
                       char* out_encrypted_key_b64, size_t ek_b64_size);

int  crypto_decrypt_dm(const char* ciphertext_b64,
                       const char* nonce_b64,
                       const char* encrypted_key_b64,
                       char* out_plaintext, size_t pt_size);

int  crypto_encrypt_room_msg(const char* plaintext, size_t ptlen,
                             const unsigned char* room_key,
                             char* out_ciphertext_b64, size_t ct_b64_size,
                             char* out_nonce_b64, size_t nonce_b64_size);

int  crypto_decrypt_room_msg(const char* ciphertext_b64,
                             const char* nonce_b64,
                             const unsigned char* room_key,
                             char* out_plaintext, size_t pt_size);

int  crypto_seal_room_key(const unsigned char* room_key,
                          const unsigned char* recipient_pk,
                          char* out_b64, size_t out_size);

int  crypto_unseal_room_key(const char* sealed_b64,
                            unsigned char* room_key);

int  crypto_generate_room_key(unsigned char* room_key);

int  crypto_verify_and_trust(const char* node_id, const char* public_key_b64);

int  crypto_is_trusted(const char* node_id);

void crypto_b64_encode(const unsigned char* bin, size_t binlen,
                       char* b64, size_t b64_size);
int  crypto_b64_decode(const char* b64, unsigned char* bin, size_t bin_size);

int  crypto_seal_for_storage(const unsigned char* plaintext, size_t ptlen,
                              unsigned char* sealed, size_t* sealed_len,
                              char* b64_out, size_t b64_size);
int  crypto_unseal_from_storage(const char* b64,
                                 unsigned char* plaintext, size_t pt_size,
                                 unsigned char* sealed_buf, size_t sealed_size);
