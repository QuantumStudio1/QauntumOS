#ifndef QAUNTUM_AUTH_H
#define QAUNTUM_AUTH_H

#include <stddef.h>

int qa_auth_valid_name(const char *name);
int qa_auth_count(const char *directory);
int qa_auth_list(const char *directory, char names[][33], size_t capacity);
int qa_auth_create(const char *directory, const char *name, const char *password);
int qa_auth_verify(const char *directory, const char *name, const char *password);

#endif
