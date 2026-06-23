#include "crypto.h"
#include "../persistence/storage.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_SODIUM
#include <sodium.h>
#endif

identity_t g_identity;

#ifdef HAVE_SODIUM

static const unsigned char* get_sk(void) { return g_identity.secret_key; }
static const unsigned char* get_pk(void) { return g_identity.public_key; }

int crypto_init(void) {
    if (sodium_init() < 0) return -1;
    memset(&g_identity, 0, sizeof(g_identity));
    return 0;
}

void crypto_cleanup(void) {
    sodium_memzero(&g_identity, sizeof(g_identity));
}

int crypto_generate_identity(void) {
    crypto_sign_keypair(g_identity.public_key, g_identity.secret_key);
    crypto_b64_encode(g_identity.public_key, 32,
                      g_identity.public_key_b64, sizeof(g_identity.public_key_b64));
    g_identity.loaded = true;
    return crypto_save_identity();
}

int crypto_load_identity(void) {
    unsigned char sk[64], pk[32];
    if (storage_load_identity(sk, sizeof(sk), pk, sizeof(pk)) != 0) return -1;
    memcpy(g_identity.secret_key, sk, 64);
    memcpy(g_identity.public_key, pk, 32);
    crypto_b64_encode(pk, 32,
                      g_identity.public_key_b64, sizeof(g_identity.public_key_b64));
    g_identity.loaded = true;
    sodium_memzero(sk, sizeof(sk));
    return 0;
}

int crypto_save_identity(void) {
    return storage_save_identity(g_identity.secret_key, sizeof(g_identity.secret_key),
                                 g_identity.public_key, sizeof(g_identity.public_key));
}

int crypto_encrypt_dm(const char* plaintext, size_t ptlen,
                       const unsigned char* recipient_pk,
                       char* out_ciphertext_b64, size_t ct_b64_size,
                       char* out_nonce_b64, size_t nonce_b64_size,
                       char* out_encrypted_key_b64, size_t ek_b64_size) {
    unsigned char session_key[crypto_aead_xchacha20poly1305_ietf_KEYBYTES];
    unsigned char nonce[crypto_aead_xchacha20poly1305_ietf_NPUBBYTES];
    unsigned char ciphertext[ptlen + crypto_aead_xchacha20poly1305_ietf_ABYTES];
    unsigned long long ctlen;

    randombytes_buf(session_key, sizeof(session_key));
    randombytes_buf(nonce, sizeof(nonce));

    if (crypto_aead_xchacha20poly1305_ietf_encrypt(
            ciphertext, &ctlen,
            (const unsigned char*)plaintext, ptlen,
            NULL, 0, NULL, nonce, session_key) != 0) {
        sodium_memzero(session_key, sizeof(session_key));
        return -1;
    }

    unsigned char curve_pk[crypto_box_PUBLICKEYBYTES];
    if (crypto_sign_ed25519_pk_to_curve25519(curve_pk, recipient_pk) != 0) {
        sodium_memzero(session_key, sizeof(session_key));
        return -1;
    }

    unsigned char sealed[crypto_box_SEALBYTES + sizeof(session_key)];
    if (crypto_box_seal(sealed, session_key, sizeof(session_key),
                        curve_pk) != 0) {
        sodium_memzero(session_key, sizeof(session_key));
        return -1;
    }

    sodium_memzero(session_key, sizeof(session_key));

    crypto_b64_encode(nonce, sizeof(nonce), out_nonce_b64, nonce_b64_size);
    crypto_b64_encode(ciphertext, (size_t)ctlen, out_ciphertext_b64, ct_b64_size);
    crypto_b64_encode(sealed, sizeof(sealed), out_encrypted_key_b64, ek_b64_size);

    sodium_memzero(nonce, sizeof(nonce));
    sodium_memzero(ciphertext, sizeof(ciphertext));
    return 0;
}

