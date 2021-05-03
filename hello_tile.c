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
    BTN_UP       = 16,
    BTN_DOWN     = 64,
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
    DrawSync(0);
    setRGB0(&draw[0], 255, 255, 255);
    setRGB0(&draw[1], 255, 255, 255);

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

    setRGB0(&draw[0], 50, 50, 50);
    setRGB0(&draw[1], 50, 50, 50);
    ClearOTagR(ot[db], OTLEN);
    display_data();
    ClearOTagR(ot[db], OTLEN); 
}

void dump_save(const char *savename) {
    printf("opening file %s\n", savename);
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


typedef enum {
    DUMPST_FILE_OPEN,
    DUMPST_FILE_SLEEP,
    DUMPST_FILE_START,
    DUMPST_DUMP
} DUMPSTATE;

typedef struct {
    DUMPSTATE state;
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

    int fd;    
    char filename[25];
    const char *statustext;
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
    if(buttons_pressed(BTN_CROSS))
    {
        menu->handle();
        return;
    }
    else if(buttons_pressed(BTN_TRIANGLE))
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

void handle_mcs_list(void)
{
    screen_page_change(SPT_MCS_LIST);        
}

void mcs_list_on_vsync(void)
{
    if(!sp.curpage->menu.loaded)
    {
        sp.page_mcs_list.menu.loaded = true;

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
    }
    menu_on_vsync();    
}


void handle_dump_mcs(void)
{
    const MENUITEM *item = &sp.page_mcs_list.menu.items[sp.page_mcs_list.menu.index];
    sprintf(sp.page_dump.dump.filename, "%s%s", item->mcslist.devnumber, item->label);
    sp.page_dump.dump.read_bytes_left = item->mcslist.filesize;
    screen_page_change(SPT_DUMP);
}

void dump_show(void) {
    sp.page_dump.dump.state = DUMPST_FILE_OPEN;
    sp.page_dump.dump.statustext = "Opening file";
    output_status(sp.page_dump.dump.statustext);
}

bool do_dump(DUMP *dump)
{
    if(dump->sleep_frames == 0) {        
        // all bytes in buf copied, need to read
        if(dump->readhead == dump->readend)
        {
            if(dump->read_bytes_left == 0)
            {
                return false;
            }
            size_t toread = (dump->read_bytes_left > dump->bufsize) ? dump->bufsize : dump->read_bytes_left;
            int32_t res = read(dump->fd, (void*)dump->buf, toread);
            if(res != toread) {
                printf("File read failed\n");
                return false;
            }
            dump->read_bytes_left -= toread;
            dump->readhead = dump->buf;
            dump->readend = dump->readhead +  toread;
        }

        // encode frameindex
        dump->startdata[0] = dump->frameindex;
        dump->startdata[1] = dump->frameindex >> 8;

        // encode size
        const uint16_t nondatasize = sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint32_t); 
        const uint16_t framedatasize = (UCNT/8) - nondatasize;
        const size_t bufleft = dump->readend - dump->readhead; 
        const uint16_t thisframesize = (bufleft > framedatasize) ? framedatasize : bufleft;
        dump->startdata[4] = thisframesize;
        dump->startdata[5] = thisframesize >> 8;
        dump->framesize = thisframesize;

        // encode crc32
        const uint32_t checksum = crc32_frame_ex(dump->startdata, nondatasize-sizeof(uint32_t), dump->readhead, thisframesize);
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
    return true;
}

bool dump_start(DUMP *dump)
{
    dump->state = DUMPST_DUMP;
    dump->statustext = NULL;  

    const uint16_t nondatasize = sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint32_t); 
    const uint16_t framedatasize = (UCNT/8) - nondatasize;

    size_t size = (dump->readend - dump->readhead) + dump->read_bytes_left;
    if(size == 0) {
        return false;
    }
    // https://stackoverflow.com/questions/17944/how-to-round-up-the-result-of-integer-division
    uint16_t fullframecnt = (size + framedatasize - 1)/framedatasize;
    uint16_t lastframeindex = fullframecnt-1;
    dump->frameindex = 0;
    dump->startdata[2] = lastframeindex;
    dump->startdata[3] = lastframeindex >> 8;

    setRGB0(&draw[0], 255, 255, 255);
    setRGB0(&draw[1], 255, 255, 255);
    return do_dump(dump);
}

static uint8_t Dump_file_buf[0x2000];
void dump_vsync(void) {
    DUMP *dump = &sp.page_dump.dump;
    int32_t res;
    switch(dump->state)
    {
        case DUMPST_FILE_OPEN:
        dump->fd = open(dump->filename, 0x1);
        if(dump->fd < 0) {
            printf("Failed to open file\n");
            goto DUMP_VSYNC_EXIT;
        }
        dump->sleep_frames = 6;
        dump->state = DUMPST_FILE_SLEEP;
        break;

        case DUMPST_FILE_SLEEP:
        dump->sleep_frames--;
        if(dump->sleep_frames > 0) break;        
        dump->statustext = "Starting file dump";
        dump->state = DUMPST_FILE_START;
        break;

        case DUMPST_FILE_START:
        dump->buf = (const uint8_t*)&Dump_file_buf;
        dump->bufsize = sizeof(Dump_file_buf);
        dump->readhead = dump->buf;
        dump->readend = dump->readhead;        
        if(!dump_start(dump))
        {
            goto DUMP_VSYNC_FILE_EXIT;
        }
        break;

        case DUMPST_DUMP:
        if(!do_dump(dump))
        {
            goto DUMP_VSYNC_FILE_EXIT;
        }       
        break;
    }
    if(dump->statustext != NULL)
    {
        output_status(dump->statustext);
    }
    return;

    DUMP_VSYNC_FILE_EXIT:
    close(dump->fd);    
    DUMP_VSYNC_EXIT:
    setRGB0(&draw[0], 50, 50, 50);
    setRGB0(&draw[1], 50, 50, 50);
    screen_page_change(SPT_MCS_LIST);
}

//char pribuff[2][32768]; // Primitive buffer
//char *nextpri;          // Next primitive pointer
int main(void)
{   
    sp.page_select_device.show = &menu_show;
    sp.page_select_device.on_vsync = &menu_on_vsync;
    sp.page_select_device.menu.count = 1;
    sp.page_select_device.menu.back = SPT_SELECT_DEVICE;
    sp.page_select_device.menu.handle = &handle_mcs_list;
    strcpy(sp.page_select_device.menu.items[0].label, "Dump mc0 saves");

    sp.page_mcs_list.show = &menu_show;
    sp.page_mcs_list.on_vsync = &mcs_list_on_vsync;
    sp.page_mcs_list.menu.count = 0;
    sp.page_mcs_list.menu.back = SPT_SELECT_DEVICE;
    sp.page_mcs_list.menu.handle = &handle_dump_mcs;
    sp.page_mcs_list.menu.loaded = false;
    
    sp.page_dump.show = &dump_show;    
    sp.page_dump.on_vsync= &dump_vsync;

    init();
    InitPAD( padbuff[0], 34, padbuff[1], 34 );
    padbuff[0][0] = padbuff[0][1] = 0xff;
    padbuff[1][0] = padbuff[1][1] = 0xff;
    StartPAD();
    
    screen_page_change(SPT_SELECT_DEVICE);

    /*TILE *tile;
    uint8_t red = 255;
    for(int i = 0; i < 2; i++)
    {
        nextpri = pribuff[i];
        ClearOTagR(ot[i], OTLEN);
        tile = (TILE*)nextpri;      // Cast next primitive
        setTile(tile);              // Initialize the primitive (very important)
        setXY0(tile, 32, 32);       // Set primitive (x,y) position
        setWH(tile, 64, 64);        // Set primitive size
        setRGB0(tile, red*i, 255, 0); // Set color yellow
        addPrim(ot[i], tile);      // Add primitive to the ordering table        
        nextpri += sizeof(TILE);    // Advance the next primitive pointer
    }*/

    for(int i = 0; i < 2; i++)
    {
        ClearOTagR(ot[i], OTLEN);
    }
    
    while(1)
    {   
        ClearOTagR(ot[db], OTLEN);
        sp.curpage->on_vsync();
        DrawSync(0);
        VSync(0);         
        PutDispEnv(&disp[db]);
        PutDrawEnv(&draw[db]);
        DrawOTag(ot[db] + OTLEN - 1);
        db = !db;
        
        


        /*TILE *tile;
        ClearOTagR(ot[db], OTLEN);
        tile = (TILE*)nextpri;      // Cast next primitive
        setTile(tile);              // Initialize the primitive (very important)
        setXY0(tile, 32, 32);       // Set primitive (x,y) position
        setWH(tile, 64, 64);        // Set primitive size
        setRGB0(tile, 255, 255, 0); // Set color yellow
        addPrim(ot[db], tile);      // Add primitive to the ordering table        
        nextpri += sizeof(TILE);    // Advance the next primitive pointer
        DrawSync(0);
        VSync(0);
        PutDispEnv(&disp[db]);
        PutDrawEnv(&draw[db]);
        DrawOTag(ot[db] + OTLEN - 1);
        db = !db; 
        nextpri = pribuff[db];
        while(1);*/

        /*
        TILE *tile;
        ClearOTagR(ot[db], OTLEN);
        tile = (TILE*)nextpri;      // Cast next primitive
        setTile(tile);              // Initialize the primitive (very important)
        setXY0(tile, 32, 32);       // Set primitive (x,y) position
        setWH(tile, 64, 64);        // Set primitive size
        setRGB0(tile, 255, 255, 0); // Set color yellow
        addPrim(ot[db], tile);      // Add primitive to the ordering table        
        nextpri += sizeof(TILE);    // Advance the next primitive pointer
        DrawOTag(ot[db] + OTLEN - 1);
        DrawSync(0);
        VSync(0);
        db = !db;
        PutDispEnv(&disp[db]);
        PutDrawEnv(&draw[db]);
        */


        /*DrawOTag(ot[db] + OTLEN - 1);
        DrawSync(0);
        VSync(0);
        db = !db;
        PutDispEnv(&disp[db]);
        PutDrawEnv(&draw[db]);*/              
    }

    return 0;
}
