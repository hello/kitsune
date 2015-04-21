#ifndef _SERIALIZATION_MACROS_H
#define _SERIALIZATION_MACROS_H

#define FREE_AND_NULL(a) vPortFree(a); a = NULL;
#define DUMP_ADVANCE(data,ptr) memcpy(ptr, &data, sizeof(data));  ptr+=sizeof(data);
#define LIFT_ADVANCE(data,ptr) memcpy(&data, ptr, sizeof(data));  ptr+=sizeof(data);

#endif
