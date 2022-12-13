/**
 *  ECE4760 Fall 2022 Final Project
    Bianca, Crystal, Rosie
 */

// Include VGA graphics library
#include "vga_graphics.h"
// Include necessary libraries
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/sync.h"
#include "hardware/spi.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/adc.h"
#include "hardware/irq.h"
#include "hardware/i2c.h"

// Include protothreads
#include "pt_cornell_rp2040_v1.h"

// helper functions for vga graphing
#include "vga_graphing.h"

// fft functions
#include "fourier.h"

//-----------------------------------------------------------------------------
// Audio Processing and Output
//-----------------------------------------------------------------------------
// DAC parameters (see the DAC datasheet)
// A-channel, 1x, active
#define DAC_config_chan_A 0b0011000000000000
// B-channel, 1x, active
#define DAC_config_chan_B 0b1011000000000000

// Amplitude output (0 to 4096) to each channel
int DAC_output_0; // right
int DAC_output_1; // left

// SPI data
uint16_t DAC_data_0 ; // output value
uint16_t DAC_data_1 ; // output value

#define EXTRA 200
//DAC output buffer, 200 extra samples to store history in order to index with delays
short DAC_buffer_0[NUM_SAMPLES+EXTRA];
short DAC_buffer_1[NUM_SAMPLES+EXTRA];
short dac_size=0; //pointer to current position in DAC_buffer

short volume = 100;

// coordinates of audio source in simulated centimeters
short source_x = 0;
short source_y = 100;

short source_x_prev = 0;
short source_y_prev = 100;

// Number of samples to delay
short left_delay = 0;
short right_delay = 0;

//float instead of short beacuse we gradually approach target value
float left_amp = 1000.0; //0 to 1000
float right_amp = 1000.0;

// Amplitude calculated by algorithm
short left_amp_target = 1000;
short right_amp_target = 1000;

// Used for vga debugging 
short source_angle = 0;

bool filtering = true;


#define SERIAL_SIZE 40
//uncomment for debugging serial messages through vga
//char serial_input[SERIAL_SIZE];


//-----------------------------------------------------------------------------
// FFT
//-----------------------------------------------------------------------------

// Contains raw ADC audio input 
fix15 fr_buf_0[NUM_SAMPLES] ;
fix15 fr_buf_1[NUM_SAMPLES] ;
int fft_i = 0; //pointer for fr_buf

// Working array for in-place FFT, processing, and IFFT
fix15 fr_in_0[NUM_SAMPLES] ;
fix15 fi_in_0[NUM_SAMPLES] ;
fix15 fr_in_1[NUM_SAMPLES] ;
fix15 fi_in_1[NUM_SAMPLES] ;

// Magnitude values of channel 0 fft+lowpass result for vga to graph 
fix15 draw_buffer[512];

// Store results of audio processing to load into DAC output buffer
short fft_out_buffer_0[NUM_SAMPLES];
short fft_out_buffer_1[NUM_SAMPLES];

// Low pass filter transfer function segment
#define FILT_SIZE 300
fix15 filter[FILT_SIZE]; //symmetry


//-----------------------------------------------------------------------------
// General
//-----------------------------------------------------------------------------
//SPI configurations (note these represent GPIO number, NOT pin number)
#define PIN_MISO 4
#define PIN_CS   5
#define PIN_SCK  6
#define PIN_MOSI 7
#define LDAC     8
#define LED      25
#define SPI_PORT spi0

// Audio input
#define PIN_ADC_0 28
#define PIN_ADC_1 26

// Two variables to store core number
volatile int corenum_0 ;
volatile int corenum_1 ;

static struct pt_sem vga_semaphore; // controls frequency of vga updates
static struct pt_sem fft_semaphore_0; // tells fft to start
static struct pt_sem fft_semaphore_1;
static struct pt_sem load_semaphore_0; // tells fft to load output into buffer
static struct pt_sem load_semaphore_1;

