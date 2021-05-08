/*

MIT License

Copyright (c) 2021 Gavin Hayes and other psx_screen_dumper authors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

//#include <sys/types.h>
//#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <libgte.h>
#include <libetc.h>
#include <libgpu.h>
#include <libapi.h>

#include "crc.h"

#define SCREENXRES 320
#define SCREENYRES 240

// data dumper settings
#define BLOCKSIZE 4
#define STARTX 5
#define STARTY 26
#define MAXENDX   315
#define MAXENDY   230
// end data dumper settings

#define MARGINX 0            // margins for text display
#define MARGINY 32
#define FONTSIZE 8 * 7       // Text Field Height

#define CHAR_HEIGHT 15
#define CHAR_WIDTH 8
#define FONT_X (SCREENXRES)
#define CLUT_X (FONT_X)
#define CLUT_Y (6 * CHAR_HEIGHT)
#define FONT_COLS 16
#define FONT_ROWS 6
#define FONT_TEXTURES 1


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

#define FRAME_HEADER_SIZE (sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint16_t))
#define FRAME_FOOTER_SIZE (sizeof(uint32_t))
#define FRAME_DATA_SIZE   ((UCNT/8) - (FRAME_HEADER_SIZE + FRAME_FOOTER_SIZE))
// END DO NOT CHANGE DIRECTLY

TILE dataframe[4];
TILE datablocks[2][UCNT];
    
DR_TPAGE fonttpage[2];
#define FONT_SPRT_CNT ((SCREENXRES/CHAR_WIDTH)*(SCREENYRES/CHAR_HEIGHT))
SPRT font[2][FONT_SPRT_CNT];
SPRT *fontSPRT;
#define FONT_OTLEN (1 + FONT_SPRT_CNT)

#define MENU_OTLEN 2
TILE menu_selected[MENU_OTLEN];


#define OTLEN (4 + UCNT+ FONT_OTLEN + MENU_OTLEN)              // Ordering Table Length 

DISPENV disp[2];             // Double buffered DISPENV and DRAWENV
DRAWENV draw[2];

u_long ot[2][OTLEN];          // double ordering table of length 8 * 32 = 256 bits / 32 bytes
short db = 0;                 // index of which buffer is used, values 0, 1



void decompressfont() {
	// Font is 1bpp. We have to convert each character to 4bpp.
	const uint8_t * rom_charset = (const uint8_t *) 0xBFC7F8DE;
	uint8_t charbuf[CHAR_HEIGHT * CHAR_WIDTH / 2];
    RECT currentrect;

	// Iterate through the 16x6 character table
	for (uint_fast8_t tabley = 0; tabley < 6; tabley++) {
		for (uint_fast8_t tablex = 0; tablex < 16; tablex++) {
			uint8_t * bufferpos = charbuf;

			// Iterate through each line of the 8x15 character
			for (uint_fast8_t chary = 0; chary < 15; chary++) {
				uint_fast8_t char1bpp = *rom_charset;
				uint_fast8_t bpos = 0;
				rom_charset++;

				// Iterate through each column of the character
				while (bpos < 8) {
					uint_fast8_t char4bpp = 0;

					if (char1bpp & 0x80) {
						char4bpp |= 0x0F;
					}
					bpos++;

					if (char1bpp & 0x40) {
						char4bpp |= 0xF0;
					}
					bpos++;

					*bufferpos = char4bpp;
					bufferpos++;
					char1bpp = char1bpp << 2;
				}
			}

			// GPU_dw takes units in 16bpp units, so we have to scale by 1/4
            setRECT(&currentrect, FONT_X + tablex * CHAR_WIDTH * 4 / 16, tabley * CHAR_HEIGHT, CHAR_WIDTH * 4 / 16, CHAR_HEIGHT);
            LoadImage(&currentrect, (u_long*)charbuf);
		}
	}
}

void dumpfont(void) {
    for (uint_fast8_t tabley = 0; tabley < 6; tabley++) {
		for (uint_fast8_t tablex = 0; tablex < 16; tablex++) {     
            SPRT *sprt = fontSPRT;       
            //SPRT *sprt = &font[db][tablex][tabley];
            setSprt(sprt);
            setXY0(sprt, 10 + (tablex * 10), 10 + (tabley*17));
            setWH(sprt, CHAR_WIDTH, CHAR_HEIGHT);
            // set position in texture
            const uint16_t uoffs =  tablex * CHAR_WIDTH;
            const uint16_t voffs =  tabley * CHAR_HEIGHT;
            //printf("uoffs %u voffs %u\n", uoffs, voffs);
            setUV0(sprt, uoffs, voffs);
            setClut(sprt, CLUT_X, CLUT_Y);
            setRGB0(sprt, 128, 128, 128);
            addPrim(ot[db], sprt);
            fontSPRT++;
        }
    }    
}

void print_text_at(const char *text, int16_t x, int16_t y, bool overstrike)
{
    while (*text != '\0') {
		int tex_idx = *text - '!';
		if (tex_idx >= 0 && tex_idx < 96) {
			// Font has a yen symbol where the \ should be
			if (tex_idx == '\\' - '!') {
				tex_idx = 95;
			}
    
            setSprt(fontSPRT);
            setXY0(fontSPRT, x, y);
            setWH(fontSPRT, CHAR_WIDTH, CHAR_HEIGHT);
            // set position in texture
            const uint16_t uoffs =  (tex_idx % 16) * CHAR_WIDTH;
            const uint16_t voffs =  (tex_idx / 16) * CHAR_HEIGHT;
            setUV0(fontSPRT, uoffs, voffs);
            setClut(fontSPRT, CLUT_X, CLUT_Y);
            setRGB0(fontSPRT, 128, 128, 128);
            addPrim(ot[db], fontSPRT);
            fontSPRT++;

            if(overstrike)
            {
                *fontSPRT = *(fontSPRT-1);
                setXY0(fontSPRT, x+1, y);
                addPrim(ot[db], fontSPRT);
                fontSPRT++;
            }
		}

		x += 10;
		text++;
	}    
}

void init(void)
{
    ResetGraph(0);
    
    SetDefDispEnv(&disp[0], 0, 0, SCREENXRES, SCREENYRES);
    SetDefDispEnv(&disp[1], 0, SCREENYRES, SCREENXRES, SCREENYRES);
    
    SetDefDrawEnv(&draw[0], 0, SCREENYRES, SCREENXRES, SCREENYRES);
    SetDefDrawEnv(&draw[1], 0, 0, SCREENXRES, SCREENYRES);
    
    if (0)
    {
        SetVideoMode(MODE_PAL);
        disp[0].screen.y += 8;
        disp[1].screen.y += 8;
    }
        
    setRGB0(&draw[0], 50, 50, 50);
    setRGB0(&draw[1], 50, 50, 50);
    
    draw[0].isbg = 1;
    draw[1].isbg = 1;
    
    SetDispMask(1);

    PutDispEnv(&disp[db]);
    PutDrawEnv(&draw[db]);
    
    FntLoad(960, 0);
    /*static RECT rect;
    setRECT(&rect, 960, 128, 2, 1);
    static uint32_t thedata;
    thedata = 0x7F200000;
    LoadImage(&rect, &thedata);*/
    FntOpen(MARGINX, SCREENYRES - MARGINY - FONTSIZE, SCREENXRES - MARGINX * 2, FONTSIZE, 0, 280 );

    // load the font from bios
    decompressfont();
    RECT currentrect;
    setRECT(&currentrect, CLUT_X, CLUT_Y, 16, 1);
    static const uint16_t PALETTE[16] = { 0x0000, 0x0842, 0x1084, 0x18C6, 0x2108, 0x294A, 0x318C, 0x39CE, 0x4631, 0x4E73, 0x56B5, 0x5EF7, 0x6739, 0x6F7B, 0x77BD, 0x7FFF };
    LoadImage(&currentrect, (u_long*)&PALETTE);
    const uint16_t tpage = getTPage(0, 0, FONT_X, 0);
    setDrawTPage(&fonttpage[0], 0, 0, tpage);
    setDrawTPage(&fonttpage[1], 0, 0, tpage);

    setTile(&menu_selected[0]);
    setRGB0(&menu_selected[0], 0, 0, 0);
    setTile(&menu_selected[1]);
    setRGB0(&menu_selected[1], 0, 0, 0);

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

