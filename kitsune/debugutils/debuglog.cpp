#include "debuglog.h"
#include "debugsingleton.h"
#include "pb_encode.h"
#include "base64.h"
#include "matmessageutils.h"
#include <sstream>
#include <assert.h>

#define MAT_BUF_SIZE (100000*2)

static DebugLogSingleton * _pSingletonData;


void DebugLog_Initialize(const char * filename) {
    if (filename) {
        DebugLogSingleton::Initialize_UsingFileStream(filename);
    }
    else {
        DebugLogSingleton::Initialize_UsingStringStream();
    }
}

void DebugLog_Deinitialize() {
    
}

const char * DebugLog_DumpStringBuf() {
    return DebugLogSingleton::DumpStringBuffer().c_str();
}


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

void SetDebugVectorU16(const char * name, const char * tags,const uint16_t * pdata, uint32_t len,int64_t t1, int64_t t2) {
    if (DebugLogSingleton::Instance()) {
        DebugLogSingleton::Instance()->SetDebugVectorU16(name,tags,pdata,len,t1,t2);
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



static void SetDebugMatrix(std::ostream & outstream, const std::string & source, const std::string & defaulttags,const char * name, const char * tags,const_IntArray_t arr, uint32_t len,int64_t t1, int64_t t2) {
    unsigned char buf[MAT_BUF_SIZE];
    pb_ostream_t output;
    size_t encodelength;
    std::string mytags = defaulttags;
    
    if (tags) {
        mytags += "," + std::string(tags);
    }
    
    memset(buf,0,sizeof(buf));
    
    output = pb_ostream_from_buffer(buf, MAT_BUF_SIZE);
    
    encodelength = SetIntMatrix(&output,name,mytags.c_str(),source.c_str(),arr,1,len,t1,t2);
    
    outstream << base64_encode(buf,encodelength) <<std::endl;
}




DebugLogSingleton::DebugLogSingleton()
: _isStringBuffer(false) {
    
}

DebugLogSingleton::~DebugLogSingleton() {
    delete _pSingletonData->_pOutstream;
}

//static
void DebugLogSingleton::Initialize_UsingFileStream(const std::string & filename, const std::string & labels) {
    _pSingletonData = new DebugLogSingleton();
    
    _pSingletonData->_pOutstream = new std::ofstream(filename,std::ios::out | std::ios::binary);
    _pSingletonData->_source = filename;
    _pSingletonData->_tags = labels;
}

//static
void DebugLogSingleton::Initialize_UsingStringStream(const std::string & source, const std::string & labels) {
    _pSingletonData = new DebugLogSingleton();
    
    _pSingletonData->_pOutstream = new std::ostringstream();
    _pSingletonData->_source = source;
    _pSingletonData->_tags = labels;
    _pSingletonData->_isStringBuffer = true;
}

const std::string & DebugLogSingleton::DumpStringBuffer() {
    assert(_pSingletonData);
    
    if (_pSingletonData->_isStringBuffer) {
        std::ostringstream * p = static_cast<std::ostringstream *>(_pSingletonData->_pOutstream);
    
        if (p) {
            _pSingletonData->_strbuf = p->str();
        
            p->str("");
            p->clear();
        }
    }
    
    return _pSingletonData->_strbuf;
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
    
    const_IntArray_t arr;
    
    arr.data.sint32 = pdata;
    arr.type = esint32;
    
    SetDebugMatrix(*_pOutstream,_source,_tags,name,tags,arr,len,t1,t2);
}

void DebugLogSingleton::SetDebugVectorS16(const char * name, const char * tags,const int16_t * pdata, uint32_t len,int64_t t1, int64_t t2) {
    
    const_IntArray_t arr;
    
    arr.data.sint16 = pdata;
    arr.type = esint16;
    
    SetDebugMatrix(*_pOutstream,_source,_tags,name,tags,arr,len,t1,t2);
}

void DebugLogSingleton::SetDebugVectorU16(const char * name, const char * tags,const uint16_t * pdata, uint32_t len,int64_t t1, int64_t t2) {
   
    const_IntArray_t arr;
    
    arr.data.uint16 = pdata;
    arr.type = euint16;

    SetDebugMatrix(*_pOutstream,_source,_tags,name,tags,arr,len,t1,t2);

}


void DebugLogSingleton::SetDebugVectorU8(const char * name, const char * tags,const uint8_t * pdata, uint32_t len,int64_t t1, int64_t t2) {
    
    
    const_IntArray_t arr;
    
    arr.data.uint8 = pdata;
    arr.type = euint8;

    SetDebugMatrix(*_pOutstream,_source,_tags,name,tags,arr,len,t1,t2);
 
}

void DebugLogSingleton::SetDebugVectorS8(const char * name, const char * tags,const int8_t * pdata, uint32_t len,int64_t t1, int64_t t2) {
    
    const_IntArray_t arr;
    
    arr.data.sint8 = pdata;
    arr.type = esint8;

    SetDebugMatrix(*_pOutstream,_source,_tags,name,tags,arr,len,t1,t2);

}
