#include "swap_mem.h"

//#define swap( x, y ) x^=y; y^=x; x^=y;
#define swap( x, y, typ ) typ s = x; x = y; y = s;

void swap_mem( unsigned char * a, unsigned char * b, unsigned int sz ) {
  #define swap_while(typ) \
    while( sz >= sizeof(typ) ) { \
      swap( *(typ*)a, *(typ*)b, typ ); \
      a += sizeof(typ); \
      b += sizeof(typ); \
      sz -= sizeof(typ); \
    }    
  if( a != b ) {
    swap_while(int);
    swap_while(char);
  }
}
