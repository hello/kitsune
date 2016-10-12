#ifndef _LPC_H_
#define _LPC_H_

#define SQRT2 46340
//1.4142135623730950488016887242096
#define MAX_LPC_ORDER 1

int32_t check_if_constant(const int16_t *data,int32_t num_elements);
void auto_corr_fun(int32_t *x,int32_t N,int64_t k,int16_t norm,int32_t *rxx);
void levinson(int32_t *autoc,uint8_t max_order,int32_t *ref,int32_t lpc[][MAX_LPC_ORDER]);
uint8_t compute_ref_coefs(int32_t *autoc,uint8_t max_order,int32_t *ref);
int32_t qtz_ref_cof(int32_t *par,uint8_t ord,int32_t *q_ref);
int32_t dqtz_ref_cof(const int32_t *q_ref,uint8_t ord,int32_t *ref);
void calc_residue(const int32_t *samples,int64_t N,int16_t ord,int16_t Q,int64_t *coff,int32_t *residues);
void calc_signal(const int32_t *residues,int64_t N,int16_t ord,int16_t Q,int64_t *coff,int32_t *samples);

#endif
