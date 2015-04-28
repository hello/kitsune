#include "hash_functions.h"
#include "string.h"

unsigned int hash( const char *key, unsigned int len, unsigned int seed ) {
	#define rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))

	#define mix(a,b,c) \
	{ \
	  a -= c;  a ^= rot(c, 4);  c += b; \
	  b -= a;  b ^= rot(a, 6);  a += c; \
	  c -= b;  c ^= rot(b, 8);  b += a; \
	  a -= c;  a ^= rot(c,16);  c += b; \
	  b -= a;  b ^= rot(a,19);  a += c; \
	  c -= b;  c ^= rot(b, 4);  b += a; \
	}

	#define final(a,b,c) \
	{ \
	  c ^= b; c -= rot(b,14); \
	  a ^= c; a -= rot(c,11); \
	  b ^= a; b -= rot(a,25); \
	  c ^= b; c -= rot(b,16); \
	  a ^= c; a -= rot(c,4);  \
	  b ^= a; b -= rot(a,14); \
	  c ^= b; c -= rot(b,24); \
	}

	unsigned int a, b, c;
	unsigned int *k = (unsigned int *)key;

	a = b = c = 0xdeadbeef + len + seed;

	while ( len > 12 ) {
		a += k[0];
		b += k[1];
		c += k[2];
		mix(a,b,c);
		len -= 12;
		k += 3;
	}

	switch( len ) {
		case 12: c+=k[2]; b+=k[1]; a+=k[0]; break;
		case 11: c+=k[2]&0xffffff; b+=k[1]; a+=k[0]; break;
		case 10: c+=k[2]&0xffff; b+=k[1]; a+=k[0]; break;
		case 9 : c+=k[2]&0xff; b+=k[1]; a+=k[0]; break;
		case 8 : b+=k[1]; a+=k[0]; break;
		case 7 : b+=k[1]&0xffffff; a+=k[0]; break;
		case 6 : b+=k[1]&0xffff; a+=k[0]; break;
		case 5 : b+=k[1]&0xff; a+=k[0]; break;
		case 4 : a+=k[0]; break;
		case 3 : a+=k[0]&0xffffff; break;
		case 2 : a+=k[0]&0xffff; break;
		case 1 : a+=k[0]&0xff; break;
		case 0 : return c;
	}

	final(a,b,c);
	return c;
}

boolean int_eq_func(void* a, void* b) {
	return memcmp(a, b, sizeof(int)) == 0;
}
unsigned int int_hash_func(void* a) {
	//return *(int*)a; //for very random data...
	return hash((char*) a, sizeof(int), 0xbeefcafe);
}
boolean str_eq_func(void* a, void* b) {
	int len_a = strlen(a);
	int len_b = strlen(b);
	if( len_a == len_b ) {
		return memcmp(a, b, len_a) == 0;
	}
	return FALSE;
}
unsigned int str_hash_func(void* a) {
	//return *(int*)a; //for very random data...
	return hash((char*) a, strlen(a), 0xbeefcafe);
}