//-----------------------------------------------------------------------------

// In place application of digital low pass filter using global filter array
// with the global filter applied at the cutoff bin of fr and fi.
void low_pass(fix15 fr[], fix15 fi[], int cutoff){
    // Values before cutoff remain at 1 (don't need to multiply)
    // Spectrum overlapping with filter window - perform convolution
    for(int i = 0; i < FILT_SIZE; i++){
        if(cutoff + i >= NUM_SAMPLES/2) return; //done with convolution before filter ends
        fr[cutoff + i] = multfix15(fr[cutoff + i], filter[i]);
        fi[cutoff + i] = multfix15(fi[cutoff + i], filter[i]);
        //filter is symmetrical (conjugate) around center of array
        //so need to apply the same filter from both ends of the array
        fr[NUM_SAMPLES-1-cutoff-i] = multfix15(fr[NUM_SAMPLES-1-cutoff-i], filter[i]);
        fi[NUM_SAMPLES-1-cutoff-i] = multfix15(fi[NUM_SAMPLES-1-cutoff-i], filter[i]);
    }
    // Values after end of filter are 0, since filter goes from 1 to 0
    for(int i = cutoff + FILT_SIZE; i<1024; i++){
        fr[i] = 0;
        fi[i] = 0;
        fr[NUM_SAMPLES-1-i] = 0;
        fi[NUM_SAMPLES-1-i] = 0;
    }
}

int dist_squared(int a, int b){
    return a*a + b*b;
}

// Calculates delay and updates amplitude if needed
void algo(){
    
    // Only run calculations if position has updated
    if(abs(source_x-source_x_prev)>3 || abs(source_y-source_y_prev)>3){
        // Calculate right_delay and left_delay
        // Get difference in distance to source, calculate number of samples to delay

        int left_dist2 = dist_squared(source_x+10, source_y);
        int right_dist2 = dist_squared(source_x-10, source_y);

        // Use speed of sound (343 m/s) to calculate delay using distance
        float left_delay_ms = sqrt((float)(left_dist2)) / 34.3; //ms
        left_delay = left_delay_ms * 2 * 12500 / 1000; //scale by 2, multiply by sampling frequency, convert ms
        float right_delay_ms = sqrt((float)(right_dist2)) / 34.3; //ms
        right_delay = right_delay_ms * 2 * 12500 / 1000;

        //normalize, make less delayed 0
        if(left_delay < right_delay){
            right_delay = (right_delay-left_delay);
            left_delay = 0;
        }
        else{
            left_delay = (left_delay-right_delay);
            right_delay = 0;
        }

        source_x_prev = source_x;
        source_y_prev = source_y;
    }

    //gradual amplitude changes instead of jumps
    if(abs(right_amp-right_amp_target)>1){
        float sign = (right_amp_target>right_amp) ? 1 : -1;
        right_amp = right_amp + sign*0.05;
    }
    else right_amp = (float) right_amp_target;

    if(abs(left_amp-left_amp_target)>1){
        float sign = (left_amp_target>left_amp) ? 1 : -1;
        left_amp = left_amp + sign * 0.05;
    }
    else left_amp = (float) left_amp_target;
}

