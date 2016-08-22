/*
   Examples given are for creating the audio samples for sending out ultrasonic sonar pulses
   or doing system identification (sounds like white noise).  

   If you want to decorrelate / convolve PN sequences directly, the basic idea is this:

   1) The device receives a sequence of real valued measurements (x1,x2,x3... xN).  For example,
      audio samples, which is type int16_t.
   2) Turn received samples into bits (xi > 0 ? 1 : 0), and stuff into an array of bits (uint8_t array, each element holds 8 measurements)
   3) Take reference PN as bit array, use "^" operator to XOR element by element, take result of XOR and do lookup. 
      gist of this is in function "pn_correlate_with_xor"

   


then you should look at the function "pn_correlate_with_xor"


   


 */


//for ultrasonic beaconing, sonar, or whatever

/*****

int main(int argc, char * argv[]) {
    
    uint32_t i;
    uint32_t num_samples_written;

    pn_init_with_mask_9();
    const uint32_t len = pn_get_length();
    
    //malloc
    uint32_t sample_buf_size = len * 24;
    uint8_t bits[len];
    int16_t samplebuf[sample_buf_size];

    
    for (i = 0; i < len; i++) {
        bits[i] = pn_get_next_bit();
    }
    
    num_samples_written = upconvert_bits_bpsk(samplebuf, bits, len * NUM_PN_PERIODS, sample_buf_size);
    
    
    create_file("bpsk.wav",samplebuf,num_samples_written);
    
    
    return 0;
}

*******/


//for system identificaiton

/************

int main(int argc, char * argv[]) {
    
    uint32_t i;
    
    pn_init_with_mask_16();

    const uint32_t len = pn_get_length();
    
    //malloc
    uint8_t bits[NUM_PN_PERIODS * len];
    int16_t samplebuf[NUM_PN_PERIODS * len];
    
    
    for (i = 0; i < NUM_PN_PERIODS * len; i++) {
        bits[i] = pn_get_next_bit();
    
        if (bits[i] > 0) {
            samplebuf[i] = 32767;
        }
        else {
            samplebuf[i] = -32767;
        }
    
    }
    
    
    create_file("reference.wav",samplebuf,len);
    create_file("white.wav",samplebuf,NUM_PN_PERIODS * len);
    
    
    return 0;
}

***********/
