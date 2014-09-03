#ifndef _DEBUGLOGSINGLETON_H_
#define _DEBUGLOGSINGLETON_H_

#include <fstream>


class DebugLogSingleton {
private:
    DebugLogSingleton();
    ~DebugLogSingleton();
public:
    static void Initialize(const std::string & filename, const std::string & labels);
    static void Deinitialize();
    static DebugLogSingleton * Instance();
    
    void SetDebugVectorS32(const char * name, const char * tags,const int32_t * pdata, uint32_t len,int64_t t1, int64_t t2);
    
    void SetDebugVectorS16(const char * name, const char * tags,const int16_t * pdata, uint32_t len,int64_t t1, int64_t t2);
    void SetDebugVectorU16(const char * name, const char * tags,const uint16_t * pdata, uint32_t len,int64_t t1, int64_t t2);

    void SetDebugVectorU8(const char * name, const char * tags,const uint8_t * pdata, uint32_t len,int64_t t1, int64_t t2);
    void SetDebugVectorS8(const char * name, const char * tags,const int8_t * pdata, uint32_t len,int64_t t1, int64_t t2);

private:
    std::ofstream * _pOutstream;
    std::string _source;
    std::string _tags;
    
};

#endif