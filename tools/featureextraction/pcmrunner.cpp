#include <iostream>
#include <fstream>
#include "../../kitsune/audiofeatures.h"
#include "../../kitsune/fft.h"
#include "../../kitsune/matmessageutils.h"
#include "../../kitsune/nanopb/pb_encode.h"
using namespace std;

#define FFT_SIZE_2N (10)
#define FFT_SIZE (1 << FFT_SIZE_2N)
#define SAMPLE_RATE (44100)


int main(int argc, char * argv[]) {
    if (argc <= 2) {
        cerr << "Takes 2 inputs, 1) input file and 2) output file" << endl;
        cerr << "It is assumed that the input is mono, 16 bits per sample, in PCM format (i.e as raw as you can get)" << endl;
    }
    
    uint8_t buf2[16384];

    pb_ostream_t output = pb_ostream_from_buffer(buf2, 16384);
    
    ifstream inFile (argv[1], ios::in | ios::binary);
    ofstream outFile(argv[2], ios::out | ios::binary);
    char buf[2 * FFT_SIZE ];
    const short * samples = (const short *) buf;
    uint8_t isStable;
  
    int16_t logmfcc[MEL_SCALE_ROUNDED_UP];
    
    AudioFeatures_Init();

    
    //pb_encode()
    
    do {
        //read
        inFile.read(buf,sizeof(buf));

        if (AudioFeatures_Extract(logmfcc,&isStable,samples, AUDIO_FFT_SIZE)) {
            
            SetInt16Matrix(&output,logmfcc,1,MEL_SCALE_ROUNDED_UP,0);
            
               outFile.write((const char *)logmfcc,sizeof(logmfcc));
           
        }
        
        
    } while (inFile);

    return 0;
}
