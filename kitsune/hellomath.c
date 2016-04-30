#include "hellomath.h"
#include <stdlib.h> //abs


static inline uint16_t umultq16(uint16_t a, uint16_t b) {
    uint32_t c;
    uint16_t * p = (uint16_t *)(&c);
    
    c = (uint32_t)a * (uint32_t)b;
    return *(p + 1);
}


#define iter1(N) \
t = root + (1 << (N)); \
if (n >= t << (N))   \
{   n -= t << (N);   \
    root |= 2 << (N); \
}

uint32_t fxd_sqrt (uint32_t n) {
    unsigned int root = 0,t;
    
    iter1 (15);    iter1 (14);    iter1 (13);    iter1 (12);
    iter1 (11);    iter1 (10);    iter1 ( 9);    iter1 ( 8);
    iter1 ( 7);    iter1 ( 6);    iter1 ( 5);    iter1 ( 4);
    iter1 ( 3);    iter1 ( 2);    iter1 ( 1);    iter1 ( 0);
    return root >> 1;
}

uint32_t fxd_sqrt_q10(uint32_t x) {
    uint32_t topbits = (x & 0xFFC00000);
    
    topbits >>= 22;
    
    if (topbits & 0x00000001) {
        topbits++;
    }
    
    
    x >>= topbits;
    
    x <<= 10;
    
    x = fxd_sqrt(x);
    
    topbits >>= 1;
    x <<= (topbits);
    
    return x;
    
}


/*
 0000 - 0
 0001 - 1
 0010 - 2
 0011 - 2
 0100 - 3
 0101 - 3
 0110 - 3
 0111 - 3
 1000 - 4
 1001 - 4
 1010 - 4
 1011 - 4
 1100 - 4
 1101 - 4
 1110 - 4
 1111 - 4
 */
#define BIT_COUNT_LOOKUP_SIZE (16)
static const uint8_t k_bit_count_lookup[BIT_COUNT_LOOKUP_SIZE] =
{0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4};


uint8_t CountHighestMsb(uint64_t x) {
    short j;
    uint8_t count = 0;
    for (j = 0; j < 16; j++) {
        if (x < 16) {
            count += k_bit_count_lookup[x];
            break;
        }
        count += 4;
        x >>= 4;
    }
    return count;
}

/*  Not super high precision, but oh wells! */
#define LOG2_LOOKUP_SIZE_2N (8)
#define LOG2_LOOKUP_SIZE ((1 << LOG2_LOOKUP_SIZE_2N))

#if LOG2_LOOKUP_SIZE_2N == 8
static const int16_t k_log2_lookup_q10[LOG2_LOOKUP_SIZE] = { -32767, -8192,
		-7168, -6569, -6144, -5814, -5545, -5317, -5120, -4946, -4790, -4650,
		-4521, -4403, -4293, -4191, -4096, -4006, -3922, -3842, -3766, -3694,
		-3626, -3560, -3497, -3437, -3379, -3323, -3269, -3217, -3167, -3119,
		-3072, -3027, -2982, -2940, -2898, -2858, -2818, -2780, -2742, -2706,
		-2670, -2636, -2602, -2568, -2536, -2504, -2473, -2443, -2413, -2383,
		-2355, -2327, -2299, -2272, -2245, -2219, -2193, -2168, -2143, -2119,
		-2095, -2071, -2048, -2025, -2003, -1980, -1958, -1937, -1916, -1895,
		-1874, -1854, -1834, -1814, -1794, -1775, -1756, -1737, -1718, -1700,
		-1682, -1664, -1646, -1629, -1612, -1594, -1578, -1561, -1544, -1528,
		-1512, -1496, -1480, -1464, -1449, -1434, -1419, -1404, -1389, -1374,
		-1359, -1345, -1331, -1317, -1303, -1289, -1275, -1261, -1248, -1235,
		-1221, -1208, -1195, -1182, -1169, -1157, -1144, -1132, -1119, -1107,
		-1095, -1083, -1071, -1059, -1047, -1036, -1024, -1013, -1001, -990,
		-979, -967, -956, -945, -934, -924, -913, -902, -892, -881, -871, -860,
		-850, -840, -830, -820, -810, -800, -790, -780, -770, -760, -751, -741,
		-732, -722, -713, -704, -694, -685, -676, -667, -658, -649, -640, -631,
		-622, -613, -605, -596, -588, -579, -570, -562, -554, -545, -537, -529,
		-520, -512, -504, -496, -488, -480, -472, -464, -456, -448, -440, -433,
		-425, -417, -410, -402, -395, -387, -380, -372, -365, -357, -350, -343,
		-335, -328, -321, -314, -307, -300, -293, -286, -279, -272, -265, -258,
		-251, -244, -237, -231, -224, -217, -211, -204, -197, -191, -184, -178,
		-171, -165, -158, -152, -145, -139, -133, -126, -120, -114, -108, -102,
		-95, -89, -83, -77, -71, -65, -59, -53, -47, -41, -35, -29, -23, -17,
		-12, -6 };
