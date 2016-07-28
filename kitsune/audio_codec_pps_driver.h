#include <stdint.h>
#include "codec_debug_config.h"

//INSTRUCTIONS & COEFFICIENTS
typedef struct{
    uint8_t reg_off;
    uint8_t reg_val;
}reg_value;

#define KITSUNE_CODE 1

#define ENABLE_DSP_SYNC_MODE 0

#if (ENABLE_DSP_SYNC_MODE==1)
#define NDAC_POWER_UP_LATER 1
#else
#define NDAC_POWER_UP_LATER 0
#endif

#if (CODEC_ADC_16KHZ==1)
#define PLL_P 1
#define PLL_R 1
#define PLL_J 6
#define PLL_D 9120UL
#define NDAC 3
#define MDAC 27
#define NADC 3
#define MADC 27
#define DOSR 64UL
#define AOSR 64UL
#else
#define PLL_P 1
#define PLL_R 1
#define PLL_J 6
#define PLL_D 9120UL
#define NDAC 3
#define MDAC 9
#define NADC 3
#define MADC 9
#define DOSR 64UL
#define AOSR 64UL
#endif

#if (DOSR != AOSR)
#error "Audio Codec OSR error!"
#endif

/*
 * IMPORTANT: https://e2e.ti.com/support/data_converters/audio_converters/w/design_notes/1098.aic3254-minidsp-d-cycles-and-minidsp-a-cycles
 *
 * IDAC>=MDAC*DOSR and IADC>=MADC*AOSR.
 *
 * Accepted range:
 * miniDSP_A_Cycles: 256 to 6144
 * miniDSP_D_Cycles: 352 to 6144
 *
 */

#define IADC (1024) //((MADC)*(AOSR))
#define IDAC (1024) //((MDAC)*(DOSR))

#define MIC_VOLUME_DB 20
#define MIC_VOLUME_CONTROL ((MIC_VOLUME_DB)*2)
#define MUTE_SPK 1

