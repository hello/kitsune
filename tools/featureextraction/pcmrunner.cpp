#include <iostream>
#include <fstream>
#include "../../kitsune/audiofeatures.h"
#include "../../kitsune/audioclassifier.h"
#include "../../kitsune/debugutils/DebugLogSingleton.h"
#include <string.h>
#include "pb_encode.h"
#include "../../kitsune/debugutils/base64.h"
using namespace std;

#define FFT_SIZE_2N (10)
#define FFT_SIZE (1 << FFT_SIZE_2N)
#define SAMPLE_RATE (44100)

static std::string _label;

static void NoveltyNotifcation(void) {
    
}

static void SegmentCallback(const int16_t * feats, const Segment_t * pSegment) {
    
    cout << pSegment->t1 << "," << pSegment->duration;
#if 1
    for (int j =0 ; j < NUM_AUDIO_FEATURES; j++) {
        cout << "," << feats[j];
    }
#endif
    cout << endl;
#if 1
    std::string tags;
    tags = "coherent";
    
    DebugLogSingleton::Instance()->SetDebugVectorS16("featAudio", tags.c_str(), feats, NUM_AUDIO_FEATURES, pSegment->t1, pSegment->t2);
#endif
}

#define OUT_BUF_SIZE (100000)
static void serialize_buf() {
    unsigned char buf[OUT_BUF_SIZE];
    pb_ostream_t output;
    size_t encodelength;
    std::string mytags = "";
    
    memset(buf,0,sizeof(buf));
    
    output = pb_ostream_from_buffer(buf, OUT_BUF_SIZE);
    
    encodelength = AudioClassifier_EncodeAudioFeatures(&output, NULL);
    std::cout << "length is " << encodelength << std::endl;
    std::ofstream file("foo.out");
    if (file.is_open()) {
        file << base64_encode(buf,encodelength) <<std::endl;
    }
}



int main(int argc, char * argv[]) {
    if (argc <= 2) {
        cerr << "Takes 2 inputs, 1) input file and 2) output file" << endl;
        cerr << "It is assumed that the input is mono, 16 bits per sample, in PCM format (i.e as raw as you can get)" << endl;
    }
    
    _label.clear();
    _label = "none";
    
    if (argc >= 4) {
        _label = argv[3];
    }
    
    
    
    ifstream inFile (argv[1], ios::in | ios::binary);
    std::cout << "Processing " << argv[1] << std::endl;
    //init output logging... tell it what its destination filename is
    DebugLogSingleton::Initialize_UsingFileStream(argv[2],_label);

    
    char buf[2 * FFT_SIZE ];
    const short * samples = (const short *) buf;
    int64_t counter = 0;
    
    
    AudioFeatures_Init(AudioClassifier_DataCallback);
    AudioClassifier_Init(NULL,NULL,NULL);

    
    
    do {
        //read
        inFile.read(buf,sizeof(buf));
        
        AudioFeatures_SetAudioData(samples,counter++);
        
        
    } while (inFile);
    
    serialize_buf();

    return 0;
}