#endif

#if LOG2_LOOKUP_SIZE_2N == 5
static const int16_t k_log2_lookup_q10[LOG2_LOOKUP_SIZE] =
{	-32767,-5120,-4096,-3497,-3072,-2742,-2473,-2245,-2048,-1874,-1718,-1578,-1449,-1331,-1221,-1119,-1024,-934,-850,-770,-694,-622,-554,-488,-425,-365,-307,-251,-197,-145,-95,-47};
#endif


int32_t FixedPointLog2Q10(uint64_t x) {
    int32_t ret;
    int16_t msb;
    int16_t shift = 0;
    if (x <= 0) {
        return k_log2_lookup_q10[0];
    }
    
    msb = CountHighestMsb(x);
    
    if (msb > LOG2_LOOKUP_SIZE_2N) {
        x >>= (msb - LOG2_LOOKUP_SIZE_2N);
        shift += msb - LOG2_LOOKUP_SIZE_2N;
    }
    
    shift -= (10 - LOG2_LOOKUP_SIZE_2N);
    
    ret = k_log2_lookup_q10[(uint16_t)x];
    ret += shift * 1024;
    
    return ret;
}

/* 2^(a + b) ---> 2^a * 2^b
 and exp(log(2)* x) == 2^x
 
 if 0 <= b < 1.... can approximate exp11
 */
uint32_t FixedPointExp2Q10(const int16_t x) {
#define LOG2_Q16 (45426)
#define B_Q16 (33915)
#define C_Q16  (21698)
#define ONE_Q10 (1 << 10)
#define ONE_Q16 (1 << 16)
    
    uint32_t accumulator;
    uint16_t ux;
    uint16_t a,b;
    uint16_t utemp16;
    
    //2^22 is the max we can do (32 - 10 == 22)
    if (x >= 22527) {
        return 0xFFFFFFFF;
    }
    
    if (x == -32768) {
        return 0;
    }
    
    ux = abs(x);
    
    //split to the left and right of the binary point (Q10)
    a = ux & 0xFC00;
    b = ux & 0x03FF;
    a >>= 10; //get Q10 as ints
    
    //multipy by log2 so  we compute exp(log(2) b) ===> 2^b
    b = umultq16((uint16_t)b,LOG2_Q16);
    
    
    b <<= 6; //convert from Q10 to Q16
    
    if (x > 0) {
        accumulator = ONE_Q16 + b;
    }
    else {
        accumulator = ONE_Q16 - b;
    }
    
    
    utemp16 = umultq16(b, b);
    utemp16 = umultq16(B_Q16, utemp16);
    
    accumulator += utemp16;
    
    utemp16 = umultq16(utemp16,b);
    utemp16 = umultq16(C_Q16, utemp16);
    
    
    if (x > 0) {
        accumulator += utemp16;
    }
    else {
        accumulator -= utemp16;
    }
    
    accumulator >>= 6; //convert back to Q10
    
    
    if (x < 0) {
        accumulator >>= a;
    }
    else {
        accumulator <<= a;
    }
    
    return accumulator;
}

