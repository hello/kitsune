#include "codec/hlo_lossless.h"

#include "codec/rice.h"

//Block size is in samples !
#define BLOCK_SIZE 240
static const int32_t sample_rate = 16000;
static const int16_t bps = 16;
static const int8_t channels = 1;
static const int32_t estimated_frames = 800; //6 seconds
static const char magic_number[5] = "hello";
static const uint32_t frame_sync = 0xAA55FF00;
static const int samples_per_channel = BLOCK_SIZE;


hlo_stream_t * hlo_lossless_init_chunkbuf( int size ) {
	return circ_stream_open(size);
}
int hlo_lossless_write_chunkbuf( hlo_stream_t * s, int16_t short_samples[] ) {
	return hlo_lossless_write_frame(s, short_samples);
}
int hlo_lossless_dump_chunkbuf( hlo_stream_t * s, hlo_stream_t * output ) {
	uint32_t word;
	int ret = 1;
	uint8_t buf[512];

	//scan for frame sync
	while( ret ) {
		ret = hlo_stream_transfer_all(FROM_STREAM, s, (uint8_t*)&word,sizeof(int32_t), 4);
		if (ret < 0 ) break;
		//since we are guaranteed writes work on the circ stream there much be a whole number
		//of frames after the first sync. It's possible frames of different sizes come in, in which case
		//an incoming frame could eat the header off the eldest frame to make room
		if( word == frame_sync ) {
			hlo_lossless_start_stream(output);
			ret = hlo_stream_transfer_all(INTO_STREAM, output, (uint8_t*)&frame_sync,sizeof(int32_t), 4);
			if (ret < 0 ) break;
			ret = hlo_stream_transfer_between(s,output,buf,sizeof(buf),4);
			if (ret < 0 ) break;
		}
	}
	hlo_stream_close(s);
	return ret;
}

int hlo_lossless_start_stream( hlo_stream_t * output) {
	int ret = 0;
	ret = hlo_stream_transfer_all(INTO_STREAM, output, (uint8_t*)&magic_number,sizeof(magic_number), 4);
	if (ret < 0 ) return ret;
	ret = hlo_stream_transfer_all(INTO_STREAM, output, (uint8_t*)&sample_rate,sizeof(int32_t), 4);
	if (ret < 0 ) return ret;
	ret = hlo_stream_transfer_all(INTO_STREAM, output, (uint8_t*)&bps,sizeof(int16_t), 4);
	if (ret < 0 ) return ret;
	ret = hlo_stream_transfer_all(INTO_STREAM, output, (uint8_t*)&channels,sizeof(int8_t), 4);
	if (ret < 0 ) return ret;
	ret = hlo_stream_transfer_all(INTO_STREAM, output, (uint8_t*)&estimated_frames,sizeof(int32_t), 4);
	return ret;
}
int hlo_lossless_write_frame(hlo_stream_t * output, int16_t short_samples[] ) {
	//Define read size
	int32_t i,j = 0;
	int16_t * buffer = short_samples;
	int ret = 0;

	int32_t residues[BLOCK_SIZE];
	uint32_t u_residues[BLOCK_SIZE];
	uint32_t encoded_residues[BLOCK_SIZE];

	uint8_t rice_param_residue;
	uint16_t req_int_residues;
	uint32_t req_bits_residues;

	ret = hlo_stream_transfer_all(INTO_STREAM, output, (uint8_t*)&frame_sync,sizeof(int32_t), 4);
	if (ret < 0 ) return ret;
	for(i = 0; i < channels; i++)
	{
		//Separate channels
		for(j = 0; j < samples_per_channel; j++)
			residues[j] = buffer[channels * j + i];

		//signed to unsigned
		signed_to_unsigned(samples_per_channel,residues,u_residues);

		//get optimum rice param and number of bits
		rice_param_residue = get_opt_rice_param(u_residues,samples_per_channel,&req_bits_residues);

		//Encode residues
		req_bits_residues = rice_encode_block(rice_param_residue,u_residues, samples_per_channel,encoded_residues);

		//Determine nunber of ints required for storage
		req_int_residues = (req_bits_residues+31)/(32);

		//Write rice_params,bytes,encoded residues to output
		ret = hlo_stream_transfer_all(INTO_STREAM, output, (uint8_t*)&rice_param_residue,sizeof(uint8_t), 4);
		if (ret < 0 ) break;
		ret = hlo_stream_transfer_all(INTO_STREAM, output, (uint8_t*)&req_int_residues,sizeof(uint16_t), 4);
		if (ret < 0 ) break;
		ret = hlo_stream_transfer_all(INTO_STREAM, output, (uint8_t*)&encoded_residues,req_int_residues*sizeof(uint32_t), 4);
		if (ret < 0 ) break;

	}
	return ret;
}
#include "uart_logger.h"
extern volatile unsigned int idlecnt;
int hlo_filter_lossless_encoder(hlo_stream_t * input, hlo_stream_t * output, void * ctx, hlo_stream_signal signal){
	int16_t short_samples[BLOCK_SIZE];
	int ret = hlo_lossless_start_stream(output);
	//x $ftestt$b $i192.168.7.24 h
	//x $a $i192.168.7.24 h
	//x $a $i192.168.7.24 x
	uint32_t got=0, sent=0;
	idlecnt = 0;

	while(1){
		ret = hlo_stream_transfer_all(FROM_STREAM, input, (uint8_t*)short_samples, sizeof(short_samples) /*read_size*bps/8*/, 4);
		if (ret < 0 ) break;
		got += ret;
		ret = hlo_lossless_write_frame( output, short_samples /*, (ret/(bps/8))/channels*/ );
		if (ret < 0 ) break;
		sent += ret;
		ret = hlo_lossless_write_frame( output, short_samples /*, (ret/(bps/8))/channels*/ );
		if (ret < 0 ) break;
		BREAK_ON_SIG(signal);
	}
	DISP("%d idle, got %d, sent %d, ratio %d\n", idlecnt, got, sent, sent*100/got);

	return ret;
}
