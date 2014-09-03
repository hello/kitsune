#include <iostream>
#include <fstream>

#include "../../kitsune/audiofeatures.h"
#include "../../kitsune/debugutils/DebugLogSingleton.h"
#include <string.h>
using namespace std;

#define FFT_SIZE_2N (10)
#define FFT_SIZE (1 << FFT_SIZE_2N)
#define SAMPLE_RATE (44100)

static void SegmentCallback(const int32_t * mfcc, const Segment_t * pSegment) {
    cout << pSegment->startOfSegment << "," << pSegment->endOfSegment;
    for (int j =0 ; j < 8; j++) {
        cout << "," << mfcc[j];
    }
    cout << endl;

}

int main(int argc, char * argv[]) {
    if (argc <= 2) {
        cerr << "Takes 2 inputs, 1) input file and 2) output file" << endl;
        cerr << "It is assumed that the input is mono, 16 bits per sample, in PCM format (i.e as raw as you can get)" << endl;
    }
    
    std::string label;
    label = '\0';
    
    if (argc >= 3) {
        label = argv[2];
    }
    
    
    
    ifstream inFile (argv[1], ios::in | ios::binary);
    
    //init output logging... tell it what its destination filename is
    DebugLogSingleton::Initialize(argv[2],label);

    
    char buf[2 * FFT_SIZE ];
    const short * samples = (const short *) buf;
    int64_t counter = 0;
    
    
    AudioFeatures_Init(SegmentCallback);

    
    
    do {
        //read
        inFile.read(buf,sizeof(buf));
        
        AudioFeatures_SetAudioData(samples, AUDIO_FFT_SIZE,counter++);
        
        
    } while (inFile);

    return 0;
}
