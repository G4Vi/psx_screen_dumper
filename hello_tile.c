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

#define FRAME_HEADER_SIZE (sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint16_t))
#define FRAME_FOOTER_SIZE (sizeof(uint32_t))
#define FRAME_DATA_SIZE   ((UCNT/8) - (FRAME_HEADER_SIZE + FRAME_FOOTER_SIZE))
// END DO NOT CHANGE DIRECTLY

#define OTLEN (4 + UCNT+ (16*6)+(16*6))              // Ordering Table Length 

DISPENV disp[2];             // Double buffered DISPENV and DRAWENV
DRAWENV draw[2];

u_long ot[2][OTLEN];          // double ordering table of length 8 * 32 = 256 bits / 32 bytes

short db = 0;                 // index of which buffer is used, values 0, 1

TILE dataframe[4];
TILE datablocks[2][UCNT];
    
DR_TPAGE fonttpage[2];
SPRT font[2][16][6];

#define CHAR_HEIGHT 15
#define CHAR_VRAM_WIDTH 8

#define FONT_X (SCREENXRES)
#define CLUT_X (FONT_X)
#define CLUT_Y (6 * CHAR_HEIGHT)
#define CHAR_DRAW_WIDTH 10


void decompressfont() {
	// Font is 1bpp. We have to convert each character to 4bpp.
	const uint8_t * rom_charset = (const uint8_t *) 0xBFC7F8DE;
	uint8_t charbuf[CHAR_HEIGHT * CHAR_VRAM_WIDTH / 2];
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
			//GPU_dw(FONT_X + tablex * CHAR_VRAM_WIDTH * 4 / 16, tabley * CHAR_HEIGHT, CHAR_VRAM_WIDTH * 4 / 16, CHAR_HEIGHT, (uint16_t *) charbuf);
            setRECT(&currentrect, FONT_X + tablex * CHAR_VRAM_WIDTH * 4 / 16, tabley * CHAR_HEIGHT, CHAR_VRAM_WIDTH * 4 / 16, CHAR_HEIGHT);
            LoadImage(&currentrect, charbuf);
		}
	}
}

void dumpfont(void) {
    SPRT *sprt = &font[db][0][0];
    for (uint_fast8_t tabley = 0; tabley < 6; tabley++) {
		for (uint_fast8_t tablex = 0; tablex < 16; tablex++) {            
            //SPRT *sprt = &font[db][tablex][tabley];
            setSprt(sprt);
            setXY0(sprt, 10 + (tablex * 10), 10 + (tabley*17));
            setWH(sprt, CHAR_VRAM_WIDTH, CHAR_HEIGHT);
            // set position in texture
            const uint16_t uoffs =  tablex * CHAR_VRAM_WIDTH;
            const uint16_t voffs =  tabley * CHAR_HEIGHT;
            printf("uoffs %u voffs %u\n", uoffs, voffs);
            setUV0(sprt, uoffs, voffs);
            setClut(sprt, CLUT_X, CLUT_Y);
            setRGB0(sprt, 128, 128, 128);
            addPrim(ot[db], sprt);
            sprt++;
        }
    }    
    addPrim(ot[db], &fonttpage[db]);
}

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
    LoadImage(&currentrect, &PALETTE);
    const uint16_t tpage = getTPage(0, 0, FONT_X, 0);
    setDrawTPage(&fonttpage[0], 0, 0, tpage);
    setDrawTPage(&fonttpage[1], 0, 0, tpage);

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
    FntPrint(message);
    FntFlush(-1);
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
    SPT_DUMP
} SCREEN_PAGE_TYPE;

typedef struct {
    const char *devnumber;
    size_t filesize;    
} MCS_LIST_EXTRADATA;

typedef struct {
    char label[20];
    union {
        MCS_LIST_EXTRADATA mcslist;
    };
} MENUITEM;

typedef struct {
    uint16_t count;
    uint16_t index;
    void(*handle)(void);
    SCREEN_PAGE_TYPE back;
    bool loaded;
    MENUITEM items[15];
} MENU;