int crypto_decrypt_dm(const char* ciphertext_b64,
                       const char* nonce_b64,
                       const char* encrypted_key_b64,
                       char* out_plaintext, size_t pt_size) {
    (void)pt_size;
    unsigned char session_key[crypto_aead_xchacha20poly1305_ietf_KEYBYTES];
    unsigned char nonce[crypto_aead_xchacha20poly1305_ietf_NPUBBYTES];
    unsigned char ciphertext[MAX_PAYLOAD_LEN + crypto_aead_xchacha20poly1305_ietf_ABYTES];
    unsigned char sealed[sizeof(session_key) + crypto_box_SEALBYTES];
    unsigned long long ctlen;

    if (crypto_b64_decode(nonce_b64, nonce, sizeof(nonce)) != 0) return -1;
    if (crypto_b64_decode(ciphertext_b64, ciphertext, sizeof(ciphertext)) != 0) return -1;
    if (crypto_b64_decode(encrypted_key_b64, sealed, sizeof(sealed)) != 0) return -1;

    unsigned char curve_sk[crypto_box_SECRETKEYBYTES];
    if (crypto_sign_ed25519_sk_to_curve25519(curve_sk, get_sk()) != 0) return -1;

    if (crypto_box_seal_open(session_key, sealed, sizeof(sealed),
                             curve_sk, get_pk()) != 0) {
        sodium_memzero(curve_sk, sizeof(curve_sk));
        return -1;
    }
    sodium_memzero(curve_sk, sizeof(curve_sk));

    ctlen = (strlen(ciphertext_b64) * 3) / 4;
    if (ctlen > sizeof(ciphertext)) ctlen = sizeof(ciphertext);

    unsigned long long ptlen;
    if (crypto_aead_xchacha20poly1305_ietf_decrypt(
            (unsigned char*)out_plaintext, &ptlen,
            NULL, ciphertext, ctlen, NULL, 0, nonce, session_key) != 0) {
        sodium_memzero(session_key, sizeof(session_key));
        return -1;
    }

    sodium_memzero(session_key, sizeof(session_key));
    if (ptlen < pt_size) out_plaintext[ptlen] = '\0';
    return (int)ptlen;
}

int crypto_encrypt_room_msg(const char* plaintext, size_t ptlen,
                             const unsigned char* room_key,
                             char* out_ciphertext_b64, size_t ct_b64_size,
                             char* out_nonce_b64, size_t nonce_b64_size) {
    unsigned char nonce[crypto_aead_xchacha20poly1305_ietf_NPUBBYTES];
    unsigned char ciphertext[ptlen + crypto_aead_xchacha20poly1305_ietf_ABYTES];
    unsigned long long ctlen;

    randombytes_buf(nonce, sizeof(nonce));

    if (crypto_aead_xchacha20poly1305_ietf_encrypt(
            ciphertext, &ctlen,
            (const unsigned char*)plaintext, ptlen,
            NULL, 0, NULL, nonce, room_key) != 0) {
        return -1;
    }

    crypto_b64_encode(nonce, sizeof(nonce), out_nonce_b64, nonce_b64_size);
    crypto_b64_encode(ciphertext, (size_t)ctlen, out_ciphertext_b64, ct_b64_size);
    sodium_memzero(nonce, sizeof(nonce));
    return 0;
}

