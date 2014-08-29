#ifndef _DEBUGLOGSINGLETON_H_
#define _DEBUGLOGSINGLETON_H_

#include <fstream>

class DebugLogSingleton {
private:
    DebugLogSingleton();
    ~DebugLogSingleton();
public:
    static void Initialize(const std::string & filename);
    static void Deinitialize();
    static DebugLogSingleton * Instance();
    
    void SetDebugVectorS16(const char * name, const int16_t * pdata, uint32_t len);
    void SetDebugVectorU8(const char * name, const uint8_t * pdata, uint32_t len);
    void SetDebugVectorS8(const char * name, const int8_t * pdata, uint32_t len);

private:
    std::ofstream * _pOutstream;
    
};

#endif