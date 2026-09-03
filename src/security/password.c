#include <ceasy/security/password.h>

#include <sodium.h>

#include <stdlib.h>
#include <string.h>

#define CEASY_MAX_PASSWORD_BYTES 1024

bool password_hash(Arena *arena, StringView password, String *digest) {
    char hash[crypto_pwhash_STRBYTES];

    if (arena == NULL || digest == NULL || password.data == NULL ||
        password.length == 0 || password.length > CEASY_MAX_PASSWORD_BYTES ||
        sodium_init() < 0 || password.length > SIZE_MAX - 1) {
        return false;
    }
    if (password.length >= sizeof(hash) ||
        crypto_pwhash_str(hash, password.data, password.length,
                          crypto_pwhash_OPSLIMIT_INTERACTIVE,
                          crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
        return false;
    }
    *digest = string_from_in(arena, stringv_from_cstr(hash));
    return digest->data != NULL;
}

bool password_verify(StringView digest, StringView password) {
    char *copy;
    bool result;

    if (digest.data == NULL || password.data == NULL || password.length == 0 ||
        password.length > CEASY_MAX_PASSWORD_BYTES || sodium_init() < 0 ||
        digest.length > SIZE_MAX - 1) {
        return false;
    }
    copy = malloc(digest.length + 1);
    if (copy == NULL) {
        return false;
    }
    memcpy(copy, digest.data, digest.length);
    copy[digest.length] = '\0';
    result =
        crypto_pwhash_str_verify(copy, password.data, password.length) == 0;
    free(copy);
    return result;
}