int crypto_decrypt_room_msg(const char* ciphertext_b64,
                             const char* nonce_b64,
                             const unsigned char* room_key,
                             char* out_plaintext, size_t pt_size) {
    (void)pt_size;
    unsigned char nonce[crypto_aead_xchacha20poly1305_ietf_NPUBBYTES];
    unsigned char ciphertext[MAX_PAYLOAD_LEN + crypto_aead_xchacha20poly1305_ietf_ABYTES];
    unsigned long long ctlen;

    if (crypto_b64_decode(nonce_b64, nonce, sizeof(nonce)) != 0) return -1;
    if (crypto_b64_decode(ciphertext_b64, ciphertext, sizeof(ciphertext)) != 0) return -1;

    ctlen = (strlen(ciphertext_b64) * 3) / 4;
    if (ctlen > sizeof(ciphertext)) ctlen = sizeof(ciphertext);

    unsigned long long ptlen;
    if (crypto_aead_xchacha20poly1305_ietf_decrypt(
            (unsigned char*)out_plaintext, &ptlen,
            NULL, ciphertext, ctlen, NULL, 0, nonce, room_key) != 0) {
        return -1;
    }

    if (ptlen < pt_size) out_plaintext[ptlen] = '\0';
    return (int)ptlen;
}

int crypto_seal_room_key(const unsigned char* room_key,
                          const unsigned char* recipient_pk,
                          char* out_b64, size_t out_size) {
    unsigned char curve_pk[crypto_box_PUBLICKEYBYTES];
    if (crypto_sign_ed25519_pk_to_curve25519(curve_pk, recipient_pk) != 0) {
        return -1;
    }

    unsigned char sealed[crypto_box_SEALBYTES + ROOM_KEY_LEN];
    if (crypto_box_seal(sealed, room_key, ROOM_KEY_LEN, curve_pk) != 0) {
        return -1;
    }

    crypto_b64_encode(sealed, sizeof(sealed), out_b64, out_size);
    return 0;
}

int crypto_unseal_room_key(const char* sealed_b64, unsigned char* room_key) {
    unsigned char sealed[crypto_box_SEALBYTES + ROOM_KEY_LEN];

    if (crypto_b64_decode(sealed_b64, sealed, sizeof(sealed)) != 0) return -1;

    unsigned char curve_sk[crypto_box_SECRETKEYBYTES];
    if (crypto_sign_ed25519_sk_to_curve25519(curve_sk, get_sk()) != 0) return -1;

    int ret = crypto_box_seal_open(room_key, sealed, sizeof(sealed),
                                   curve_sk, get_pk());
    sodium_memzero(curve_sk, sizeof(curve_sk));
    return ret == 0 ? 0 : -1;
}

int crypto_generate_room_key(unsigned char* room_key) {
    randombytes_buf(room_key, ROOM_KEY_LEN);
    return 0;
}

int crypto_verify_and_trust(const char* node_id, const char* public_key_b64) {
    return storage_add_trusted_key(node_id, public_key_b64);
}

int crypto_is_trusted(const char* node_id) {
    return storage_is_key_trusted(node_id);
}

int crypto_seal_for_storage(const unsigned char* plaintext, size_t ptlen,
                              unsigned char* sealed, size_t* sealed_len,
                              char* b64_out, size_t b64_size) {
    unsigned char curve_pk[crypto_box_PUBLICKEYBYTES];
    if (crypto_sign_ed25519_pk_to_curve25519(curve_pk, get_pk()) != 0) return -1;
    if (crypto_box_seal(sealed, plaintext, ptlen, curve_pk) != 0) return -1;
    *sealed_len = ptlen + crypto_box_SEALBYTES;
    crypto_b64_encode(sealed, *sealed_len, b64_out, b64_size);
    return 0;
}

int crypto_unseal_from_storage(const char* b64,
                                 unsigned char* plaintext, size_t pt_size,
                                 unsigned char* sealed_buf, size_t sealed_size) {
    if (crypto_b64_decode(b64, sealed_buf, sealed_size) != 0) return -1;
    unsigned char curve_sk[crypto_box_SECRETKEYBYTES];
    if (crypto_sign_ed25519_sk_to_curve25519(curve_sk, get_sk()) != 0) return -1;
    unsigned long long mlen = pt_size;
    int ret = crypto_box_seal_open(plaintext, sealed_buf, sealed_size,
                                   curve_sk, get_pk());
    sodium_memzero(curve_sk, sizeof(curve_sk));
    return ret == 0 ? (int)mlen : -1;
}

