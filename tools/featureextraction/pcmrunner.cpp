#include <iostream>
#include <fstream>
#include "../../kitsune/audiofeatures.h"
#include "../../kitsune/fft.h"

using namespace std;

#define FFT_SIZE_2N (10)
#define FFT_SIZE (1 << FFT_SIZE_2N)
#define SAMPLE_RATE (44100)

int main(int argc, char * argv[]) {
    if (argc <= 2) {
        cerr << "Takes 2 inputs, 1) input file and 2) output file" << endl;
        cerr << "It is assumed that the input is mono, 16 bits per sample, in PCM format (i.e as raw as you can get)" << endl;
    }
    
    ifstream inFile (argv[1], ios::in | ios::binary);
    ofstream outFile(argv[2], ios::out | ios::binary);
    char buf[2 * FFT_SIZE ];
    const short * samples = (const short *) buf;
    
  
    int16_t logmfcc[MEL_SCALE_ROUNDED_UP];
    
    AudioFeatures_Init();

    do {
        //read
        inFile.read(buf,sizeof(buf));

        if (AudioFeatures_Extract(logmfcc, samples, AUDIO_FFT_SIZE)) {
            //output to file
            outFile.write((const char *)logmfcc,sizeof(logmfcc));
        }
        
        
    } while (inFile);

    return 0;
}