typedef struct {
    void (*exit)(void);
    const char *statustext;
    const uint8_t *buf;
    size_t bufsize;
    const uint8_t *readhead;
    const uint8_t *readend;
    uint32_t read_bytes_left; // number of bytes available to be fetched
    int sleep_frames;
    uint16_t frameindex;

    uint16_t framesize;
    uint8_t startdata[6];
    uint8_t enddata[4];
    const uint8_t *printhead;
    
    // used only for file read dump
    int fd;    
    char filename[25];
    
} DUMP;

typedef struct {
    void (*show)(void);
    void (*on_vsync)(void);

    union {
        MENU menu;
        DUMP dump;
    };

} SCREEN_PAGE;

typedef struct {
    SCREEN_PAGE_TYPE current;
    SCREEN_PAGE_TYPE last;

    SCREEN_PAGE page_select_device;
    SCREEN_PAGE page_mcs_list;
    SCREEN_PAGE page_dump;

    SCREEN_PAGE *curpage;
} SCREEN_PAGES;


SCREEN_PAGES sp;

void screen_page_change(const SCREEN_PAGE_TYPE new)
{
    sp.current = new;
    switch(new)
    {
        case SPT_SELECT_DEVICE:
        sp.curpage = &sp.page_select_device;        
        break;
        case SPT_MCS_LIST:
        sp.curpage = &sp.page_mcs_list;
        break;
        case SPT_DUMP:
        sp.curpage = &sp.page_dump;
        break;
    }
    sp.curpage->show();
}



void menu_show(void)
{
    MENU *menu = &sp.curpage->menu;
    for(int i = 0; i < menu->count; i++)
    {
        FntPrint("%s\n", menu->items[i].label);
    }
    FntFlush(-1);
}

void menu_on_vsync(void)
{
    MENU *menu = &sp.curpage->menu;
    if(new_buttons_pressed(BTN_CROSS))
    {
        menu->handle();
        return;
    }
    else if(new_buttons_pressed(BTN_TRIANGLE))
    {
        if(menu->back != sp.current)
        {
            menu->loaded = false;
            menu->count = 0;
            menu->index = 0;
            screen_page_change(menu->back);
            return;
        }
    }
    else if(buttons_pressed(BTN_UP))
    {
        if(menu->index > 0)
        {
            menu->index--;           
        }
    }
    else if(buttons_pressed(BTN_DOWN))
    {
        if(menu->index < (menu->count-1))
        {
            menu->index++;            
        }
    }
    menu_show();   
}

void select_device_handle(void)
{
    screen_page_change(SPT_MCS_LIST);        
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
                goto dump_frame_cleanup;
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

        // encode frameindex
        dump->startdata[0] = dump->frameindex;
        dump->startdata[1] = dump->frameindex >> 8;

        // encode size
        const size_t bufleft = dump->readend - dump->readhead; 
        const uint16_t thisframesize = (bufleft > FRAME_DATA_SIZE) ? FRAME_DATA_SIZE : bufleft;
        dump->startdata[4] = thisframesize;
        dump->startdata[5] = thisframesize >> 8;
        dump->framesize = thisframesize;

        // encode crc32
        const uint32_t checksum = crc32_frame_ex(dump->startdata, FRAME_HEADER_SIZE, dump->readhead, thisframesize);
        dump->enddata[0] = (uint8_t)(checksum);
        dump->enddata[1] = (uint8_t)(checksum >> 8);
        dump->enddata[2] = (uint8_t)(checksum >> 16);
        dump->enddata[3] = (uint8_t)(checksum >> 24);        

        // display for half a second
        dump->sleep_frames = 30;

        dump->printhead = dump->readhead;
        dump->readhead += thisframesize;
        dump->frameindex++;
    }

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

    dump->sleep_frames--;
    return;

    dump_frame_cleanup:
    dump->exit();
}

void dump_start(DUMP *dump, const uint16_t fullframecnt)
{    
    const uint16_t lastframeindex = fullframecnt-1;
    dump->frameindex = 0;
    dump->startdata[2] = lastframeindex;
    dump->startdata[3] = lastframeindex >> 8;

    setRGB0(&draw[0], 255, 255, 255);
    setRGB0(&draw[1], 255, 255, 255);
    sp.page_dump.on_vsync = &dump_frame;
    dump_frame();
}

