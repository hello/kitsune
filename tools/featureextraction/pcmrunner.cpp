#include <iostream>
#include <fstream>
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
    
    short fr[FFT_SIZE];
    short fi[FFT_SIZE];
    short psd[FFT_SIZE >> 1];
    uint8_t mel[MEL_SCALE_SIZE];
    
    do {
        //read
        inFile.read(buf,sizeof(buf));

        memcpy(fr,samples,sizeof(fr));
        memset(fi,0,sizeof(fi));
        
        //get fft
        fft(fr,fi,FFT_SIZE_2N);
        
        //take sum abs of complex and reals on one half of the PSD
        abs_fft(psd,fr,fi, FFT_SIZE_2N);
        
        memset(mel,0,sizeof(mel));
        mel_freq(mel, psd, FFT_SIZE_2N, SAMPLE_RATE / FFT_SIZE);
        
        //output to file
        outFile.write((const char *)mel,sizeof(mel));
        
        
    } while (inFile);

    return 0;
}