uint8_t VecNormalize8(int8_t * vec, uint8_t n) {
    int32_t accumulator = 0;
    int16_t divisor;
    uint16_t i;
    int16_t temp16;
    
    for (i = 0; i < n; i++) {
        accumulator += (int16_t)vec[i]*(int16_t)vec[i];
    }
    
    divisor = fxd_sqrt(accumulator);
    
    if (divisor <= 0) {
        return 0;
    }
    
    divisor = (1 << 14) / divisor;
    
    for (i = 0; i < n; i++) {
        temp16 = (int16_t)vec[i]*(int16_t)divisor;
        temp16 += (1 << 6);
        temp16 >>= 7;
        
        //hacky, but we want to stay in range of -127 and 127 (symmetric)
        if (temp16 == 128) {
            temp16--;
        }
        
        if (temp16 == -128) {
            temp16++;
        }
        
        vec[i] = (int8_t)temp16;
    }
    
    return 1;
    
}

int16_t cosvec16(const int16_t * vec1, const int16_t * vec2, uint8_t n) {
    int32_t temp1,temp2,temp3;
    static const uint8_t q = 10;
    uint8_t i;
    
    temp1 = 0;
    temp2 = 0;
    temp3 = 0;
    for (i = 0; i < n; i++) {
        temp1 += (int32_t)vec1[i]*(int32_t)vec1[i];
        temp2 += (int32_t)vec2[i]*(int32_t)vec2[i];
        temp3 += (int32_t)vec1[i]*(int32_t)vec2[i];
    }
    
    if (!temp1 || !temp2) {
        return INT16_MAX;
    }
    
    temp1 = fxd_sqrt(temp1);
    temp2 = fxd_sqrt(temp2);
    
    if (temp1 > temp2) {
        temp3 /= temp2;
        temp3 <<= q;
        temp3 /= temp1;
    }
    else {
        temp3 /= temp1;
        temp3 <<= q;
        temp3 /= temp2;
    }
    
    return (int16_t) temp3;
    
}

int16_t cosvec8(const int8_t * vec1, const int8_t * vec2, uint8_t n) {
    int32_t temp1,temp2,temp3;
    static const uint8_t q = 10;
    uint8_t i;
    
    temp1 = 0;
    temp2 = 0;
    temp3 = 0;
    for (i = 0; i < n; i++) {
        temp1 += (int16_t)vec1[i]*(int16_t)vec1[i];
        temp2 += (int16_t)vec2[i]*(int16_t)vec2[i];
        temp3 += (int16_t)vec1[i]*(int16_t)vec2[i];
    }
    
    if (!temp1 || !temp2) {
        return INT16_MAX;
    }
    
    temp1 = fxd_sqrt(temp1);
    temp2 = fxd_sqrt(temp2);
    
    if (temp1 > temp2) {
        temp3 /= temp2;
        temp3 <<= q;
        temp3 /= temp1;
    }
    else {
        temp3 /= temp1;
        temp3 <<= q;
        temp3 /= temp2;
    }
    
    return (int16_t) temp3;
    
}

void Scale16VecTo8(int8_t * pscaled, const int16_t * vec,uint16_t n) {
    int16_t extrema = 0;
    int16_t i;
    int16_t absval;
    int16_t shift;
    
    for (i = 0; i < n; i++) {
        absval = abs(vec[i]);
        if (absval > extrema) {
            extrema = absval;
        }
    }
    
    shift = CountHighestMsb(extrema);
    shift -= 7;
    
    if (shift > 0 ) {
        for (i = 0; i < n; i++) {
            pscaled[i] = vec[i] >> shift;
        }
    }
    else {
        for (i = 0; i < n; i++) {
            pscaled[i] = (int8_t)vec[i];
        }
    }
    
}

void MatMul(int16_t * out, const int16_t * a, const int16_t * b, uint8_t numrowsA,uint8_t numcolsA, uint8_t numcolsB,uint8_t q) {
    const int16_t * arow;
    const int16_t * brow;
    int16_t * crow;
    uint16_t iRow,iCol,iBrow;
    int32_t temp32;
    
    arow = a;
    crow = out;
    for (iRow = 0; iRow < numrowsA; iRow++) {
        for (iCol = 0; iCol < numcolsB; iCol++) {
            temp32 = 0;
            brow = b + iCol;
            for (iBrow = 0; iBrow < numcolsA; iBrow++) {
                temp32 += (int32_t)arow[iBrow] * (int32_t)(*brow);
                brow += numcolsB;
            }
            temp32 >>= q;
            crow[iCol] = temp32;
        }
        arow += numcolsA;
        crow += numcolsB;
    }
}