void dump_exit(void)
{
    setRGB0(&draw[0], 50, 50, 50);
    setRGB0(&draw[1], 50, 50, 50);
    screen_page_change(SPT_MCS_LIST);
}

static inline void dump_show(void)
{
    DUMP *dump = &sp.page_dump.dump;
    dump->exit = &dump_exit;
    dump->statustext = "";
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
    dump->exit();
}

void dump_file_sleep(void)
{
    DUMP *dump = &sp.page_dump.dump;
    dump->sleep_frames--;
    if(dump->sleep_frames == 0)
    {
        dump->statustext = "Starting file dump";
        sp.page_dump.on_vsync = &dump_file_start;
    }    
    output_status(dump->statustext);
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
        dump->exit();
        return;
    }
    dump->exit = &dump_file_exit;
    dump->sleep_frames = 6;
    sp.page_dump.on_vsync = &dump_file_sleep;
    output_status(dump->statustext);
}

void dump_file_show(void) {
    dump_show();
    sp.page_dump.on_vsync = &dump_file_open;
    sp.page_dump.dump.statustext = "Opening file";    
    output_status(sp.page_dump.dump.statustext);    
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
    dump->exit();
}

void mcs_list_load(void)
{
    sp.page_mcs_list.menu.loaded = true;
    sp.page_mcs_list.on_vsync = &menu_on_vsync;

    InitCARD(1);
    StartCARD();
    _bu_init();

    struct DIRENTRY file;
    if(firstfile("bu00:*", &file) == NULL)
    {
        output_status("firstfile failed");
        return;
    }
    int i = 0;
    do {
        file.name[19] = '\0';
        printf("file %s size %u\n", file.name, file.size);
        sprintf(sp.page_mcs_list.menu.items[i].label, "%s", file.name);
        sp.page_mcs_list.menu.items[i].mcslist.devnumber = "bu00:";
        sp.page_mcs_list.menu.items[i].mcslist.filesize = file.size;
        i++;
    } while(nextfile(&file) != NULL);
    sp.page_mcs_list.menu.count = i;

    menu_on_vsync();    
}

void mcs_list_show(void)
{
    menu_show();
    if(!sp.page_mcs_list.menu.loaded)
    {
        sp.page_mcs_list.on_vsync = &mcs_list_load;
    }
}


void mcs_list_handle(void)
{
    const MENUITEM *item = &sp.page_mcs_list.menu.items[sp.page_mcs_list.menu.index];
    sprintf(sp.page_dump.dump.filename, "%s%s", item->mcslist.devnumber, item->label);
    sp.page_dump.dump.read_bytes_left = item->mcslist.filesize;
    sp.page_dump.show = &dump_file_show;
    screen_page_change(SPT_DUMP);
}

int main(void)
{   
    sp.page_select_device.show = &menu_show;
    sp.page_select_device.on_vsync = &menu_on_vsync;    
    sp.page_select_device.menu.back = SPT_SELECT_DEVICE;
    sp.page_select_device.menu.handle = &select_device_handle;
    strcpy(sp.page_select_device.menu.items[0].label, "Dump mc0 saves");
    sp.page_select_device.menu.count = 1;

    sp.page_mcs_list.show = &mcs_list_show;    
    sp.page_mcs_list.menu.back = SPT_SELECT_DEVICE;
    sp.page_mcs_list.menu.handle = &mcs_list_handle;
    sp.page_mcs_list.menu.loaded = false;
    sp.page_mcs_list.menu.count = 0;

    init();
    InitPAD( padbuff[0], 34, padbuff[1], 34 );
    padbuff[0][0] = padbuff[0][1] = 0xff;
    padbuff[1][0] = padbuff[1][1] = 0xff;
    StartPAD();
    
    screen_page_change(SPT_SELECT_DEVICE);  
    
    while(1)
    {       
        ClearOTagR(ot[db], OTLEN);
        dumpfont();
        sp.curpage->on_vsync();
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
