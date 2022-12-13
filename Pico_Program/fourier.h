#include <math.h>

// === the fixed point macros (16.15) ========================================
typedef signed int fix15 ;
#define multfix15(a,b) ((fix15)((((signed long long)(a))*((signed long long)(b)))>>15))
#define float2fix15(a) ((fix15)((a)*32768.0)) // 2^15
#define fix2float15(a) ((float)(a)/32768.0)
#define absfix15(a) abs(a) 
#define int2fix15(a) ((fix15)(a << 15))
#define fix2int15(a) ((int)(a >> 15))
#define char2fix15(a) (fix15)(((fix15)(a)) << 15)

// Number of samples per FFT
#define NUM_SAMPLES 2048
// Number of samples per FFT, minus 1
#define NUM_SAMPLES_M_1 2047
// Length of short (16 bits) minus log2 number of samples
#define SHIFT_AMOUNT 5
// Log2 number of samples
#define LOG2_NUM_SAMPLES 11


// Max and min macros
#define max(a,b) ((a>b)?a:b)
#define min(a,b) ((a<b)?a:b)

extern fix15 zero_point_4 ;

// Sine table for the FFT calculation
extern fix15 Sinewave[NUM_SAMPLES]; 

// Hann window table for FFT calculation
// extern fix15 window[NUM_SAMPLES]; 

void FFTfix(fix15 fr[], fix15 fi[]);
void init_fft(void);
void conjugate(fix15 fi[]);