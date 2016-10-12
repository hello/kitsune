#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <math.h>

#include "lpc.h"

//Lookup Tables for 1st, 2nd and higher order quantized reflection coeffs
static const int32_t
lookup_1st_order_coeffs[128] =
{-2147483648,-2147221504,-2146435072,-2145124352,-2143289344,-2140930048,-2138046464,-2134638592,-2130706432,-2126249984,-2121269248,-2115764224,-2109734912,-2103181312,-2096103424,-2088501248,-2080374784,-2071724032,-2062548992,-2052849664,-2042626048,-2031878144,-2020605952,-2008809472,-1996488704,-1983643648,-1970274304,-1956380672,-1941962752,-1927020544,-1911554048,-1895563264,-1879048192,-1862008832,-1844445184,-1826357248,-1807745024,-1788608512,-1768947712,-1748762624,-1728053248,-1706819584,-1685061632,-1662779392,-1639972864,-1616642048,-1592786944,-1568407552,-1543503872,-1518075904,-1492123648,-1465647104,-1438646272,1411121152,-1383071744,-1354498048,-1325400064,-1295777792,-1265631232,-1234960384,-1203765248,-1172045824,-1139802112,-1107034112,-1073741824,-1039925248,-1005584384,-970719232,-935329792,-899416064,-862978048,-826015744,-788529152,-750518272,-711983104,-672923648,-633339904,-593231872,-552599552,-511442944,-469762048,-427556864,-384827392,-341573632,-297795584,-253493248,-208666624,-163315712,-117440512,-71041024,-24117248,23330816,71303168,119799808,168820736,218365952,268435456,319029248,370147328,421789696,473956352,526647296,579862528,633602048,687865856,742653952,797966336,853803008,910163968,967049216,1024458752,1082392576,1140850688,1199833088,1259339776,1319370752,1379926016,1441005568,1502609408,1564737536,1627389952,1690566656,1754267648,1818492928,1883242496,1948516352,2014314496,2080636928};

/*Checks if every sample of a block is equal or not*/
int32_t check_if_constant(const int16_t *data,int32_t num_elements)
{
	int16_t temp = data[0];
	int i;

	for( i = 1; i < num_elements; i++)
	{
		if(temp != data[i])
			return -1;
	}

	return 0;
}

/*autocorrelation function*/
void auto_corr_fun(int32_t *x,int32_t N,int64_t k,int16_t norm,int32_t *rxx)
{
	int64_t i, n;
	int32_t sum = 0,mean = 0;

	for (i = 0; i < N; i++)
		sum += x[i];
	mean = sum/N;

	for(i = 0; i <= k; i++)
	{
		rxx[i] = 0.0;
		for (n = i; n < N; n++)
			rxx[i] += ((int64_t)(x[n] - mean) * (x[n-i] - mean))>>15;
	}

	if(norm)
	{
		for (i = 1; i <= k; i++)
			rxx[i] /= rxx[0];
		rxx[0] = 1<<15;
	}

	return;
}

/*Levinson recursion algorithm*/
void levinson(int32_t *autoc,uint8_t max_order,int32_t *ref,int32_t lpc[][MAX_LPC_ORDER])
{
	int32_t i, j, i2;
	int32_t r, err, tmp;
	int32_t lpc_tmp[MAX_LPC_ORDER];

	for(i = 0; i < max_order; i++)
		lpc_tmp[i] = 0;
	err = 1.0;
	if(autoc)
		err = autoc[0];

	for(i = 0; i < max_order; i++)
	{
		if(ref)
			r = ref[i];
		else
		{
			r = -autoc[i+1];
			for(j = 0; j < i; j++)
				r -= lpc_tmp[j] * autoc[i-j];
			r /= err;
			err *= 1.0 - (r * r);
		}

		i2 = i >> 1;
		lpc_tmp[i] = r;
		for(j = 0; j < i2; j++)
		{
			tmp = lpc_tmp[j];
			lpc_tmp[j] += r * lpc_tmp[i-1-j];
			lpc_tmp[i-1-j] += r * tmp;
		}
		if(i & 1)
			lpc_tmp[j] += lpc_tmp[j] * r;

		for(j = 0; j <= i; j++)
			lpc[i][j] = -lpc_tmp[j];
	}

	return;
}

