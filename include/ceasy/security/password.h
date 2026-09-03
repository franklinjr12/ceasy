#ifndef CEASY_SECURITY_PASSWORD_H
#define CEASY_SECURITY_PASSWORD_H

#include <stdbool.h>

#include <ceasy/memory/arena.h>
#include <ceasy/string/string.h>

bool password_hash(Arena *arena, StringView password, String *digest);
bool password_verify(StringView digest, StringView password);

#endif
