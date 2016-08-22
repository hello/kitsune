#include "pn_stream.h"
#include "pn.h"
#include <string.h>
#include <stdint.h>

static hlo_stream_t * _pstream;
static hlo_stream_vftbl_t _tbl;

static int close (void * context) {
	if (_pstream) {
		hlo_stream_close(_pstream);
	}

	_pstream = NULL;

	return 0;
}

static int read(void * ctx, void * buf, size_t size) {
	//gets bit from the PN sequence
	uint32_t i;
	int16_t * p16 = (int16_t *)buf;
	for (i = 0; i < size / sizeof(int16_t); i++) {
		if (pn_get_next_bit()) {
			p16[i] = INT16_MAX;
		}
		else {
			p16[i] = -INT16_MAX;
		}
	}

	return 0;
}

void pn_stream_init(void) {
	memset(&_tbl,0,sizeof(_tbl));

	_tbl.close = close;
	_tbl.read = read;

	_pstream = hlo_stream_new(&_tbl,NULL,HLO_STREAM_READ);


	pn_init_with_mask_9();


}

hlo_stream_t * pn_stream_open(void){
	if (!_pstream) {
		pn_stream_init();
	}

	return _pstream;
}