static const reg_value REG_Section_program[] = {
    {  0,0x0},
    {  0x7F,0x00},
//			# Set AutoINC
    {121,0x01},
//			# reg[0][0][1] = 0x01                        ; reg(0)(1)(0x00 => 0 )     S/W Reset
    {  1,0x01},
//			# reg[0][0][4] = 0x33                        ; ADC_CLKIN = PLL_MCLK, DAC_CLKIN = PLL_MCLK
    {  4,0x33},
//			# reg[0][0][5] = 0x00                        ; PLL_CLKIN = MCLK1
    {  5,0x00},
#if (KITSUNE_CODE==1)
//			# reg[0][0][6] = 0x91                        ; P=1, R=1
    {  6,(1 << 7) | (PLL_P << 4) | (PLL_R << 0)},
//			# reg[0][0][7]  = 0x08             ; P=1, R=1, J=8
    {  7,PLL_J},
//			# reg[0][0][13] = 0x00             ; DOSR = 128 (MSB)
    { 13, (DOSR & 0x0300) >> 8},
//			# reg[0][0][14] = 0x80             ; DOSR = 128 (LSB)
    { 14, (DOSR & 0x00FF) >> 0},
//			# reg[0][0][18] = 0x02             ; NADC Powerdown NADC = 2
    { 18,(NADC << 0)},
//			# reg[0][0][19] = 0x90             ; MADC Powerup MADC = 16
    { 19,  (1 << 7) | (MADC << 0)},
//			# reg[0][0][20] = 0x40             ; AOSR = 64
    { 20, AOSR},
#if (NDAC_POWER_UP_LATER==1)
//			# reg[0][0][11] = 0x02
    { 11, (NDAC << 0)},
#else
	//
	{ 11, (1 << 7) | (NDAC << 0)},
#endif
//			# reg[0][0][8] = 0x00                        ; D=0000 (MSB)
    {  8,(PLL_D & 0xFF00) >> 8},
//			# reg[0][0][9] = 0x00                        ; D=0000 (LSB)
    {  9,(PLL_D & 0xFF) >> 0},
//			# reg[0][0][12] = 0x88                       ; reg(0)(0)(0x0c => 12)     DAC Powerup MDAC = 8
    { 12,  (1 << 7) | (MDAC << 0)},
    {  0x7F,0x78},
	 {  0,0x00},
 #else
	 //			# reg[0][0][6] = 0x91                        ; P=1, R=1
    {  6,0x91},
//			# reg[0][0][7]  = 0x08             ; P=1, R=1, J=8
    {  7,0x08},
//			# reg[0][0][13] = 0x00             ; DOSR = 128 (MSB)
    { 13,0x00},
//			# reg[0][0][14] = 0x80             ; DOSR = 128 (LSB)
    { 14,0x80},
//			# reg[0][0][18] = 0x02             ; NADC Powerdown NADC = 2
    { 18,0x02},
//			# reg[0][0][19] = 0x90             ; MADC Powerup MADC = 16
    { 19,0x90},
//			# reg[0][0][20] = 0x40             ; AOSR = 64
    { 20,0x40},
//			# reg[0][0][11] = 0x82             ; P=1, R=1, J=8; DOSR = 128 (MSB); DOSR = 128 (LSB); NADC Powerdown NADC = 2; NADC Powerup MADC = 8; AOSR = 128; P=1, R=1, J=8; DOSR = 192 (MSB); DOSR = 192 (LSB); NADC Powerdown NADC = 2; NADC Powerup MADC = 12; AOSR = 128; P=1, R=1, J=8; DOSR = 256 (MSB); DOSR = 256 (LSB); NADC Powerdown NADC = 2; NADC Powerup MADC = 16; AOSR = 128; P=1, R=1, J=24; DOSR = 384 (MSB); DOSR = 384 (LSB); NADC Powerdown NADC = 2; NADC Powerup MADC = 24; AOSR = 128; P=1, R=1, J=16; DOSR = 512 (MSB); DOSR = 512 (LSB); NADC Powerdown NADC = 2; NADC Powerup MADC = 32; AOSR = 128; P=1, R=1, J=24; DOSR = 768 (MSB); DOSR = 768 (LSB); NADC Powerdown NADC = 2; NADC Powerup MADC = 48; AOSR = 128; NDAC = 2, divider powered off; NDAC = 2, divider powered on
    { 11,0x82},
//			# reg[0][0][8] = 0x00                        ; D=0000 (MSB)
    {  8,0x00},
//			# reg[0][0][9] = 0x00                        ; D=0000 (LSB)
    {  9,0x00},
//			# reg[0][0][12] = 0x88                       ; reg(0)(0)(0x0c => 12)     DAC Powerup MDAC = 8
    { 12,0x88},
    {  0x7F,0x78},
#endif
//			# reg[120][0][50] = 0x88                     ; Interpolation Ratio is 8, FIFO = Enabled
    { 50,0x88},
    {  0x7F,0x64},
//			# reg[100][0][50] = 0xa4                     ; Decimation Ratio is 4, CIC AutoNorm = Enabled, FIFO = Enabled
    { 50,0xA4},
    {  0x7F,0x00},
#if (ENABLE_DSP_SYNC_MODE==1)
//			# reg[0][0][60] = 0x80                       ; reg(0)(0)(0x3c => 60)     DAC prog Mode, DAC & ADC filter engines powered up together
    { 60,0x80},
#else
//			# reg[0][0][60] = 0x00                       ; reg(0)(0)(0x3c => 60)     DAC prog Mode, DAC & ADC filter engines powered up together                          ; reg(0)(0)(0x3c => 60)     DAC prog Mode, DAC & ADC filter engines not powered up together
    { 60,0x00},
#endif
//			# reg[0][0][61] = 0x00                       ; reg(0)(0)(0x3d => 61)     ADC prog mode
    { 61,0x00},
#if (KITSUNE_CODE==1)
//			# reg[0][0][83] = 0x0                    ; adc vol control = 0db
    { 83,MIC_VOLUME_CONTROL},
//			# reg[0][0][84] = 0x0                    ; adc vol control = 0db
    { 84,MIC_VOLUME_CONTROL},
#endif
    {  0,0x01},
//			# reg[0][1][1 ] = 0x00                       ; reg(0)(1)(0x01 => 0 )     Crude avdd disabled
    {  1,0x00},
//			# reg[0][1][3 ] = 0x00                       ; reg(0)(1)(0x03 => 3 )     LDAC FIR
    {  3,0x00},
//			# reg[0][1][4 ] = 0x00                       ; reg(0)(1)(0x04 => 4 )     RDAC FIR
    {  4,0x00},
#if 0
//			# reg[0][1][31] = 0x80                       ; reg(0)(1)(0x1F =>31 )     HP in ground Centered mode; HPL gain 0 dB
    { 31,0x80},
//			# reg[0][1][32] = 0x00                       ; reg(0)(1)(0x20 =>32 )     HPR independent gain 0 dB
    { 32,0x00},
//			# reg[0][1][33] = 0x28                       ; reg(0)(1)(0x21 =>33 )     Charge pump runs on Osc./4
    { 33,0x28},
//			# reg[0][1][34] = 0x33 ;0x3e                       ; reg(0)(1)(0x22 =>34 )     Set CP mode
    { 34,0x33},
//			# reg[0][1][35] = 0x10                       ; reg(0)(1)(0x23 =>35 )     Power up CP with HP
    { 35,0x10},
//			# reg[0][1][52] = 0x40                       ; reg(0)(1)(0x34 => 52)     ADC IN1_L is selected for left P
    { 52,0x40},
//			# reg[0][1][54] = 0x40                       ; reg(0)(1)(0x36 => 54)     ADC CM1 is selected for left M
    { 54,0x40},
//			# reg[0][1][55] = 0x40                       ; reg(0)(1)(0x37 => 55)     ADC IN1_R is selected for right P
    { 55,0x40},
//			# reg[0][1][57] = 0x40                       ; reg(0)(1)(0x39 => 57)     ADC CM1 is selected for right M
    { 57,0x40},

#endif
//			# reg[0][1][121] = 0x33                      ; reg(0)(1)(0x79 => 121)    Quick charge time for Mic inputs
    {121,0x33},
//			# reg[0][1][122] = 0x01                      ; reg(0)(1)(0x7A => 122)    Vref charge time - 40 ms.
    {122,0x01},
#if (KITSUNE_CODE==1)
#if 0
	{ 51, (4<<0)},
#endif
#endif
};