/*Calculate reflection coefficients*/
uint8_t compute_ref_coefs(int32_t *autoc,uint8_t max_order,int32_t *ref)
{
	int32_t i, j;
	int32_t error;
	int32_t gen[2][MAX_LPC_ORDER];
	uint8_t order_est;

	//Schurr recursion
	for(i = 0; i < max_order; i++)
		gen[0][i] = gen[1][i] = autoc[i+1];

	error = autoc[0];
	ref[0] = ((int64_t)-gen[1][0]<<15) / error;
	error += ((int64_t)gen[1][0] * ref[0] ) >> 15;
	for(i = 1; i < max_order; i++)
	{
		for(j = 0; j < max_order - i; j++)
		{
			gen[1][j] = gen[1][j+1] + (int64_t)ref[i-1] * gen[0][j] >> 15;
			gen[0][j] = (int64_t)gen[1][j+1] * ref[i-1] >> 15 + gen[0][j];
		}
        ref[i] = ((int64_t)-gen[1][0]<<15) / error;
		error += ((int64_t)gen[1][0] * ref[i])>>15;
	}

	//Estimate optimal order using reflection coefficients
	order_est = 1;
	for(i = max_order - 1; i >= 0; i--)
	{
		if(abs(ref[i]) > 0.05*(1<<15))
		{
			order_est = i+1;
			break;
		}
	}

	return(order_est);
}

static int32_t fastsqrt( int32_t x ) {
    int64_t s = x;
    x = 20000u;
    x = ( x + s / x )/2;
    x = ( x + s / x )/2;
    x = ( x + s / x )/2;
    x = ( x + s / x )/2;
    return x;
}

/*Quantize reflection coeffs*/
int32_t qtz_ref_cof(int32_t *par,uint8_t ord,int32_t *q_ref)
{
	int i;
	for( i = 0; i < ord; i++)
	{
		if(i == 0)
			q_ref[i] = (( ((SQRT2 * fastsqrt(par[i]  + 1)) >> 15) - (1<<15) )) >> 9;
	}

	return(0);
}

/*Dequantize reflection coefficients*/
int32_t dqtz_ref_cof(const int32_t *q_ref,uint8_t ord,int32_t *ref)
{
	int i;

	if(ord <= 1)
	{
		ref[0] = 0;
		return(0);
	}

	for( i = 0; i < ord; i++)
	{
		if(i == 0)
			ref[i] = lookup_1st_order_coeffs[q_ref[i] + 64];
	}

	return(0);
}

/*Calculate residues from samples and lpc coefficients*/
void calc_residue(const int32_t *samples,int64_t N,int16_t ord,int16_t Q,int64_t *coff,int32_t *residues)
{
	int64_t k, i;
	int64_t corr;
	int64_t y;

	corr = ((int64_t)1) << (Q - 1);//Correction term

	residues[0] = samples[0];
	for(k = 1; k <= ord; k++)
	{
		y = corr;
		for(i = 1; i <= k; i++)
		{
			y += (int64_t)coff[i] * samples[k-i];
			if((k - i) == 0)
				break;
		}
		residues[k] = samples[k] - (int32_t)(y >> Q);
	}

	for(k = ord + 1; k < N; k++)
	{
		y = corr;

		for(i = 0; i <= ord; i++)
			y += (int64_t)(coff[i] * samples[k-i]);
		residues[k] = samples[k] - (int32_t)(y >> Q);
	}

	return;
}

/*Calculate samples from residues and lpc coefficients*/
void calc_signal(const int32_t *residues,int64_t N,int16_t ord,int16_t Q,int64_t *coff,int32_t *samples)
{
	int64_t k, i;
	int64_t corr;
	int64_t y;

	corr = ((int64_t)1) << (Q - 1);//Correction term

	samples[0] = residues[0];
	for(k = 1; k <= ord; k++)
	{
		y = corr;
		for(i = 1; i <= k; i++)
		{
			y -= (int64_t)(coff[i] * samples[k-i]);
			if((k - i) == 0)
				break;
		}
		samples[k] = residues[k] - (int32_t)(y >> Q);
	}

	for(k = ord + 1; k < N; k++)
	{
		y = corr;

		for(i = 0; i <= ord; i++)
			y -= (int64_t)(coff[i] * samples[k-i]);
		samples[k] = residues[k] - (int32_t)(y >> Q);
	}

	return;
}
