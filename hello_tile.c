//#include <sys/types.h>
//#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <libgte.h>
#include <libetc.h>
#include <libgpu.h>
#include <libapi.h>

#include "crc.h"

#define VMODE 0         // Video Mode : 0 : NTSC, 1: PAL

#define SCREENXRES 320
#define SCREENYRES 240

#define MARGINX 0            // margins for text display
#define MARGINY 32

#define FONTSIZE 8 * 7       // Text Field Height

// data dumper settings
#define BLOCKSIZE 4
#define STARTX 5
#define STARTY 26
#define MAXENDX   315
#define MAXENDY   230
// end data dumper settings
// DO NOT CHANGE DIRECTLY
#define PIXW (((MAXENDX-STARTX)/BLOCKSIZE)*BLOCKSIZE)
#define PIXH (((MAXENDY-STARTY) / BLOCKSIZE)*BLOCKSIZE)
#define ENDX (STARTX + PIXW)
#define ENDY (STARTY + PIXH)

#define USTARTX (STARTX+BLOCKSIZE)
#define USTARTY (STARTY+BLOCKSIZE)
#define UENDX (ENDX-BLOCKSIZE)
#define UENDY (ENDY-BLOCKSIZE)
#define UCNTW ((UENDX - USTARTX)/BLOCKSIZE)
#define UCNTH ((UENDY - USTARTY)/BLOCKSIZE)
#define UCNT (UCNTW * UCNTH)
// END DO NOT CHANGE DIRECTLY

#define OTLEN (4 + UCNT)              // Ordering Table Length 

DISPENV disp[2];             // Double buffered DISPENV and DRAWENV
DRAWENV draw[2];

u_long ot[2][OTLEN];          // double ordering table of length 8 * 32 = 256 bits / 32 bytes

short db = 0;                 // index of which buffer is used, values 0, 1

TILE dataframe[4];
TILE datablocks[2][UCNT];

void init(void)
{
    ResetGraph(0);
    
    SetDefDispEnv(&disp[0], 0, 0, SCREENXRES, SCREENYRES);
    SetDefDispEnv(&disp[1], 0, SCREENYRES, SCREENXRES, SCREENYRES);
    
    SetDefDrawEnv(&draw[0], 0, SCREENYRES, SCREENXRES, SCREENYRES);
    SetDefDrawEnv(&draw[1], 0, 0, SCREENXRES, SCREENYRES);
    
    if (VMODE)
    {
        SetVideoMode(MODE_PAL);
        disp[0].screen.y += 8;
        disp[1].screen.y += 8;
    }
        
    setRGB0(&draw[0], 255, 255, 255);
    setRGB0(&draw[1], 255, 255, 255);
    
    draw[0].isbg = 1;
    draw[1].isbg = 1;
    
    SetDispMask(1);

    PutDispEnv(&disp[db]);
    PutDrawEnv(&draw[db]);
    
    FntLoad(960, 0);
    static RECT rect;
    setRECT(&rect, 960, 128, 2, 1);
    static uint32_t thedata[16];
    thedata[0] = 0x7F200000;
    LoadImage(&rect, &thedata);
    FntOpen(MARGINX, SCREENYRES - MARGINY - FONTSIZE, SCREENXRES - MARGINX * 2, FONTSIZE, 0, 280 );    
}

/*
void draw_data()
{
    TILE *tile = &tilebuff[db];
    ClearOTagR(ot[db], OTLEN);
    setTile(tile);                              // initialize the blue_tile structure ( fill the length and tag(?) value )
	setXY0(tile, 5, 26);   // Set X,Y
	setWH(tile, 4, 4);  
    if(db == 0)
    {
        setRGB0(tile, 60, 180, 255);                // Set color
    }
    else
    {
        setRGB0(tile, 255, 32, 255);
    }
    addPrim(ot[db], tile); 
    DrawOTag(ot[db] + OTLEN - 1);    
}
*/

void display_data()
{
    db = !db;
    DrawSync(0);
    VSync(0);        
    PutDispEnv(&disp[db]);
    PutDrawEnv(&draw[db]);
}

u_char padbuff[2][34];

typedef struct _PADTYPE
{
    unsigned char	stat;
    unsigned char	len:4;
    unsigned char	type:4;
    unsigned short	btn;
    unsigned char	rs_x,rs_y;
    unsigned char	ls_x,ls_y;
} PADTYPE;

typedef enum {
    BTN_TRIANGLE = 4096,
    BTN_CIRCLE   = 8192,
    BTN_CROSS    = 16384
} BTN;

void wait_for_pad(const BTN btn)
{
    // controller on port 1 connected
    volatile PADTYPE *pad = (PADTYPE *)padbuff[0];
    while(1)
    {
            
        if( pad->stat == 0 )
        {
            if( ( pad->type == 0x4 ) || 
                ( pad->type == 0x5 ) || 
                ( pad->type == 0x7 ) ) {
                if(!(pad->btn & btn)) {
                    break;
                }
            }                                        
        }            
    }
}

void delay_ds(uint32_t deciseconds) {
	uint32_t frames = deciseconds * 6;
	while (frames) {
		VSync(0);
		frames--;
	}
}

void output_status(const char *message) {
    FntPrint(message);
    FntFlush(-1);
    display_data();
}

void draw_dataframe(void)
{
    ClearOTagR(ot[db], OTLEN);
    addPrim(ot[db], &dataframe[0]);
    addPrim(ot[db], &dataframe[1]);
    addPrim(ot[db], &dataframe[2]);
    addPrim(ot[db], &dataframe[3]);
    
    for(int j = 0; j < UCNT; j++)
    {
        addPrim(ot[db], &datablocks[db][j]);
    }
    DrawOTag(ot[db] + OTLEN - 1);
}

