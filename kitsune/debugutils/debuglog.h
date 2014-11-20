#ifndef _DEBUGLOG_H_
#define _DEBUGLOG_H_

#include <stdint.h>

#ifdef USE_CPP_DEBUG_LOGGING


#ifdef __cplusplus
extern "C" {
#endif

    void SetDebugVectorS32(const char * name, const char * tags,const int32_t * pdata, uint32_t len,int64_t t1, int64_t t2);
    void SetDebugVectorS16(const char * name, const char * tags,const int16_t * pdata, uint32_t len,int64_t t1, int64_t t2);
    void SetDebugVectorU16(const char * name, const char * tags,const uint16_t * pdata, uint32_t len,int64_t t1, int64_t t2);
    void SetDebugVectorU8(const char * name, const char * tags,const uint8_t * pdata, uint32_t len,int64_t t1, int64_t t2);
    void SetDebugVectorS8(const char * name, const char * tags,const int8_t * pdata, uint32_t len,int64_t t1, int64_t t2);
    
    void DebugLog_Initialize(const char * filename);
    void DebugLog_Deinitialize();
    const char * DebugLog_DumpStringBuf();

#ifdef __cplusplus
}
#endif


#define DEBUG_LOG_S32(name,tags,ptr,len,t1,t2)  (SetDebugVectorS32( (name) , (tags), (ptr) , (len), (t1), (t2) ) )
#define DEBUG_LOG_S16(name,tags,ptr,len,t1,t2)  (SetDebugVectorS16( (name) , (tags), (ptr) , (len), (t1), (t2) ) )
#define DEBUG_LOG_U16(name,tags,ptr,len,t1,t2)  (SetDebugVectorU16( (name) , (tags), (ptr) , (len), (t1), (t2) ) )
#define DEBUG_LOG_U8(name,tags,ptr,len,t1,t2)  (SetDebugVectorU8( (name) , (tags), (ptr) , (len), (t1), (t2) ) )
#define DEBUG_LOG_S8(name,tags,ptr,len,t1,t2)  (SetDebugVectorS8( (name) , (tags), (ptr) , (len), (t1), (t2) ) )

#endif



/* do nothing otherwise */
#ifndef DEBUG_LOG_S32
#define DEBUG_LOG_S32(name,tags,ptr,len,t1,t2)
#endif

#ifndef DEBUG_LOG_S16
#define DEBUG_LOG_S16(name,tags,ptr,len,t1,t2)
#endif

#ifndef DEBUG_LOG_U16
#define DEBUG_LOG_U16(name,tags,ptr,len,t1,t2)
#endif

#ifndef DEBUG_LOG_U8
#define DEBUG_LOG_U8(name,tags,ptr,len,t1,t2)
#endif

#ifndef DEBUG_LOG_S8
#define DEBUG_LOG_S8(name,tags,ptr,len,t1,t2)
#endif

#endif
