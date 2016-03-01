#include <iostream>
#include <fstream>
#include "../../kitsune/audiofeatures.h"
#include "../../kitsune/audioclassifier.h"
#include "../../kitsune/protobuf/matrix.pb.h"
#include "../../kitsune/debugutils/debugsingleton.h"
#include <string.h>
#include <sstream>
#include "pb_encode.h"
#include "../../kitsune/debugutils/base64.h"
#include <sndfile.hh>

using namespace std;

#define BUF_SIZE (1 << 24)

static uint8_t _buf[BUF_SIZE];
static uint8_t _outbuf[4*BUF_SIZE];


static void read_file (const std::string & fname) {
   

    SndfileHandle file ;
    
    file = SndfileHandle (fname) ;
    
    printf ("Opened file '%s'\n", fname.c_str()) ;
    printf ("    Sample rate : %d\n", file.samplerate ()) ;
    printf ("    Channels    : %d\n", file.channels ()) ;
    
    
    
    int16_t buf[AUDIO_FFT_SIZE * file.channels()];
    int16_t monobuf[AUDIO_FFT_SIZE];
    while (true) {
    int count = file.read(buf, AUDIO_FFT_SIZE * file.channels());
        if (count <= 0) {
            break;
        }
        
        memset(monobuf,0,sizeof(monobuf));
        
        for (int i = 0; i < AUDIO_FFT_SIZE; i ++) {
            monobuf[i] = buf[file.channels() * i];
        }
        
        AudioFeatures_SetAudioData(monobuf, AUDIO_FFT_SIZE);
    }
}





int main(int argc, char * argv[]) {
    const std::string inFile = argv[1];
    
    memset(_buf,0,sizeof(_buf));
    memset(_outbuf,0,sizeof(_outbuf));

    AudioClassifier_Init(NULL);
    AudioClassifier_SetStorageBuffers(_buf, sizeof(_buf));
    AudioFeatures_Init(AudioClassifier_DataCallback,NULL);
    
    read_file(inFile);

    const MatrixClientMessage * p = (MatrixClientMessage *)getMatrixClientMessage();
    
    pb_ostream_t pbout = pb_ostream_from_buffer(_outbuf, sizeof(_outbuf));
    
    pb_encode(&pbout, MatrixClientMessage_fields, p);
    
    const std::string filename = inFile + ".proto";
    ofstream outprotofile(filename,std::ios::binary);

    if (outprotofile.is_open()) {
        std::cout << "wrote " << pbout.bytes_written << " bytes to " << filename << std::endl;
        outprotofile.write((const char *)_outbuf,pbout.bytes_written);
    }
    
    return 0;
}