void set_byte(uint8_t value, int pos)
{
    for(uint8_t bit = 0; bit < 8; bit++) {        
        if((value >> bit) & 1)
        {
            setRGB0(&datablocks[db][pos], 0, 0, 0);
        }
        else
        {
            setRGB0(&datablocks[db][pos], 255, 255, 255);
        }
        pos++;       
    }
}

void dump_data(const void *data, size_t size)
{
    // setup frame
    //  top
    setTile(&dataframe[0]);
    setXY0(&dataframe[0], STARTX, STARTY);
    setWH(&dataframe[0], PIXW, BLOCKSIZE);
    setRGB0(&dataframe[0], 0, 0, 0);

    // bottom
    setTile(&dataframe[1]);
    setXY0(&dataframe[1], STARTX, ENDY-BLOCKSIZE);
    setWH(&dataframe[1], PIXW, BLOCKSIZE);
    setRGB0(&dataframe[1], 0, 0, 0);

    // left
    setTile(&dataframe[2]);
    setXY0(&dataframe[2], STARTX, STARTY+BLOCKSIZE);
    setWH(&dataframe[2], BLOCKSIZE, PIXH-(2*BLOCKSIZE));
    setRGB0(&dataframe[2], 0, 0, 0);

    // right
    setTile(&dataframe[3]);
    setXY0(&dataframe[3], ENDX-BLOCKSIZE, STARTY+BLOCKSIZE);
    setWH(&dataframe[3], BLOCKSIZE, PIXH-(2*BLOCKSIZE));
    setRGB0(&dataframe[3], 0, 0, 0);

    // setup tiles
    for(int i = 0; i < 2; i++)
    {
        int x = USTARTX;
        int y = USTARTY;
        for(int j = 0; j < UCNT; j++)
        {
            setTile(&datablocks[i][j]);
            setXY0(&datablocks[i][j], x, y);
            setWH(&datablocks[i][j], BLOCKSIZE, BLOCKSIZE);
            setRGB0(&datablocks[i][j], 255, 255, 255);
            x += BLOCKSIZE;
            if(x == UENDX)
            {
                x = USTARTX;
                y += BLOCKSIZE;
            }
        }
    }

    // draw and display 
    draw_dataframe();
    display_data();
    
    //const uint8_t *buf = (const uint8_t*)(data);
    // current frame index, last frame index, data size in bytes, checksum
    const uint16_t nondatasize = sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint32_t); 
    const uint16_t framedatasize = (UCNT/8) - nondatasize;
    uint16_t fullframecnt = (size / framedatasize);
    if(size % framedatasize) {
        fullframecnt += 1;
    }
    uint16_t lastframeindex = fullframecnt-1;
     const uint8_t *buf = ((const uint8_t*)(data));
    for(uint16_t frameindex = 0; frameindex <= lastframeindex; frameindex++) {
        const uint16_t datanow = (frameindex == lastframeindex) ? size : framedatasize;              

        // this can be an unaligned load, is this a problem on real hw?
        const uint32_t checksum = crc32_frame(frameindex, lastframeindex, datanow, buf);

        //printf("dumping frame %u of %u size %u chk 0x%X\n", frameindex, lastframeindex, datanow, checksum);        
        
        int bitindex = 0;
        // write the frame index in LE
        set_byte(frameindex, bitindex);
        bitindex += 8;
        set_byte(frameindex >> 8, bitindex);
        bitindex += 8;

        // write the last frameindex in LE
        set_byte(lastframeindex, bitindex);
        bitindex += 8;
        set_byte(lastframeindex >> 8, bitindex);
        bitindex += 8;

        // write the data size in LE
        set_byte(datanow, bitindex);
        bitindex += 8;
        set_byte(datanow >> 8, bitindex);
        bitindex += 8;
        
        // write the data        
        for(int i = 0; i < datanow; i++) {
            set_byte(buf[i], bitindex);
            bitindex += 8;
        }        

        // write checksum in LE
        set_byte((uint8_t)(checksum), UCNT-32);
        set_byte((uint8_t)(checksum >> 8), UCNT-24);
        set_byte((uint8_t)(checksum >> 16), UCNT-16);
        set_byte((uint8_t)(checksum >> 24), UCNT-8);

        draw_dataframe();
        display_data();      
        delay_ds(5);

        size -= datanow;  
        buf += framedatasize;
    }    
}

void dump_save(const char *savename) {
    output_status("Opening file");
    int32_t fd = open((char*)savename, 0x1);
    if(fd < 0) {
        output_status("Failed to open save");
        return;
    }
    delay_ds(1);
    
    output_status("Reading from file");
    uint8_t buf[0x2000];
    int32_t res = read(fd, buf, sizeof(buf));
    if(res != (int32_t)sizeof(buf)) {
        output_status("File read failed");
        close(fd);        
        return;
    }
    close(fd);
    output_status("File read success");
    dump_data(buf, sizeof(buf));
    while(1);
}

int main(void)
{   
    init();
    InitPAD( padbuff[0], 34, padbuff[1], 34 );
    padbuff[0][0] = padbuff[0][1] = 0xff;
    padbuff[1][0] = padbuff[1][1] = 0xff;
    StartPAD();

    while(1)
    {
        //draw_data();
        FntPrint("Press X to start");
        FntFlush(-1);
        display_data();
        wait_for_pad(BTN_CROSS);
        InitCARD(1);
        StartCARD();
        _bu_init();
        dump_save("bu00:BASLUS-01384DRACULA");
    }

    return 0;
}
