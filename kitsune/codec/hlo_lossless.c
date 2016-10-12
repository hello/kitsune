#include "codec/hlo_lossless.h"

#include "codec/lpc.h"
#include "codec/rice.h"

//Block size is in samples !
#define BLOCK_SIZE 240
static const int32_t sample_rate = 16000;
static const int16_t bps = 16;
static const int8_t channels = 1;
static const int32_t estimated_frames = 800; //6 seconds
static const int16_t Q = 35;
static const int64_t corr = ((int64_t)1) << Q;
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
	int ret = 1;
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
	int ret = 1;

	int32_t qtz_ref_coeffs[MAX_LPC_ORDER];
	int32_t int_samples[BLOCK_SIZE];
	int32_t residues[BLOCK_SIZE];
	uint32_t unsigned_ref[MAX_LPC_ORDER];
	uint32_t encoded_ref[MAX_LPC_ORDER];
	uint32_t u_residues[BLOCK_SIZE];
	uint32_t encoded_residues[BLOCK_SIZE];
	int64_t lpc[MAX_LPC_ORDER + 1];
	int32_t qtz_samples[BLOCK_SIZE];
	int32_t autocorr[MAX_LPC_ORDER + 1];
	int32_t ref[MAX_LPC_ORDER];
	int32_t lpc_mat[MAX_LPC_ORDER][MAX_LPC_ORDER];

	uint8_t opt_lpc_order;
	uint8_t rice_param_ref,rice_param_residue;
	uint16_t req_int_ref,req_int_residues;
	uint32_t req_bits_ref,req_bits_residues;

	ret = hlo_stream_transfer_all(INTO_STREAM, output, (uint8_t*)&frame_sync,sizeof(int32_t), 4);
	if (ret < 0 ) return ret;
	for(i = 0; i < channels; i++)
	{
		//Separate channels
		for(j = 0; j < samples_per_channel; j++)
			short_samples[j] = buffer[channels * j + i];
		//Quantize sample data
		for(j = 0; j < samples_per_channel; j++)
			qtz_samples[j] = (short_samples[j]<<15);
		//Calculate autocorrelation data
		auto_corr_fun(qtz_samples,samples_per_channel,MAX_LPC_ORDER,1,
			autocorr);
		//Calculate reflection coefficients
		opt_lpc_order = compute_ref_coefs(autocorr,MAX_LPC_ORDER,ref);
		//Quantize reflection coefficients
		qtz_ref_cof(ref,opt_lpc_order,qtz_ref_coeffs);
		//signed to unsigned
		signed_to_unsigned(opt_lpc_order,qtz_ref_coeffs,unsigned_ref);

		//get optimum rice param and number of bits
		rice_param_ref = get_opt_rice_param(unsigned_ref,opt_lpc_order,
			&req_bits_ref);
		//Encode ref coeffs
		req_bits_ref = rice_encode_block(rice_param_ref,unsigned_ref,
			opt_lpc_order,encoded_ref);
		//Determine number of ints required for storage
		req_int_ref = (req_bits_ref+31)/(32);

		//Dequantize reflection
		dqtz_ref_cof(qtz_ref_coeffs,opt_lpc_order,ref);

		//Reflection to lpc
		levinson(NULL,opt_lpc_order,ref,lpc_mat);

		lpc[0] = 0;
		for(j = 0; j < opt_lpc_order; j++)
			lpc[j + 1] = corr * lpc_mat[opt_lpc_order - 1][j];

		for(j = opt_lpc_order; j < MAX_LPC_ORDER; j++)
			lpc[j + 1] = 0;

		//Copy samples
		for(j = 0; j < samples_per_channel; j++)
			int_samples[j] = short_samples[j];

		//Get residues
		calc_residue(int_samples,samples_per_channel,opt_lpc_order,Q,lpc,
			residues);

		//signed to unsigned
		signed_to_unsigned(samples_per_channel,residues,u_residues);

		//get optimum rice param and number of bits
		rice_param_residue = get_opt_rice_param(u_residues,
			samples_per_channel,&req_bits_residues);

		//Encode residues
		req_bits_residues = rice_encode_block(rice_param_residue,u_residues,
			samples_per_channel,encoded_residues);

		//Determine nunber of ints required for storage
		req_int_residues = (req_bits_residues+31)/(32);

		//Write rice_params,bytes,encoded lpc_coeffs to output
		ret = hlo_stream_transfer_all(INTO_STREAM, output, (uint8_t*)&rice_param_ref,sizeof(uint8_t), 4);
		if (ret < 0 ) break;
		ret = hlo_stream_transfer_all(INTO_STREAM, output, (uint8_t*)&req_int_ref,sizeof(uint16_t), 4);
		if (ret < 0 ) break;
		ret = hlo_stream_transfer_all(INTO_STREAM, output, (uint8_t*)&opt_lpc_order,sizeof(uint8_t), 4);
		if (ret < 0 ) break;
		ret = hlo_stream_transfer_all(INTO_STREAM, output, (uint8_t*)encoded_ref,req_int_ref*sizeof(uint32_t), 4);
		if (ret < 0 ) break;

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

int hlo_filter_lossless_encoder(hlo_stream_t * input, hlo_stream_t * output, void * ctx, hlo_stream_signal signal){
	int16_t short_samples[BLOCK_SIZE];
	int ret = hlo_lossless_start_stream(output);

	while(1){
		ret = hlo_stream_transfer_all(FROM_STREAM, input, (uint8_t*)short_samples, sizeof(short_samples) /*read_size*bps/8*/, 4);
		if (ret < 0 ) break;

		ret = hlo_lossless_write_frame( output, short_samples /*, (ret/(bps/8))/channels*/ );
		if (ret < 0 ) break;

		BREAK_ON_SIG(signal);
	}
	return ret;
}