// This timer ISR is called on core 0, runs at 12.5kHz
bool repeating_timer_callback_core_0(struct repeating_timer *t) {

    // Obtain audio input
    adc_select_input(0); //channel 0
    int audio_in_0 = adc_read();

    adc_select_input(2); //channel 1
    int audio_in_1 = adc_read();

    //Read audio input into buffer
    fr_buf_0[fft_i] = int2fix15(audio_in_0);
    fr_buf_1[fft_i] = int2fix15(audio_in_1);
    fft_i++;

    if(fft_i == NUM_SAMPLES){ //acquired enough samples for fft
        for(int i = 0; i<NUM_SAMPLES; i++){ //load values into fr_in
            fr_in_0[i] = fr_buf_0[i];
            fi_in_0[i] = 0;
            fr_in_1[i] = fr_buf_1[i];
            fi_in_1[i] = 0;
        }
        fft_i = 0; //reset to continue reading new data into the buffer

        PT_SEM_SIGNAL(pt, &fft_semaphore_0); //perform FFT
        PT_SEM_SIGNAL(pt, &fft_semaphore_1);
    }

    // Calculate/update values of delay and amplitude
    algo(); 

    //Grab next pair of outputs to the speakers/headphones
    DAC_output_0 = DAC_buffer_0[dac_size - right_delay] * right_amp * volume / 100000;
    DAC_output_1 = DAC_buffer_1[dac_size - left_delay] * left_amp * volume / 100000;
    dac_size++;

    //Output buffer has run out, load fft output buffer into dac output buffer
    if(dac_size>= NUM_SAMPLES + EXTRA){ //Extra is used for right_delay and left_delay
        for(int i = 0; i < EXTRA; i++){
            DAC_buffer_0[i] = DAC_buffer_0[NUM_SAMPLES+i];
            DAC_buffer_1[i] = DAC_buffer_1[NUM_SAMPLES+i];
        }
        for(int i = 0; i<NUM_SAMPLES; i++){
            DAC_buffer_0[i + EXTRA] = min(4095, fft_out_buffer_0[i]);
            DAC_buffer_1[i + EXTRA] = min(4095, fft_out_buffer_1[i]);
        }
        dac_size = 0 + EXTRA;

        PT_SEM_SIGNAL(pt, &load_semaphore_0); //fft output loaded, can replace with new results
        PT_SEM_SIGNAL(pt, &load_semaphore_1);
    }

    // Configure DAC output
    DAC_data_0 = (DAC_config_chan_B | ((DAC_output_0) & 0xffff));
    DAC_data_1 = (DAC_config_chan_A | ((DAC_output_1) & 0xffff));

    // SPI write
    spi_write16_blocking(SPI_PORT, &DAC_data_0, 1);
    spi_write16_blocking(SPI_PORT, &DAC_data_1, 1);
    
    return true;
}

// Runs at 1 kHz
bool repeating_timer_callback_vga(struct repeating_timer *t) {
    PT_SEM_SIGNAL(pt, &vga_semaphore);
    return true;
}

static PT_THREAD (protothread_serial(struct pt *pt))
{
    PT_BEGIN(pt) ;

    static char user_in[SERIAL_SIZE]; //user input buffer
    
    while(1) {
        for(int i = 0; i<20; i++) user_in[i] = '\0'; //clear the user input buffer

        serial_read ;
        sscanf(pt_serial_in_buffer,"%s", &user_in) ; //read serial data into buffer
    
        //to display serial input on vga
        //for(int i = 0; i<SERIAL_SIZE; i++) serial_input[i] = user_in[i];

        int acc = 0; //number value
        int sign = 1; //whether the number is positive or negative
        int ser_state = 0; //which value we are currently reading

        //Assumes serial input is well-formed and valid
        //format: x{num}y{num}v{num} etc, scalable for more values
        if(user_in[0]=='x'){
            for(int i = 1; i<SERIAL_SIZE; i++){

                char c = user_in[i];

                if(ser_state==0){ //reading x
                    if(c=='-') sign = -1;
                    else if(c>='0' && c<='9') acc = acc*10 + c - '0';
                    else{ //done reading x
                        source_x = sign*acc;
                        ser_state++;
                        acc = 0;
                        sign = 1;
                    }
                }
                else if(ser_state==1){ //reading y
                    if(c=='-') sign = -1;
                    else if(c>='0' && c<='9') acc = acc*10 + c - '0';
                    else{ 
                        source_y = sign*acc;
                        ser_state++;
                        acc = 0;
                        sign = 1;
                    }
                }
                else if(ser_state==2){ //reading v
                    if(c>='0' && c<='9') acc = acc*10 + c - '0';
                    else{ 
                        volume = acc; //0-100
                        ser_state++;
                        acc = 0;
                        sign = 1;
                    }
                }
                else if(ser_state==3){ //reading a
                    if(c=='-') sign = -1;
                    else if(c>='0' && c<='9') acc = acc*10 + c - '0';
                    else{ 
                        source_angle = sign*acc;
                        ser_state++;
                        acc = 0;
                        sign = 1;
                    }
                }
                else if(ser_state==4){ //reading f
                    if(c>='0' && c<='9') acc = acc*10 + c - '0';
                    else{ 
                        filtering = (acc==1);
                        ser_state++;
                        acc = 0;
                    }
                }
                else if(ser_state==5){ //reading l
                    if(c>='0' && c<='9') acc = acc*10 + c - '0';
                    else{ 
                        left_amp_target = acc;
                        ser_state++;
                        acc = 0;
                    }
                }
                else if(ser_state==6){ //reading r
                    if(c>='0' && c<='9') acc = acc*10 + c - '0';
                    else{ 
                        right_amp_target = acc;
                        ser_state++;
                        acc = 0;
                    }
                }
                else i = SERIAL_SIZE; //done parsing 
            }
        }
    }
    PT_END(pt) ;
}

