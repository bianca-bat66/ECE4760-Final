#include <string.h>
#include <stdio.h>
#include "vga_graphics.h"
#include "vga_graphing.h"

void draw_plot_lines(int y, char min[], char max[]){
    char screentext[40];
    drawHLine(75, y, 5, CYAN) ;
    sprintf(screentext, min) ;
    setCursor(70-5*strlen(min), y-5) ;
    writeString(screentext) ;
    
    drawHLine(75, y-75, 5, CYAN) ;
    
    drawHLine(75, y-150, 5, CYAN) ;
    sprintf(screentext, max) ;
    setCursor(70-5*strlen(max), y-150) ;
    writeString(screentext) ;

    drawVLine(80, y-150, 150, CYAN) ;
}

void graph(int x, int y, float value, int min, int range, char color){
    drawPixel(x, y - (int)(150 * ((float)(value-min) / range)), color);
}

void graph_line(int x, int y, float v1, float v2, int min, int range, char color){
    if(v1==v2) graph(x, y, v1, min, range, color);
    float low = (v1<v2)?v1:v2;
    float max = (v1>v2)?v1:v2;
    int base = y - (int) (150 * (low/range));
    int height = 150 * ((max-low)/range);
    drawVLine(x, base, height, color);
}

//amplitude (modulated) values [0, 1]
void graph_amp(int x, int y, float entry, char color){
    graph(x, y, (float)(entry), 0, 1, color);
}

//dac output value [-2048, 2048]
void graph_dac(int x, int y, int entry, char color){
    graph(x, y, (float)(entry), -2048, 4096, color);
}