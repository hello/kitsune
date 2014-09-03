#include "debuglog.h"
#include "DebugLogSingleton.h"
#include "../nanopb/pb_encode.h"
#include "base64.h"
#include "matmessageutils.h"

#define MAT_BUF_SIZE (100000*2)

void SetDebugVectorS32(const char * name, const char * tags,const int32_t * pdata, uint32_t len,int64_t t1, int64_t t2) {
    if (DebugLogSingleton::Instance()) {
        DebugLogSingleton::Instance()->SetDebugVectorS32(name,tags,pdata,len,t1,t2);
    }
}

void SetDebugVectorS16(const char * name, const char * tags,const int16_t * pdata, uint32_t len,int64_t t1, int64_t t2) {
    if (DebugLogSingleton::Instance()) {
        DebugLogSingleton::Instance()->SetDebugVectorS16(name,tags,pdata,len,t1,t2);
    }
}

void SetDebugVectorU8(const char * name, const char * tags,const uint8_t * pdata, uint32_t len,int64_t t1, int64_t t2) {
    if (DebugLogSingleton::Instance()) {
        DebugLogSingleton::Instance()->SetDebugVectorU8(name,tags,pdata,len,t1,t2);
    }
}

void SetDebugVectorS8(const char * name, const char * tags,const int8_t * pdata, uint32_t len,int64_t t1, int64_t t2) {
    if (DebugLogSingleton::Instance()) {
        DebugLogSingleton::Instance()->SetDebugVectorS8(name,tags,pdata,len,t1,t2);
    }
}

static DebugLogSingleton * _pSingletonData;

DebugLogSingleton::DebugLogSingleton() {
    
}

DebugLogSingleton::~DebugLogSingleton() {
    delete _pSingletonData->_pOutstream;
}

//static
void DebugLogSingleton::Initialize(const std::string & filename, const std::string & labels) {
    _pSingletonData = new DebugLogSingleton();
    
    _pSingletonData->_pOutstream = new std::ofstream(filename,std::ios::out | std::ios::binary);
    _pSingletonData->_source = filename;
    _pSingletonData->_tags = labels;
}

//static
void DebugLogSingleton::Deinitialize() {
    delete _pSingletonData;
    _pSingletonData = NULL;
}

//static
DebugLogSingleton * DebugLogSingleton::Instance() {
    return  _pSingletonData;
}

void DebugLogSingleton::SetDebugVectorS32(const char * name, const char * tags,const int32_t * pdata, uint32_t len,int64_t t1, int64_t t2) {
    
    unsigned char buf[MAT_BUF_SIZE];
    pb_ostream_t output;
    size_t encodelength;
    std::string mytags = _tags + "," + std::string(tags);
    
    memset(buf,0,sizeof(buf));
    
    output = pb_ostream_from_buffer(buf, MAT_BUF_SIZE);
    IntArray_t arr;
    
    arr.data.sint32 = pdata;
    arr.type = esint32;
    

    encodelength = SetIntMatrix(&output,name,mytags.c_str(),_source.c_str(),arr,1,len,t1,t2);
    
    *_pOutstream << base64_encode(buf,encodelength) <<std::endl;
}

void DebugLogSingleton::SetDebugVectorS16(const char * name, const char * tags,const int16_t * pdata, uint32_t len,int64_t t1, int64_t t2) {
    
    unsigned char buf[MAT_BUF_SIZE];
    pb_ostream_t output;
    size_t encodelength;
    std::string mytags = _tags + "," + std::string(tags);

    
    memset(buf,0,sizeof(buf));
    
    output = pb_ostream_from_buffer(buf, MAT_BUF_SIZE);
    IntArray_t arr;
    
    arr.data.sint16 = pdata;
    arr.type = esint16;
    
    encodelength = SetIntMatrix(&output,name,mytags.c_str(),_source.c_str(),arr,1,len,t1,t2);
    
    *_pOutstream << base64_encode(buf,encodelength) <<std::endl;
}

void DebugLogSingleton::SetDebugVectorU16(const char * name, const char * tags,const uint16_t * pdata, uint32_t len,int64_t t1, int64_t t2) {
    unsigned char buf[MAT_BUF_SIZE];
    pb_ostream_t output;
    size_t encodelength;
    std::string mytags = _tags + "," + std::string(tags);

    
    memset(buf,0,sizeof(buf));
    
    output = pb_ostream_from_buffer(buf, MAT_BUF_SIZE);
    
    IntArray_t arr;
    
    arr.data.uint16 = pdata;
    arr.type = euint16;

    encodelength = SetIntMatrix(&output,name,mytags.c_str(),_source.c_str(),arr,1,len,t1,t2);
    
    *_pOutstream << base64_encode(buf,encodelength) <<std::endl;
}


void DebugLogSingleton::SetDebugVectorU8(const char * name, const char * tags,const uint8_t * pdata, uint32_t len,int64_t t1, int64_t t2) {
    
    unsigned char buf[MAT_BUF_SIZE];
    pb_ostream_t output;
    size_t encodelength;
    std::string mytags = _tags + "," + std::string(tags);

    
    memset(buf,0,sizeof(buf));
    
    output = pb_ostream_from_buffer(buf, MAT_BUF_SIZE);
    
    IntArray_t arr;
    
    arr.data.uint8 = pdata;
    arr.type = euint8;
    
    encodelength = SetIntMatrix(&output,name,mytags.c_str(),_source.c_str(),arr,1,len,t1,t2);
    
    *_pOutstream << base64_encode(buf,encodelength) <<std::endl;
}

void DebugLogSingleton::SetDebugVectorS8(const char * name, const char * tags,const int8_t * pdata, uint32_t len,int64_t t1, int64_t t2) {
    
    unsigned char buf[MAT_BUF_SIZE];
    pb_ostream_t output;
    size_t encodelength;
    std::string mytags = _tags + "," + std::string(tags);

    
    memset(buf,0,sizeof(buf));
    
    output = pb_ostream_from_buffer(buf, MAT_BUF_SIZE);
    
    IntArray_t arr;
    
    arr.data.sint8 = pdata;
    arr.type = esint8;
    
    encodelength = SetIntMatrix(&output,name,mytags.c_str(),_source.c_str(),arr,1,len,t1,t2);
    
    *_pOutstream << base64_encode(buf,encodelength) <<std::endl;
}
