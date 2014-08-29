#include "debuglog.h"
#include "DebugLogSingleton.h"
#include "../nanopb/pb_encode.h"
#include "base64.h"
#include "matmessageutils.h"

#define MAT_BUF_SIZE (100000*2)

void SetDebugVectorS32(const char * name, const int32_t * pdata, uint32_t len) {
    if (DebugLogSingleton::Instance()) {
        DebugLogSingleton::Instance()->SetDebugVectorS32(name,pdata,len);
    }
}

void SetDebugVectorS16(const char * name, const int16_t * pdata, uint32_t len) {
    if (DebugLogSingleton::Instance()) {
        DebugLogSingleton::Instance()->SetDebugVectorS16(name,pdata,len);
    }
}

void SetDebugVectorU8(const char * name, const uint8_t * pdata, uint32_t len) {
    if (DebugLogSingleton::Instance()) {
        DebugLogSingleton::Instance()->SetDebugVectorU8(name,pdata,len);
    }
}

void SetDebugVectorS8(const char * name, const int8_t * pdata, uint32_t len) {
    if (DebugLogSingleton::Instance()) {
        DebugLogSingleton::Instance()->SetDebugVectorS8(name,pdata,len);
    }
}

static DebugLogSingleton * _pSingletonData;

DebugLogSingleton::DebugLogSingleton() {
    
}

DebugLogSingleton::~DebugLogSingleton() {
    delete _pSingletonData->_pOutstream;
}

void DebugLogSingleton::Initialize(const std::string & filename) {
    _pSingletonData = new DebugLogSingleton();
    
    _pSingletonData->_pOutstream = new std::ofstream(filename,std::ios::out | std::ios::binary);
}

void DebugLogSingleton::Deinitialize() {
    delete _pSingletonData;
    _pSingletonData = NULL;
}

DebugLogSingleton * DebugLogSingleton::Instance() {
    return  _pSingletonData;
}

void DebugLogSingleton::SetDebugVectorS32(const char * name, const int32_t * pdata, uint32_t len) {
    
    unsigned char buf[MAT_BUF_SIZE];
    pb_ostream_t output;
    size_t encodelength;
    
    memset(buf,0,sizeof(buf));
    
    output = pb_ostream_from_buffer(buf, MAT_BUF_SIZE);
    IntArray_t arr;
    
    arr.data.sint32 = pdata;
    arr.type = esint32;
    
    encodelength = SetIntMatrix(&output,arr,1,len,0,0);
    
    *_pOutstream << std::string(name) << "\t" << base64_encode(buf,encodelength) <<std::endl;
}

void DebugLogSingleton::SetDebugVectorS16(const char * name, const int16_t * pdata, uint32_t len) {
    
    unsigned char buf[MAT_BUF_SIZE];
    pb_ostream_t output;
    size_t encodelength;
    
    memset(buf,0,sizeof(buf));
    
    output = pb_ostream_from_buffer(buf, MAT_BUF_SIZE);
    IntArray_t arr;
    
    arr.data.sint16 = pdata;
    arr.type = esint16;
    
    encodelength = SetIntMatrix(&output,arr,1,len,0,0);
    
    *_pOutstream << std::string(name) << "\t" << base64_encode(buf,encodelength) <<std::endl;
}

void DebugLogSingleton::SetDebugVectorU16(const char * name, const uint16_t * pdata, uint32_t len) {
    unsigned char buf[MAT_BUF_SIZE];
    pb_ostream_t output;
    size_t encodelength;
    
    memset(buf,0,sizeof(buf));
    
    output = pb_ostream_from_buffer(buf, MAT_BUF_SIZE);
    
    IntArray_t arr;
    
    arr.data.uint16 = pdata;
    arr.type = euint16;

    encodelength = SetIntMatrix(&output,arr,1,len,0,0);
    
    *_pOutstream << std::string(name) << "\t" << base64_encode(buf,encodelength) <<std::endl;
}


void DebugLogSingleton::SetDebugVectorU8(const char * name, const uint8_t * pdata, uint32_t len) {
    
    unsigned char buf[MAT_BUF_SIZE];
    pb_ostream_t output;
    size_t encodelength;
    
    memset(buf,0,sizeof(buf));
    
    output = pb_ostream_from_buffer(buf, MAT_BUF_SIZE);
    
    IntArray_t arr;
    
    arr.data.uint8 = pdata;
    arr.type = euint8;
    
    encodelength = SetIntMatrix(&output,arr,1,len,0,0);
    
    *_pOutstream << std::string(name) << "\t" << base64_encode(buf,encodelength) <<std::endl;
}

void DebugLogSingleton::SetDebugVectorS8(const char * name, const int8_t * pdata, uint32_t len) {
    
    unsigned char buf[MAT_BUF_SIZE];
    pb_ostream_t output;
    size_t encodelength;
    
    memset(buf,0,sizeof(buf));
    
    output = pb_ostream_from_buffer(buf, MAT_BUF_SIZE);
    
    IntArray_t arr;
    
    arr.data.sint8 = pdata;
    arr.type = esint8;
    
    encodelength = SetIntMatrix(&output,arr,1,len,0,0);
    
    *_pOutstream << std::string(name) << "\t" << base64_encode(buf,encodelength) <<std::endl;
}