PADTYPE lastpad; 

typedef enum {
    BTN_UP       = 16,
    BTN_DOWN     = 64,
    BTN_TRIANGLE = 4096,
    BTN_CIRCLE   = 8192,
    BTN_CROSS    = 16384
} BTN;

/* all passed in must be pressed for true */
uint16_t buttons_pressed(const BTN btn)
{
    // controller on port 1 connected
    volatile PADTYPE *pad = (PADTYPE *)padbuff[0];
    if( pad->stat == 0 )
    {
        if( ( pad->type == 0x4 ) || 
            ( pad->type == 0x5 ) || 
            ( pad->type == 0x7 ) ) {
            return (!(pad->btn & btn));            
        }                                        
    }
    return 0;
}

uint16_t new_buttons_pressed(const BTN btn)
{
    // if not currently pressed ignore
    if(!buttons_pressed(btn)) return 0;

    if( lastpad.stat == 0 )
    {
        // At least one must not have been pressed
        if( ( lastpad.type == 0x4 ) || 
            ( lastpad.type == 0x5 ) || 
            ( lastpad.type == 0x7 ) ) {
            return (lastpad.btn & btn);            
        }                                        
    }
    return 0;
}

void output_status(const char *message) {
    /*FntPrint(message);
    FntFlush(-1);*/
    print_text_at(message, 20, SCREENYRES-30, false);
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

typedef enum {
    SPT_SELECT_DEVICE,
    SPT_MCS_LIST,
    SPT_DUMP,
    SPT_DBG_FONT,
    SPT_CREDITS
} SCREEN_PAGE_TYPE;

typedef struct {
    const char *devnumber;
    const char *filename;
    size_t filesize;    
} MCS_LIST_ITEM_EXTRADATA;

typedef struct SCREEN_PAGE SCREEN_PAGE;
typedef struct {
    SCREEN_PAGE *changeto;
    union {
        // mcs list extradata
        struct {
            const char *devnumber;
            const char *printdev;
        };
        // dump buf extradata
        struct {
            const uint8_t *buf;
            const size_t bufsize;
        };
    };
    
} SELECT_DEVICE_ITEM_EXTRADATA;

typedef struct {
    char label[20];
    union {
        SELECT_DEVICE_ITEM_EXTRADATA selectdevice;
        MCS_LIST_ITEM_EXTRADATA mcslist;
    };
} MENUITEM;

typedef struct {
    const char *devnumber;
} MENU_DIRECTORY_EXTRADATA;

typedef struct {
    uint16_t count;
    uint16_t index;
    void(*handle)(void);
    union {
        MENU_DIRECTORY_EXTRADATA mde;
    };
    MENUITEM items[15];
    char title[24];
    const char *status;
} MENU;


typedef struct {    
    char name[24];
    const char *statustext;    
    const uint8_t *buf;
    size_t bufsize;
    const uint8_t *readhead;
    const uint8_t *readend;
    uint32_t read_bytes_left; // number of bytes available to be fetched
    int sleep_frames;
    uint16_t frameindex;
    uint16_t framecount;
    uint32_t crc32;

    uint16_t framesize;
    uint8_t startdata[6];
    uint8_t enddata[4];
    const uint8_t *printhead;
    
    // used only for file read dump
    int fd;    
    char filename[25];
    
} DUMP;

typedef struct SCREEN_PAGE{
    SCREEN_PAGE_TYPE current;
    void (*init)(const void*);
    void (*draw)(void);
    void (*on_vsync)(void);
    struct SCREEN_PAGE *back;
    void (*exit)(void);

    union {
        MENU menu;
        DUMP dump;
    };

} SCREEN_PAGE;

typedef struct {    
    SCREEN_PAGE *curpage;

    SCREEN_PAGE page_select_device;
    SCREEN_PAGE page_mcs_list;
    SCREEN_PAGE page_dump;
    SCREEN_PAGE page_dbg_font;
    SCREEN_PAGE page_credits;

    
} SCREEN_PAGES;


SCREEN_PAGES sp;

void nop(void)
{

}

static inline void sp_set(SCREEN_PAGE *new)
{
    sp.curpage = new;
}

void sp_init_page(SCREEN_PAGE *new)
{
    new->back = sp.curpage;
    sp_set(new);
}

void sp_exit(void)
{
    sp_set(sp.curpage->back);    
}

static inline MENUITEM *menu_add_item(MENU *menu, const char *label)
{    
    if(menu->count >= 15)
    {
        static MENUITEM throwaway;
        return &throwaway;
    }
    strcpy(menu->items[menu->count].label, label);
    return &menu->items[menu->count++];
}

static inline void menu_select_device_add_item(MENU *menu, const char *label, const SELECT_DEVICE_ITEM_EXTRADATA *extradata)
{
    memcpy(&(menu_add_item(menu, label))->selectdevice, extradata, sizeof(*extradata));
}

static inline void menu_mcs_list_add_item(MENU *menu, const char *label, const MCS_LIST_ITEM_EXTRADATA *extradata)
{
    MENUITEM *item = menu_add_item(menu, label);
    memcpy(&item->mcslist, extradata, sizeof(*extradata));
    item->mcslist.filename = item->label;
}

void menu_draw(void)
{
    MENU *menu = &sp.curpage->menu;
    // print title
    const char *titletext = menu->title;
    uint16_t centerx = (SCREENXRES / 2)-((CHAR_WIDTH+1));
    uint16_t xtitle = centerx - ((strlen(titletext)/2)*(CHAR_WIDTH+1));
    print_text_at(titletext, xtitle, 20, true);

    // draw the menu items
    uint16_t x = 20;
    uint16_t centery = (SCREENYRES / 2)-(CHAR_HEIGHT/2);
    uint16_t y = centery - ((menu->count/2) * 20);    
    for(int i = 0; i < menu->count; i++)
    {        
        print_text_at(menu->items[i].label, x, y, true);
        if(i == menu->index)
        {
            setXY0(&menu_selected[db], 0, y-2);
            setWH(&menu_selected[db], SCREENXRES, 20);
            addPrim(ot[db], &menu_selected[db]);
        }
        y += 20;
    }

    if(menu->status != NULL) {
        output_status(menu->status);
    }
}

void menu_init(void) {
    MENU *menu = &sp.curpage->menu;
    menu->count = 0;
    menu->index = 0;
}

void menu_exit(void)
{
    MENU *menu = &sp.curpage->menu;
    if(sp.curpage->back != NULL)
    {
        menu->status = NULL;
        sp_exit();
    }    
}

void menu_on_vsync(void)
{
    MENU *menu = &sp.curpage->menu;
    if(new_buttons_pressed(BTN_CROSS))
    {
        menu->status = NULL;
        if(menu->index < menu->count)
        {
            menu->handle();
            return;
        }        
    }
    else if(new_buttons_pressed(BTN_TRIANGLE))
    {        
        sp.curpage->exit();
        return;
    }
    else if(new_buttons_pressed(BTN_UP))
    {
        if(menu->index > 0)
        {
            menu->index--;           
        }
    }
    else if(new_buttons_pressed(BTN_DOWN))
    {
        if(menu->index < (menu->count-1))
        {
            menu->index++;            
        }
    }   
}

void dump_buf_init(const void *param);
void mcs_list_init(const void *param);
void select_device_handle(void)
{
    const SELECT_DEVICE_ITEM_EXTRADATA *sditem = &sp.page_select_device.menu.items[sp.page_select_device.menu.index].selectdevice;
    sditem->changeto->init(sditem);  
}

void dump_encode_frame(DUMP *dump, const uint8_t *framebuf, const const uint16_t framesize)
{
    // encode frameindex
    dump->startdata[0] = dump->frameindex;
    dump->startdata[1] = dump->frameindex >> 8;

    // encode size
    dump->startdata[4] = framesize;
    dump->startdata[5] = framesize >> 8;
    dump->framesize = framesize;

    // encode crc32
    const uint32_t checksum = crc32_frame(dump->startdata, FRAME_HEADER_SIZE, framebuf, framesize);
    dump->enddata[0] = (uint8_t)(checksum);
    dump->enddata[1] = (uint8_t)(checksum >> 8);
    dump->enddata[2] = (uint8_t)(checksum >> 16);
    dump->enddata[3] = (uint8_t)(checksum >> 24);
    
    // display for half a second
    dump->printhead = framebuf;
    dump->sleep_frames = 30;
    dump->frameindex++;    
}

void dump_frame(void)
{
    // abort if back button is pressed
    if(new_buttons_pressed(BTN_TRIANGLE))
    {
        goto dump_frame_cleanup;
    }

    DUMP *dump = &sp.page_dump.dump;
    if(dump->sleep_frames == 0) {        
        // all bytes in buf copied, need to read
        if(dump->readhead == dump->readend)
        {
            if(dump->read_bytes_left == 0)
            {
                if(dump->frameindex == dump->framecount)
                {
                    goto dump_frame_cleanup;
                }
                else
                {
                    // encode the crc32
                    dump->crc32 = ~dump->crc32;
                    dump_encode_frame(dump, (const uint8_t*)&dump->crc32, sizeof(uint32_t));
                    return;
                }                
            }
            size_t toread = (dump->read_bytes_left > dump->bufsize) ? dump->bufsize : dump->read_bytes_left;
            int32_t res = read(dump->fd, (void*)dump->buf, toread);
            if(res != toread) {
                printf("File read failed\n");
                goto dump_frame_cleanup;
            }
            dump->read_bytes_left -= toread;
            dump->readhead = dump->buf;
            dump->readend = dump->readhead +  toread;
        }        

        // encode frame
        const size_t bufleft = dump->readend - dump->readhead; 
        const uint16_t thisframesize = (bufleft > FRAME_DATA_SIZE) ? FRAME_DATA_SIZE : bufleft;
        dump_encode_frame(dump, dump->readhead, thisframesize);

        // update running crc32
        dump->crc32 = crc32_inner(dump->crc32, dump->readhead, thisframesize);

        // advance readhead       
        dump->readhead += thisframesize;        
    }   

    dump->sleep_frames--;
    return;

    dump_frame_cleanup:
    sp.page_dump.exit();
}

void dump_draw(void)
{
    DUMP *dump = &sp.page_dump.dump;
    // calculate the blocks
    int bitindex = 0;
    // start data
    for(int i = 0; i < 6; i++)
    {
        set_byte(dump->startdata[i], bitindex);
        bitindex += 8;
    }
    // actual data
    for(int i = 0; i < dump->framesize; i++) {
        set_byte(dump->printhead[i], bitindex);
        bitindex += 8;
    }
    // end data
    bitindex = UCNT-32;
    for(int i = 0; i < 4; i++) {
        set_byte(dump->enddata[i], bitindex);
        bitindex += 8;
    }

    // draw a frame of data    
    addPrim(ot[db], &dataframe[0]);
    addPrim(ot[db], &dataframe[1]);
    addPrim(ot[db], &dataframe[2]);
    addPrim(ot[db], &dataframe[3]);

    for(int j = 0; j < UCNT; j++)
    {
        addPrim(ot[db], &datablocks[db][j]);
    }
}

void dump_start(DUMP *dump, uint16_t fullframecnt)
{
    fullframecnt += 2;
    dump->framecount = fullframecnt;
    const uint16_t lastframeindex = fullframecnt - 1;
    dump->frameindex = 0;
    dump->startdata[2] = lastframeindex;
    dump->startdata[3] = lastframeindex >> 8;

    setRGB0(&draw[0], 255, 255, 255);
    setRGB0(&draw[1], 255, 255, 255);
    sp.page_dump.draw = &dump_draw;
    sp.page_dump.on_vsync = &dump_frame;
    dump_encode_frame(dump, dump->name, strlen(dump->name));
    dump->sleep_frames--;

    dump->crc32 = 0xFFFFFFFF;
}

void dump_exit(void)
{
    setRGB0(&draw[0], 50, 50, 50);
    setRGB0(&draw[1], 50, 50, 50);
    sp_exit();
}

void dump_status_draw(void)
{
    DUMP *dump = &sp.page_dump.dump;
    if(dump->statustext != NULL)
    {
        output_status(dump->statustext);
    }    
}

static inline void dump_init(void)
{
    DUMP *dump = &sp.page_dump.dump;
    sp.page_dump.exit = &dump_exit;
    dump->statustext = "";
    sp.page_dump.draw = &dump_status_draw;
}

static uint8_t Dump_file_buf[0x2000];
// read_bytes_left and fd must be set
void dump_file_start(void)
{
    DUMP *dump = &sp.page_dump.dump;
     
    dump->buf = (const uint8_t*)&Dump_file_buf;
    dump->bufsize = sizeof(Dump_file_buf);
    dump->readhead = dump->buf;
    dump->readend = dump->readhead;

    const size_t size = dump->read_bytes_left;
    if(size == 0) {
        goto dump_file_start_cleanup;
    }

    const uint16_t fullreads = size/dump->bufsize;
    const uint16_t frames_per_read = (dump->bufsize + FRAME_DATA_SIZE - 1) / FRAME_DATA_SIZE;
    const uint16_t leftoverbytes = (size - (fullreads * dump->bufsize));
    const uint16_t last_read_frames = (leftoverbytes + FRAME_DATA_SIZE - 1) / FRAME_DATA_SIZE;
    const uint16_t fullframecnt = (fullreads * frames_per_read) + last_read_frames;
    dump_start(dump, fullframecnt);
    return;

    dump_file_start_cleanup:
    sp.page_dump.exit();
}

void dump_file_sleep2(void)
{
    sp.page_dump.on_vsync = &dump_file_start;
}

void dump_file_sleep(void)
{
    DUMP *dump = &sp.page_dump.dump;
    dump->sleep_frames--;
    if(dump->sleep_frames == 0)
    {
        dump->statustext = "Starting file dump";
        sp.page_dump.on_vsync = &dump_file_sleep2;
    }    
}

void dump_file_exit(void)
{
    DUMP *dump = &sp.page_dump.dump;
    close(dump->fd);
    dump_exit();
}

void dump_file_open(void)
{
    DUMP *dump = &sp.page_dump.dump;    
    dump->fd = open(dump->filename, 0x1);
    if(dump->fd < 0) {
        printf("Failed to open file\n");
        sp.page_dump.exit();
        return;
    }
    sp.page_dump.exit = &dump_file_exit;
    dump->sleep_frames = 6;
    sp.page_dump.on_vsync = &dump_file_sleep;
}

void dump_file_init(const void *param) {
    const MCS_LIST_ITEM_EXTRADATA *ed = param;
    sp_init_page(&sp.page_dump);
    sprintf(sp.page_dump.dump.filename, "%s%s", ed->devnumber, ed->filename);
    sp.page_dump.dump.read_bytes_left = ed->filesize;
    strcpy(sp.page_dump.dump.name, ed->filename);    
    dump_init();
    sp.page_dump.on_vsync = &dump_file_open;
    sp.page_dump.dump.statustext = "Opening file";     
}

// buf and bufsize must be set
void dump_buf_start(void)
{
    DUMP *dump = &sp.page_dump.dump;
    const size_t size = dump->bufsize;
    if(size == 0) {
        goto dump_buf_start_cleanup;
    }
    // https://stackoverflow.com/questions/17944/how-to-round-up-the-result-of-integer-division
    const uint16_t fullframecnt = (size + FRAME_DATA_SIZE - 1)/FRAME_DATA_SIZE;
    dump->readhead = dump->buf;
    dump->readend = dump->readhead + dump->bufsize;
    dump_start(dump, fullframecnt);
    return;

    dump_buf_start_cleanup:
    sp.page_dump.exit();
}

void dump_buf_init(const void *param)
{
    const SELECT_DEVICE_ITEM_EXTRADATA *sd_extradata = param;
    sp_init_page(&sp.page_dump);
    sp.page_dump.dump.buf = sd_extradata->buf;
    sp.page_dump.dump.bufsize = sd_extradata->bufsize;
    strcpy(sp.page_dump.dump.name, "PSX-BIOS.ROM");    
    dump_init();
    sp.page_dump.on_vsync = &dump_buf_start;
    sp.page_dump.dump.statustext = "Dump from buf";    
}

void mcs_list_load(void)
{
    if(new_buttons_pressed(BTN_TRIANGLE))
    {        
        sp.curpage->exit();
        return;
    }
    sp.page_mcs_list.on_vsync = &menu_on_vsync;

    InitCARD(1);
    StartCARD();
    _bu_init();

    struct DIRENTRY file;
    char tosearch[20];
    sprintf(tosearch, "%s*", sp.page_mcs_list.menu.mde.devnumber);
    printf("tosearch %s\n", tosearch);
    if(firstfile(tosearch, &file) == NULL)
    {        
        sp.page_mcs_list.menu.status = "firstfile failed";
        return;
    }
    int i = 0;
    do {
        file.name[19] = '\0';
        printf("file %s size %u\n", file.name, file.size);
        menu_mcs_list_add_item(&sp.page_mcs_list.menu, file.name, &(MCS_LIST_ITEM_EXTRADATA) {
            .devnumber = sp.page_mcs_list.menu.mde.devnumber,
            .filesize = file.size
        });
        i++;
    } while(nextfile(&file) != NULL);
    if(i == 0) {
        sp.page_mcs_list.menu.status = "No save files found!";
    }
    else {
        sp.page_mcs_list.menu.status = NULL;
    }
}

void mcs_list_preload(void)
{
    if(new_buttons_pressed(BTN_TRIANGLE))
    {        
        sp.curpage->exit();
        return;
    }   
    sp.page_mcs_list.on_vsync = &mcs_list_load;
}

void mcs_list_init(const void *param)
{
    const SELECT_DEVICE_ITEM_EXTRADATA *sditem = param;
    sp_init_page(&sp.page_mcs_list);
    sp.page_mcs_list.menu.mde.devnumber = sditem->devnumber;
    sprintf(sp.page_mcs_list.menu.title, "Dump %s saves", sditem->printdev);        
    sp.page_mcs_list.menu.status = "Reading MC files";    
    menu_init();
    sp.page_mcs_list.on_vsync = &mcs_list_preload;
}

void mcs_list_handle(void)
{
    const MENUITEM *item = &sp.page_mcs_list.menu.items[sp.page_mcs_list.menu.index];
    dump_file_init(&item->mcslist);
}

void dbg_font_init(const void *param)
{
    sp_init_page(&sp.page_dbg_font);
}

void dbg_font_on_vsync(void)
{
    if(new_buttons_pressed(BTN_CROSS) || new_buttons_pressed(BTN_TRIANGLE))
    {
        sp.curpage->exit();        
        return;
    }    
}

void dbg_font_draw(void)
{
    dumpfont();
}

void credits_init(const void *param)
{
    sp_init_page(&sp.page_credits);
}

void credits_on_vsync(void)
{
    if(new_buttons_pressed(BTN_CROSS) || new_buttons_pressed(BTN_TRIANGLE))
    {
        sp.curpage->exit();
        return;    
    }   
}

void credits_draw(void)
{
    uint16_t x = 10;
    uint16_t y = 80;
    print_text_at("G4Vi", x, y, true);
    y += 20;
    const char *socram8888 = "socram8888";
    print_text_at(socram8888, x, y, true);
    print_text_at("- BIOS charset stuff", x+(10*(strlen(socram8888)+1)), y, false);
    y += 20;
}

int main(void)
{   
    sp.page_select_device.current = SPT_SELECT_DEVICE;
    sp.page_select_device.draw = &menu_draw;
    sp.page_select_device.on_vsync = &menu_on_vsync;
    sp.page_select_device.back = NULL;
    sp.page_select_device.exit = &menu_exit;
    sp.page_select_device.menu.handle = &select_device_handle;
    strcpy(sp.page_select_device.menu.title, "PSX Screen Dumper");

    menu_select_device_add_item(&sp.page_select_device.menu, "Dump mc0 saves", &(SELECT_DEVICE_ITEM_EXTRADATA){
        .changeto = &sp.page_mcs_list,
        .devnumber = "bu00:",
        .printdev = "mc0"
    });

    menu_select_device_add_item(&sp.page_select_device.menu, "Dump mc1 saves", &(SELECT_DEVICE_ITEM_EXTRADATA){
        .changeto = &sp.page_mcs_list,
        .devnumber = "bu10:",
        .printdev = "mc1"
    });

    menu_select_device_add_item(&sp.page_select_device.menu, "Dump bios", &(SELECT_DEVICE_ITEM_EXTRADATA){
        .changeto = &sp.page_dump,
        .buf = (const uint8_t*)0xBFC00000,
        .bufsize = 0x80000
    });

    menu_select_device_add_item(&sp.page_select_device.menu, "test font", &(SELECT_DEVICE_ITEM_EXTRADATA){
        .changeto = &sp.page_dbg_font
    });

    menu_select_device_add_item(&sp.page_select_device.menu, "Credits", &(SELECT_DEVICE_ITEM_EXTRADATA){
        .changeto = &sp.page_credits
    });   

    sp.page_mcs_list.current = SPT_MCS_LIST;
    sp.page_mcs_list.init = &mcs_list_init;
    sp.page_mcs_list.exit = &menu_exit;
    sp.page_mcs_list.menu.handle = &mcs_list_handle;
    sp.page_mcs_list.menu.count = 0;
    sp.page_mcs_list.draw = &menu_draw;

    sp.page_dump.init = &dump_buf_init;
    sp.page_dump.current = SPT_DUMP;
    sp.page_dump.draw = &nop;

    sp.page_dbg_font.current = SPT_DBG_FONT;
    sp.page_dbg_font.init = &dbg_font_init;
    sp.page_dbg_font.draw = &dbg_font_draw;
    sp.page_dbg_font.on_vsync = &dbg_font_on_vsync;
    sp.page_dbg_font.exit = &sp_exit;

    sp.page_credits.current = SPT_CREDITS;
    sp.page_credits.init = &credits_init;
    sp.page_credits.draw = &credits_draw;
    sp.page_credits.on_vsync = &credits_on_vsync;
    sp.page_credits.exit = &sp_exit;


    init();
    InitPAD( padbuff[0], 34, padbuff[1], 34 );
    padbuff[0][0] = padbuff[0][1] = 0xff;
    padbuff[1][0] = padbuff[1][1] = 0xff;
    StartPAD();    

    sp.curpage = &sp.page_select_device;   
    
    while(1)
    {   
        fontSPRT = &font[db][0];    
        ClearOTagR(ot[db], OTLEN);
        sp.curpage->on_vsync();
        sp.curpage->draw();
        addPrim(ot[db], &fonttpage[db]);
        lastpad = *(PADTYPE*)padbuff[0];
        DrawSync(0);
        VSync(0);         
        PutDispEnv(&disp[db]);
        PutDrawEnv(&draw[db]);
        DrawOTag(ot[db] + OTLEN - 1);
        db = !db;        
    }

    return 0;
}