static const reg_value REG_Section_program2[] = {
    {  0,0x0},
    {  0x7F,0x78},
//			# reg[120][0][24]=0x80
    { 24,0x80},
    {255,0x00},
    {255,0x01},
    {  0,0x0},
    {  0x7F,0x28},
//			# reg[40][0][1] = 0x04                        ; adaptive mode for ADC
    {  1,0x04},
    {  0x7F,0x50},
//			# reg[80][0][1] = 0x04                        ; adaptive mode for DAC
    {  1,0x04},
    {  0x7F,0x64},
#if (KITSUNE_CODE==1) // sync mode
//			# reg[100][0][48] = 12;IDAC  = 256    ; MDAC*DOSR;IADC  = 256    ; MADC*AOSR;IDAC  = 512    ; MDAC*DOSR;IADC  = 512    ; MADC*AOSR;IDAC  = 1024    ; MDAC*DOSR;IADC  = 1024    ; MADC*AOSR;IDAC  = 1536    ; MDAC*DOSR;IADC  = 1536   ; MADC*AOSR;IDAC  = 2048    ; MDAC*DOSR;IADC  = 2048   ; MADC*AOSR;IDAC  = 3072    ; MDAC*DOSR;IADC  = 3072   ; MADC*AOSR;IDAC  = 4096    ; MDAC*DOSR;IADC  = 4096   ; MADC*AOSR;IDAC  = 6144    ; MDAC*DOSR;IADC  = 6144   ; MADC*AOSR
	//{47,1<<0},
	{ 48,(IADC & 0xFF00) >> 8},
//			# reg[100][0][49] = 0
    { 49,(IADC & 0xFF) >> 0 },
    {  0x7F,0x78},
	//{47,1<<0},
//			# reg[120][0][48] = 12
    { 48,(IDAC & 0xFF00) >> 8 },

//			# reg[120][0][49] = 0
    { 49, (IDAC & 0xFF) >> 0 },
#else // async mode
//			# reg[100][0][48] = 4;IDAC  = 256    ; MDAC*DOSR;IADC  = 256    ; MADC*AOSR;IDAC  = 512    ; MDAC*DOSR;IADC  = 512    ; MADC*AOSR;IDAC  = 1024    ; MDAC*DOSR;IADC  = 1024    ; MADC*AOSR;IDAC  = 1536    ; MDAC*DOSR;IADC  = 1536   ; MADC*AOSR;IDAC  = 2048    ; MDAC*DOSR;IADC  = 2048   ; MADC*AOSR;IDAC  = 3072    ; MDAC*DOSR;IADC  = 3072   ; MADC*AOSR;IDAC  = 4096    ; MDAC*DOSR;IADC  = 4096   ; MADC*AOSR;IDAC  = 6144    ; MDAC*DOSR;IADC  = 6144   ; MADC*AOSR

    { 48,0x04},
//			# reg[100][0][49] = 0
    { 49,0x00},
    {  0x7F,0x78},
//			# reg[120][0][48] = 4
    { 48,0x04},
//			# reg[120][0][49] = 0
    { 49,0x00},

#endif
    {  0x7F,0x00},
//			# reg[0][0][63] = 0xc2                       ; reg(0)(0)(0x3f => 63)     DAC L&R DAC powerup Ldata-LDAC Rdata-RDAC (soft-stepping disable)
    { 63,0xC2},
//			# reg[0][0][64] = 0x00                       ; reg(0)(0)(0x40 => 64)     DAC Left and Right DAC unmuted with indep.  vol. ctrl
    { 64,0x00},
#if (KITSUNE_CODE==1)
#if 0
	// digital volume control
	{ 65, 0x30},
	{ 66, 0x30},
#endif
#endif
//			# reg[0][0][81] = 0xd6                      ; reg(0)(0)(0x51 => d6)     ADC Powerup ADC left and right channels in Digital mode(soft-stepping disable)
    { 81,0xD6},
//			# reg[0][0][82] = 0x00                       ; reg(0)(0)(0x51 => 81)     ADC Powerup ADC left and right channels (soft-stepping disable); reg(0)(0)(0x52 => 82)     ADC Unmute ADC left and right channels,L,R fine gain=0dB
    { 82,0x00},
    {  0,0x01},
#if (KITSUNE_CODE==1)
//			# reg[0][1][22] = 0xC3 ;
    { 22,0xC3},
//			# reg[0][1][45] = 0x06 ;
    { 45,0x06},
//			# reg[0][1][46] = 0x0C ; 		-6db
    { 46,0x0C},
//			# reg[0][1][47] = 0x0C ;
    { 47,0x0C},
#if (MUTE_SPK==1)
	//			# reg[0][1][48] = 0x21 ;   12db
	{ 48,0x00},
#else
//			# reg[0][1][48] = 0x21 ;   12db
    { 48,0x21},
#endif
#else
//			# reg[0][1][27] = 0x33                       ; reg(0)(1)(0x1B =>27 )     LDAC -> HPL, RDAC -> HPR; Power on HPL + HPR
    { 27,0x33},
#endif
//			# reg[0][1][59] = 0x00                       ; reg(0)(1)(0x3b => 59)     ADC unmute left mic PGA with 0 dB gain
 //   { 59,0x00},
//			# reg[0][1][60] = 0x00                       ; reg(0)(1)(0x3c => 60)     ADC unmute right mic PGA with 0 dB gain
//    { 60,0x00},
    {  0,0x04},
//			# reg[0][4][1]  = 0                          ; ASI1 Audio Interface = I2S
    {  1,0x00},
//			# reg[0][4][10] = 0                          ; ASI1 Audio Interface WCLK and BCLK
    { 10,0x00},
//			# reg[0][4][8]  = 0x50                       ; ASI1 Left DAC Datapath = Left Data, ASI1 Right DAC Datapath = Right Data
    {  8, (1 << 6) |  (1 << 4) }, // 0x50}, // TODO DKH
//			# reg[0][4][23] = 0x05 ; ASI2_IN_CH<L1,R1> = miniDSP_A_out_ch<L2,R2>
//    { 23,0x05},
//			# reg[0][4][24] = 0x50 ; ASI2_OUT_CH<L1> = Channel<L1> ; ASI2_OUT_CH<R1> = Channel<R1>
//    { 24,0x50},
//			# reg[0][4][39] = 0x06 ; ASI3_IN_CH<L1,R1> = miniDSP_A_out_ch<L3,R3>
//    { 39,0x06},
//			# reg[0][4][40] = 0x50 ; ASI3_OUT_CH<L1> = Channel<L1> ; ASI3_OUT_CH<R1> = Channel<R1>
//    { 40,0x50},
#if (KITSUNE_CODE==1)
#if 0
	//minidsp data port control
	{118, 0x00},

	// asi and adc/dac syncing
    {119,0x00},
#endif
#endif
    {  0,0x0},
    {  0x7F,0x64},
#if (ENABLE_DSP_SYNC_MODE==1)
//			# reg[100][0][20] = 0x00 ; Disable ADC double buffer mode
    { 20,0x00},
    {  0x7F,0x78},
//			# reg[120][0][20] = 0x00 ; Disable DAC double buffer mode
    { 20,0x00},
#else
//			# reg[100][0][20] = 0x80 ; Disable ADC double buffer mode; Disable DAC double buffer mode; Enable ADC double buffer mode
    { 20,0x80},
    {  0x7F,0x78},
//			# reg[120][0][20] = 0x80 ; Enable DAC double buffer mode
    { 20,0x80},
#endif
#if (KITSUNE_CODE==1)
    {  0x7F,0x00},
#if (NDAC_POWER_UP_LATER==1)
//			# reg[0][0][11] = 0x82                   ; NDAC = 2, divider powered off
    { 11,(1 << 7) | (NDAC << 0)},
#endif
#endif
    {  0x7F,0x64},
//			# reg[100][0][60] = 0x80                 ; NDAC = 2, divider powered off; Enable FIFO on CIC2
    { 60,0x80},
    {  0x7F,0x00},
//			# reg[0][0][112] = 0xd4; Enable CIC2 and Digital mic for Left and Right Channel
    {112, ( (1<<7) | (1 << 6) | (2 << 4) | (1 << 2) ) },//TODO DKH
    {  0,0x04},

#if (KITSUNE_CODE==1)
#if (CODEC_LEFT_LATCH_FALLING == 1)
	{100, (1 << 7) | (0 << 6) | (0 << 3) | (1 << 2)}, // TODO added by me for left latch falling
#endif
	//DigMic1 -Left and right			# reg[0][4][101] = 0x34     ; GPIO4 --> DigMic2 and DigMic3 data, GPIO5 --> DigMic4 and DigMic5 data
    {101,(3 << 4) | (4 << 0)},
//			# reg[0][4][91] = 0x04      ; GPIO6 in input mode.
    { 91,(0x01 << 2)},
//			# reg[0][4][90] = 0x04      ; GPIO5 in input mode
    { 90,(0x01 << 2)},
//			# reg[0][4][87] = 0x28      ; GPIO2 is ADC_MOD_CLK
    { 87,(0x0A << 2)},
	{89,(0x01 << 2)},
	{96,0}, //GPIO 1 is disabled
	// DigMic2 L&R ; GPIO6 --> DigMic2 and DigMic3 data, GPIO6 --> DigMic4 and DigMic5 data
	//TODO L&R set to Gpio 6 for dig mic 2
	{102, (5 << 4) | (5 << 0) },
	 {  0,0x04},
#else
//			# reg[0][4][101] = 0x45     ; GPIO5 --> DigMic2 and DigMic3 data, GPIO6 --> DigMic4 and DigMic5 data
    {101,0x45},
//			# reg[0][4][91] = 0x04      ; GPIO6 in input mode.
    { 91,0x04},
//			# reg[0][4][90] = 0x04      ; GPIO5 in input mode
    { 90,0x04},
//			# reg[0][4][87] = 0x28      ; GPIO2 is ADC_MOD_CLK
    { 87,0x28},

#endif
//			# reg[0][4][1]  = 0x00                          ; Assume CODEC slave mode for Multi channel, ASI1 Audio Interface = I2S mode
    {  1,0x00},
//			# reg[0][4][4] = 0x40        ;reg[0][4][12] = 0x81                           ; ASI1 master, BCLK = 6.144Mhz;reg[0][4][13] = 0x80                           ; ASI1_WDIV = 128; For 4 channel digmic,
    {  4,0x40},
//			# reg[0][4][4] = 0x40;reg[0][4][12] = 0x81                           ; ASI1 master, BCLK = 6.144Mhz;reg[0][4][13] = 0x80                           ; ASI1_WDIV = 128
    {  4,0x40},
    {  0,0x00},


//			# reg[0][0][82] = 0
    { 82,0x00},
#if 0 // AGC
//			# reg[0][0][86] = 32
    { 86,0x20},
//			# reg[0][0][87] = 254
    { 87,0xFE},
//			# reg[0][0][88] = 0
    { 88,0x00},
//			# reg[0][0][89] = 104
    { 89,0x68},
//			# reg[0][0][90] = 168
    { 90,0xA8},
//			# reg[0][0][91] = 6
    { 91,0x06},
//			# reg[0][0][92] = 0
    { 92,0x00},
//			# reg[0][0][84] = 0
    { 84,0x00},
//			# reg[0][0][94] = 32
    { 94,0x20},
//			# reg[0][0][95] = 254
    { 95,0xFE},
//			# reg[0][0][96] = 0
    { 96,0x00},
//			# reg[0][0][97] = 104
    { 97,0x68},
//			# reg[0][0][98] = 168
    { 98,0xA8},
//			# reg[0][0][99] = 6
    { 99,0x06},
//			# reg[0][0][100] = 0
    {100,0x00},
#endif
};

#include "codec_settings_priv.h"
