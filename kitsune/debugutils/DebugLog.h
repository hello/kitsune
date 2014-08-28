#ifndef _DEBUGLOG_H_
#define _DEBUGLOG_H_

#include <stdint.h>

#ifdef USE_CPP_DEBUG_LOGGING


#ifdef __cplusplus
extern "C" {
    void SetDebugVectorS16(const char * name, const int16_t * pdata, uint32_t len);
    void SetDebugVectorU8(const char * name, const uint8_t * pdata, uint32_t len);
    void SetDebugVectorS8(const char * name, const int8_t * pdata, uint32_t len);

}
#else
    void SetDebugVectorS16(const char * name, const int16_t * pdata, uint32_t len);
    void SetDebugVectorU8(const char * name, const uint8_t * pdata, uint32_t len);
    void SetDebugVectorS8(const char * name, const int8_t * pdata, uint32_t len);
#endif

#define DEBUG_LOG_S16(name,ptr,len)  (SetDebugVectorS16( (name) , (ptr) , (len) ))
#define DEBUG_LOG_U8(name,ptr,len)  (SetDebugVectorU8( (name) , (ptr) , (len) ))
#define DEBUG_LOG_S8(name,ptr,len)  (SetDebugVectorS8( (name) , (ptr) , (len) ))

#endif



/* do nothing otherwise */
#ifndef DEBUG_LOG_S16
#define DEBUG_LOG_S16(name,ptr,len) (void)
#endif

#ifndef DEBUG_LOG_U8
#define DEBUG_LOG_U8(name,ptr,len) (void)
#endif

#ifndef DEBUG_LOG_S8
#define DEBUG_LOG_S8(name,ptr,len) (void)
#endif

#endif