//FFT THREAD
static PT_THREAD (protothread_fft_0(struct pt *pt))
{
    PT_BEGIN(pt) ;
    
    while(1) {
        PT_SEM_WAIT(pt, &fft_semaphore_0); //only do fft with enough samples

        int cut_0 = (int) right_amp_target; //use the amplitude value as the cutoff bin for lowpass frequency

        FFTfix(fr_in_0, fi_in_0);

        if(filtering) low_pass(fr_in_0, fi_in_0, cut_0);

        for (int i=0; i<512; i++) {
            fix15 one = max(abs(fr_in_0[i*2]), abs(fi_in_0[i*2])) + multfix15(min(abs(fr_in_0[i*2]), abs(fi_in_0[i*2])), zero_point_4);
            fix15 two = max(abs(fr_in_0[i*2+1]), abs(fi_in_0[2*i+1])) + multfix15(min(abs(fr_in_0[i*2+1]), abs(fi_in_0[i*2+1])), zero_point_4);
            draw_buffer[i] = max(one, two);
        }

        // Inverse FFT
        conjugate(fi_in_0);
        FFTfix(fr_in_0, fi_in_0);
        conjugate(fi_in_0);

        PT_SEM_WAIT(pt, &load_semaphore_0); //don't load fft results to fft_out_buffer until DAC_buffer loads it first

        for(int i = 0; i<NUM_SAMPLES; i++){
            //load results into buffer for speakers output
            fft_out_buffer_0[i] = (int)(fix2float15(fr_in_0[i])*2048.0);
        }
    
    }

    PT_END(pt) ;
}

//FFT THREAD
static PT_THREAD (protothread_fft_1(struct pt *pt))
{
    PT_BEGIN(pt) ;
    
    while(1) {
        PT_SEM_WAIT(pt, &fft_semaphore_1); 
        
        int cut_1 = (int) left_amp_target;

        // FFT
        FFTfix(fr_in_1, fi_in_1);

        // Lowpass
        if(filtering) low_pass(fr_in_1, fi_in_1, cut_1);

        // IFFT
        conjugate(fi_in_1);
        FFTfix(fr_in_1, fi_in_1);
        conjugate(fi_in_1);

        PT_SEM_WAIT(pt, &load_semaphore_1);

        // Load result to fft_out_buffer when ready
        for(int i = 0; i<NUM_SAMPLES; i++){
            fft_out_buffer_1[i] = (int)(fix2float15(fr_in_1[i])*2048.0);
        }
    }
    PT_END(pt) ;
}

void vga_print_int(int x, int y, char title[], int value, int size){
    setCursor(x, y);
    writeString(title);
    char output_arr[size];
    sprintf(output_arr, "%d", value);
    writeString(output_arr);
}

