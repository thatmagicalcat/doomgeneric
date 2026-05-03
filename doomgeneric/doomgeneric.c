#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/time.h>

#include "doomkeys.h"
#include "m_argv.h"

#include "doomgeneric.h"

pixel_t* DG_ScreenBuffer = NULL;

void M_FindResponseFile(void);
void D_DoomMain (void);

struct FramebufferInfo framebuffer_info;
int keyboard_device_fd;
uint32_t* fb_ptr;

struct FramebufferInfo {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint16_t bpp;
    uint8_t memory_model;
    uint8_t r_sz, r_shift, g_sz, g_shift, b_sz, b_shift;
};

struct RawKeyEvent {
    uint64_t timestamp_nanos;
    uint8_t code;
    uint8_t state;
};

static const unsigned char keycode_to_doom[] = {
    KEY_ESCAPE, // escape
    KEY_F1, // f1
    KEY_F2, // f2
    KEY_F3, // f3
    KEY_F4, // f4
    KEY_F5, // f5
    KEY_F6, // f6
    KEY_F7, // f7
    KEY_F8, // f8
    KEY_F9, // f9
    KEY_F10, // f10
    KEY_F11, // f11
    KEY_F12, // f12
    0, // printscreen
    0, // sysrq
    0, // scrolllock
    KEY_PAUSE, // pausebreak
    '`', // oem8
    '1', // key1
    '2', // key2
    '3', // key3
    '4', // key4
    '5', // key5
    '6', // key6
    '7', // key7
    '8', // key8
    '9', // key9
    '0', // key0
    KEY_MINUS, // oemminus
    KEY_EQUALS, // oemplus
    KEY_BACKSPACE, // backspace
    0, // insert
    0, // home
    0, // pageup
    0, // numpadlock
    '/', // numpaddivide
    '*', // numpadmultiply
    '-', // numpadsubtract
    KEY_TAB, // tab
    'q', // q
    'w', // w
    'e', // e
    'r', // r
    't', // t
    'y', // y
    'u', // u
    'i', // i
    'o', // o
    'p', // p
    '[', // oem4
    ']', // oem6
    '\\', // oem5
    0, // oem7
    0, // delete
    0, // end
    0, // pagedown
    0, // numpad7
    0, // numpad8
    0, // numpad9
    '+', // numpadadd
    0, // capslock
    'a', // a
    's', // s
    'd', // d
    'f', // f
    'g', // g
    'h', // h
    'j', // j
    'k', // k
    'l', // l
    ';', // oem1
    '\'', // oem3
    KEY_ENTER, // return
    0, // numpad4
    0, // numpad5
    0, // numpad6
    KEY_RSHIFT, // lshift
    'z', // z
    'x', // x
    'c', // c
    'v', // v
    'b', // b
    'n', // n
    'm', // m
    ',', // oemcomma
    '.', // oemperiod
    '/', // oem2
    KEY_RSHIFT, // rshift
    KEY_UPARROW, // arrowup
    0, // numpad1
    0, // numpad2
    0, // numpad3
    KEY_ENTER, // numpadenter
    KEY_FIRE, // lcontrol
    0, // lwin
    KEY_LALT, // lalt
    KEY_USE, // spacebar
    KEY_RALT, // raltgr
    0, // rwin
    0, // apps
    KEY_FIRE, // rcontrol
    KEY_LEFTARROW, // arrowleft
    KEY_DOWNARROW, // arrowdown
    KEY_RIGHTARROW, // arrowright
    0, // numpad0
    0, // numpadperiod
    0, // oem9
    0, // oem10
    0, // oem11
    0, // oem12
    0, // oem13
    0, // prevtrack
    0, // nexttrack
    0, // mute
    0, // calculator
    0, // play
    0, // stop
    0, // volumedown
    0, // volumeup
    0, // wwwhome
    0, // powerontestok
    0, // toomanykeys
    KEY_FIRE, // rcontrol2
    KEY_RALT, // ralt2
};

void doomgeneric_Create(int argc, char **argv) {
    // save arguments
    myargc = argc;
    myargv = argv;

    M_FindResponseFile();

    DG_ScreenBuffer = malloc(DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4);

    DG_Init();

    D_DoomMain ();
}

void DG_Init() {
    keyboard_device_fd = open("/dev/kbd", O_RDONLY);

    int fd = open("/dev/fb0", O_RDWR);
    read(fd, &framebuffer_info, sizeof(framebuffer_info));

    fb_ptr = (uint32_t*)mmap(
        NULL,
        framebuffer_info.pitch * framebuffer_info.height,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        0
    );
}

int DG_GetKey(int* pressed, unsigned char* key) {
    struct RawKeyEvent event;
    int bytes_read = read(keyboard_device_fd, &event, sizeof(event));
    
    if (bytes_read == sizeof(event)) {
        *pressed = event.state;
        
        // translate keycode
        if (event.code < sizeof(keycode_to_doom)) {
            *key = keycode_to_doom[event.code];
        } else {
            *key = 0;
        }
        
        return 1;
    }
    
    return 0; // no key events available right now
}

void DG_DrawFrame() {
    if (!fb_ptr || !DG_ScreenBuffer) return;
    for (int y = 0; y < DOOMGENERIC_RESY; y++)
        memcpy(
            (uint8_t*)fb_ptr + (y * framebuffer_info.pitch),
            DG_ScreenBuffer + (y * DOOMGENERIC_RESX),
            DOOMGENERIC_RESX * 4
        );
}

int main(int argc, char **argv) {
    char *mock_argv[] = {
        "doomgeneric",
        "-iwad",
        "doom1.wad",
        NULL
    };

    int mock_argc = 3;
    doomgeneric_Create(mock_argc, mock_argv);
    while (1) doomgeneric_Tick();

    return 0;
}

void *__dso_handle = 0;

uint32_t DG_GetTicksMs() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint32_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

void DG_SleepMs(uint32_t ms) {
    usleep(ms * 1000);
}

void DG_SetWindowTitle(const char * title) { }
