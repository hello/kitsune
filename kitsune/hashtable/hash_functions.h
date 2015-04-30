#ifndef HASH_FUNCTIONS_H
#define HASH_FUNCTIONS_H

unsigned int hash( const char *key, unsigned int len, unsigned int seed );

#include "boolean.h"
boolean int_eq_func(void* a,void* b);
unsigned int int_hash_func(void* a);

boolean str_eq_func(void* a, void* b);
unsigned int str_hash_func(void* a);
#endif