void vga_print_float(int x, int y, char title[], float value, int size){
    setCursor(x, y);
    writeString(title);
    char output_arr[size];
    sprintf(output_arr, "%.3f", value);
    writeString(output_arr);
}

static PT_THREAD (protothread_vga(struct pt *pt))
{
    // Mark beginning of thread
    PT_BEGIN(pt);

    // Control rate of drawing
    static int throttle = 0 ;

    // Draw the static aspects of the display
    setTextColor(WHITE);
    setTextSize(2) ;
    setCursor(30, 10);
    writeString("Real-Time Spatial Audio");
    setTextSize(1) ;
    setCursor(560, 15);
    writeString("FA22 ECE4760");
    setCursor(500, 30);
    writeString("Bianca, Crystal, Rosie");

    setTextSize(1) ;
    setCursor(35, 230-78);
    writeString("Output");
    setCursor(35, 230-78+15);
    setTextColor(RED);
    writeString("Right");
    setTextColor(GREEN);
    setCursor(35, 230-78+30);
    writeString(" Left");
    setTextColor(WHITE);
    setCursor(50, 430-78);
    writeString("FFT");
    setCursor(30, 430-78+15);
    setTextColor(MAGENTA);
    writeString(" Zoom x1");
    setTextColor(BLUE);
    setCursor(30, 430-78+30);
    writeString("Zoom x10");

    setTextColor(WHITE);

    // Draw plot lines 
    draw_plot_lines(230, "-2048", "+2048"); //top: DAC output
    draw_plot_lines(430, "0", "150"); //bottom: FFT

    // Grid markings for bottom plot
    for(int i = 1; i<609-81; i++){
        if(i%100==0) drawVLine(80+i, 426, 7, CYAN);
        else if(i%10==0) drawVLine(80+i, 428, 3, CYAN);
    }
    drawHLine(81, 430, 609-81, CYAN);

    while (true) {
        // Wait on semaphore
        PT_SEM_WAIT(pt, &vga_semaphore);

        // Increment drawspeed controller
        throttle += 1 ;

        // If the controller has exceeded a threshold, draw
        if(throttle >= 10){
            // Zero drawspeed controller
            throttle = 0 ;

            // Uncomment for Serial parsing debugging
            // setCursor(250, 15);
            // fillRect(250, 10, 400, 13, BLACK);
            // writeString("Serial: ");
            // writeString(serial_input);
            // vga_print_int(25, 15, "x: ", source_x, 5);
            // vga_print_int(75, 15, "y: ", source_y, 5);
            // vga_print_int(125, 15, "v: ", volume, 4);
            // vga_print_int(175, 15, "a: ", source_angle, 4);

            fillRect(30, 43, 220, 13, BLACK);
            vga_print_int(30, 45, "L Amp: ", (int) left_amp, 5);
            vga_print_int(115, 45, "R Amp: ", (int) right_amp, 5);

            fillRect(30, 58, 220, 13, BLACK);
            vga_print_int(30, 60, "L Delay: ", left_delay, 7);
            vga_print_int(115, 60, "R Delay: ", right_delay, 7);

            setCursor(250, 45);
            fillRect(250, 42, 200, 13, BLACK);
            if(filtering){
                setTextColor(GREEN);
                writeString("Filtering");
            }
            else{
                setTextColor(RED);
                writeString("No Filter");
            }
            setTextColor(WHITE);

            for(int i = 0; i<511; i++){
                //Top Graph: DAC output
                drawVLine(i+81, 79, 170, BLACK); //erase column
                graph(i+81, 230, (float)(DAC_buffer_0[i*2]), 0, 4096, RED);
                graph(i+81, 230, (float)(DAC_buffer_0[i*2+1]), 0, 4096, RED);
                graph(i+81, 230, (float)(DAC_buffer_1[i*2]), 0, 4096, GREEN);
                graph(i+81, 230, (float)(DAC_buffer_1[i*2+1]), 0, 4096, GREEN);

                //Bottom Graph: FFT frequency spectrum after low pass (if filtering on)
                // 1022 samples to get full frequency spectrum
                // We have 2048 bins, but only look at first 1024 because of symmetry
                drawVLine(i+81, 270, 426-270, BLACK); //erase column
                float height = fix2float15(draw_buffer[i]);
                if(height<10) drawVLine(i+81, 426-(height*10.0), (height*10.0), BLUE);
                else{
                    height = min(height, 147);
                    drawVLine(i+81, 426-height, height, MAGENTA);
                }
            }
        }
    }
  PT_END(pt);
} 