#else

int crypto_init(void) {
    memset(&g_identity, 0, sizeof(g_identity));
    return 0;
}
void crypto_cleanup(void) { memset(&g_identity, 0, sizeof(g_identity)); }
int crypto_generate_identity(void) {
    memset(&g_identity, 0, sizeof(g_identity));
    g_identity.loaded = true;
    return 0;
}
int crypto_load_identity(void) {
    memset(&g_identity, 0, sizeof(g_identity));
    g_identity.loaded = true;
    return 0;
}
int crypto_save_identity(void) { return 0; }
int crypto_encrypt_dm(const char* pt, size_t ptlen, const unsigned char* rpk,
                       char* ct, size_t ct_sz, char* nc, size_t nc_sz,
                       char* ek, size_t ek_sz) { (void)pt; (void)ptlen; (void)rpk; (void)ct; (void)ct_sz; (void)nc; (void)nc_sz; (void)ek; (void)ek_sz; return -1; }
int crypto_decrypt_dm(const char* ct, const char* nc, const char* ek,
                       char* pt, size_t pt_sz) { (void)ct; (void)nc; (void)ek; (void)pt; (void)pt_sz; return -1; }
int crypto_encrypt_room_msg(const char* pt, size_t ptlen, const unsigned char* rk,
                             char* ct, size_t ct_sz, char* nc, size_t nc_sz) { (void)pt; (void)ptlen; (void)rk; (void)ct; (void)ct_sz; (void)nc; (void)nc_sz; return -1; }
int crypto_decrypt_room_msg(const char* ct, const char* nc, const unsigned char* rk,
                             char* pt, size_t pt_sz) { (void)ct; (void)nc; (void)rk; (void)pt; (void)pt_sz; return -1; }
int crypto_seal_room_key(const unsigned char* rk, const unsigned char* rpk,
                          char* out, size_t out_sz) { (void)rk; (void)rpk; (void)out; (void)out_sz; return -1; }
int crypto_unseal_room_key(const char* sealed, unsigned char* rk) { (void)sealed; (void)rk; return -1; }
int crypto_generate_room_key(unsigned char* rk) { (void)rk; return -1; }
int crypto_verify_and_trust(const char* nid, const char* pk_b64) { (void)nid; (void)pk_b64; return -1; }
int crypto_is_trusted(const char* nid) { (void)nid; return 0; }
int crypto_seal_for_storage(const unsigned char* pt, size_t ptlen, unsigned char* sealed, size_t* slen, char* b64, size_t b64_sz) { (void)pt; (void)ptlen; (void)sealed; (void)slen; (void)b64; (void)b64_sz; return -1; }
int crypto_unseal_from_storage(const char* b64, unsigned char* pt, size_t pt_sz, unsigned char* sbuf, size_t sbuf_sz) { (void)b64; (void)pt; (void)pt_sz; (void)sbuf; (void)sbuf_sz; return -1; }

#endif

void crypto_b64_encode(const unsigned char* bin, size_t binlen,
                       char* b64, size_t b64_size) {
#ifdef HAVE_SODIUM
    sodium_bin2base64(b64, b64_size, bin, binlen,
                      sodium_base64_VARIANT_ORIGINAL);
#else
    (void)bin; (void)binlen;
    if (b64_size > 0) b64[0] = '\0';
#endif
}

int crypto_b64_decode(const char* b64, unsigned char* bin, size_t bin_size) {
#ifdef HAVE_SODIUM
    size_t outlen = 0;
    return sodium_base642bin(bin, bin_size, b64, strlen(b64),
                             NULL, &outlen, NULL,
                             sodium_base64_VARIANT_ORIGINAL) == 0 ? 0 : -1;
#else
    (void)b64; (void)bin; (void)bin_size;
    return -1;
#endif
}