void init_gpio(){
    // Initialize SPI channel (channel, baud rate set to 20MHz)
    spi_init(SPI_PORT, 20000000) ;
    // Format (channel, data bits per transfer, polarity, phase, order)
    spi_set_format(SPI_PORT, 16, 0, 0, 0);

    // Map SPI signals to GPIO ports
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_CS, GPIO_FUNC_SPI) ;

    adc_init();

    // Audio Aux Inputs
    adc_gpio_init(PIN_ADC_0); //26
    adc_gpio_init(PIN_ADC_1); //28

    // Map LDAC pin to GPIO port, hold it low (could alternatively tie to GND)
    gpio_init(LDAC) ;
    gpio_set_dir(LDAC, GPIO_OUT) ;
    gpio_put(LDAC, 0) ;

}

void core1_main(){
    // create an alarm pool on core 1
    alarm_pool_t *core1pool ;
    core1pool = alarm_pool_create(2, 16) ;

    // Create a repeating timer that calls repeating_timer_callback.
    struct repeating_timer timer_vga;

    // VGA update frequency is 1000 kHz
    alarm_pool_add_repeating_timer_us(core1pool, -1000, repeating_timer_callback_vga, NULL, &timer_vga);

    // Updates graphics 
    pt_add_thread(protothread_vga);

    // FFT thread for channel 1, on separate cores for load balancing 
    pt_add_thread(protothread_fft_1);

    pt_schedule_start;
}

void init_arrays(){
    // Initialize DAC_output and fft_out buffers to 0 because these could 
    // be read before they are written, so it is important to reset
    for(int i = 0; i<NUM_SAMPLES; i++){
        fft_out_buffer_0[i] = 0;
        fft_out_buffer_1[i] = 0;
    }

    for(int i = 0; i<NUM_SAMPLES+EXTRA; i++){
        DAC_buffer_0[i] = 0;
        DAC_buffer_1[i] = 0;
    }
}

// Core 0 entry point
int main() {
    // Initialize stdio/uart
    stdio_init_all();
    printf("Begin\n");

    // Initialize Video Graphics Array
    initVGA();

    // Initialize general purpose input output interfaces
    init_gpio();

    // Initialize DAC_output and fft_out buffers
    init_arrays();

    // Initializes the Sine lookup table used in FFT algorithm
    init_fft();

    // Load the low pass filter values (from 1 to 0)
    for(int i = 0; i<FILT_SIZE; i++){
        float v = ((float)(i)) / ((float)(FILT_SIZE));
        filter[i] = float2fix15(1.0 - v*v);
    }

    // Create repeating timer that calls repeating_timer_callback (default core 0)
    struct repeating_timer timer_core_0;

    // Negative delay means we will call repeating_timer_callback, and call it
    // again 45us (20kHz kHz) later regardless of how long the callback took to execute
    add_repeating_timer_us(-80, repeating_timer_callback_core_0, NULL, &timer_core_0);

    multicore_reset_core1();
    multicore_launch_core1(core1_main);

    // Reads serial messages from python GUI and parses them
    pt_add_thread(protothread_serial);

    // FFT thread for channel 0
    pt_add_thread(protothread_fft_0);

    pt_schedule_start ;
}