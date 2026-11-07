/*
 * Controle Tester - app homebrew para PS3 (PSL1GHT)
 *
 * Mostra em tempo real o estado dos botoes e dos analogicos do controle 1,
 * e os valores brutos (raw) de todos os controles conectados.
 * Sair: O (circulo)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ppu-types.h>

#include <sysutil/sysutil.h>
#include <sysutil/video.h>
#include <io/pad.h>
#include <rsx/rsx.h>

/* ------------------------------------------------------------------ config */

#define HOST_SIZE   (32 * 1024 * 1024)
#define CMD_SIZE    (1024 * 1024)
#define MAX_PADS    7

/* cores em XRGB */
#define C_BG        0x00101014
#define C_OFF       0x001C1C26
#define C_PURPLE    0x009B4DFF
#define C_WHITE     0x00FFFFFF
#define C_GREY      0x00A0A0B8
#define C_GREEN     0x0050E080
#define C_RED       0x00FF5060

/* Layout digital (CellPadData): button[2] = DIGITAL1, button[3] = DIGITAL2 */
typedef struct {
    const char *name;
    int layer;      /* 2 ou 3 (indice em pad.button[]) */
    u16 mask;
} ButtonDef;

static const ButtonDef buttons[16] = {
    { "SELECT",   2, 0x0001 },
    { "START",    2, 0x0008 },
    { "UP",       2, 0x0010 },
    { "RIGHT",    2, 0x0020 },
    { "DOWN",     2, 0x0040 },
    { "LEFT",     2, 0x0080 },
    { "L1",       2, 0x0400 },
    { "R1",       2, 0x0800 },
    { "L2",       2, 0x0100 },
    { "R2",       2, 0x0200 },
    { "L3",       2, 0x0002 },
    { "R3",       2, 0x0004 },
    { "TRIANGLE", 3, 0x1000 },
    { "CIRCLE",   3, 0x2000 },
    { "CROSS",    3, 0x4000 },
    { "SQUARE",   3, 0x8000 }
};

/* ------------------------------------------------------------------ estado */

static volatile int running = 1;
static u16 last_buttons[MAX_PADS][4];

/* ------------------------------------------------------------------ video */

static gcmContextData *ctx = NULL;
static u32  *fb[2];
static u32   fb_offset[2];
static u32   fb_w = 0, fb_h = 0;
static int   cur = 0;

static void init_video(void)
{
    void *host_addr = memalign(1024 * 1024, HOST_SIZE);
    rsxInit(&ctx, CMD_SIZE, HOST_SIZE, host_addr);

    videoState vstate;
    videoGetState(0, 0, &vstate);

    videoResolution res;
    videoGetResolution(vstate.displayMode.resolutionId, &res);
    fb_w = res.width;
    fb_h = res.height;

    videoConfiguration vconfig;
    memset(&vconfig, 0, sizeof(vconfig));
    vconfig.resolution = vstate.displayMode.resolutionId;
    vconfig.format     = VIDEO_BUFFER_FORMAT_XRGB;
    vconfig.pitch      = fb_w * 4;
    vconfig.aspect     = VIDEO_ASPECT_AUTO;
    videoConfigure(0, &vconfig, NULL, 0);
    videoGetState(0, 0, &vstate);

    gcmSetFlipMode(GCM_FLIP_VSYNC);

    for (int i = 0; i < 2; i++) {
        fb[i] = rsxMemalign(1024 * 1024, fb_w * fb_h * 4);
        rsxAddressToOffset(fb[i], &fb_offset[i]);
        gcmSetDisplayBuffer(i, fb_offset[i], fb_w * 4, fb_w, fb_h);
    }
}

static void rect(int x, int y, int w, int h, u32 col)
{
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = (x + w > (int)fb_w) ? (int)fb_w : x + w;
    int y1 = (y + h > (int)fb_h) ? (int)fb_h : y + h;
    u32 *base = fb[cur];
    for (int j = y0; j < y1; j++)
        for (int i = x0; i < x1; i++)
            base[(u32)j * fb_w + (u32)i] = col;
}

static void outline(int x, int y, int w, int h, int t, u32 col)
{
    rect(x, y, w, t, col);
    rect(x, y + h - t, w, t, col);
    rect(x, y, t, h, col);
    rect(x + w - t, y, t, h, col);
}

/* Fonte bitmap 5x7 para ASCII 32..126 (cada byte e uma coluna, bit0 = topo) */
static const unsigned char font5x7[95][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /*   */
    {0x00,0x00,0x5F,0x00,0x00}, /* ! */
    {0x00,0x07,0x00,0x07,0x00}, /* " */
    {0x14,0x7F,0x14,0x7F,0x14}, /* # */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* $ */
    {0x23,0x13,0x08,0x64,0x62}, /* % */
    {0x36,0x49,0x55,0x22,0x50}, /* & */
    {0x00,0x05,0x03,0x00,0x00}, /* ' */
    {0x00,0x1C,0x22,0x41,0x00}, /* ( */
    {0x00,0x41,0x22,0x1C,0x00}, /* ) */
    {0x08,0x2A,0x1C,0x2A,0x08}, /* * */
    {0x08,0x08,0x3E,0x08,0x08}, /* + */
    {0x00,0x50,0x30,0x00,0x00}, /* , */
    {0x08,0x08,0x08,0x08,0x08}, /* - */
    {0x00,0x60,0x60,0x00,0x00}, /* . */
    {0x20,0x10,0x08,0x04,0x02}, /* / */
    {0x3E,0x51,0x49,0x45,0x3E}, /* 0 */
    {0x00,0x42,0x7F,0x40,0x00}, /* 1 */
    {0x42,0x61,0x51,0x49,0x46}, /* 2 */
    {0x21,0x41,0x45,0x4B,0x31}, /* 3 */
    {0x18,0x14,0x12,0x7F,0x10}, /* 4 */
    {0x27,0x45,0x45,0x45,0x39}, /* 5 */
    {0x3C,0x4A,0x49,0x49,0x30}, /* 6 */
    {0x01,0x71,0x09,0x05,0x03}, /* 7 */
    {0x36,0x49,0x49,0x49,0x36}, /* 8 */
    {0x06,0x49,0x49,0x29,0x1E}, /* 9 */
    {0x00,0x36,0x36,0x00,0x00}, /* : */
    {0x00,0x56,0x36,0x00,0x00}, /* ; */
    {0x08,0x14,0x22,0x41,0x00}, /* < */
    {0x14,0x14,0x14,0x14,0x14}, /* = */
    {0x00,0x41,0x22,0x14,0x08}, /* > */
    {0x02,0x01,0x51,0x09,0x06}, /* ? */
    {0x32,0x49,0x79,0x41,0x3E}, /* @ */
    {0x7E,0x11,0x11,0x11,0x7E}, /* A */
    {0x7F,0x49,0x49,0x49,0x36}, /* B */
    {0x3E,0x41,0x41,0x41,0x22}, /* C */
    {0x7F,0x41,0x41,0x22,0x1C}, /* D */
    {0x7F,0x49,0x49,0x49,0x41}, /* E */
    {0x7F,0x09,0x09,0x09,0x01}, /* F */
    {0x3E,0x41,0x49,0x49,0x7A}, /* G */
    {0x7F,0x08,0x08,0x08,0x7F}, /* H */
    {0x00,0x41,0x7F,0x41,0x00}, /* I */
    {0x20,0x40,0x41,0x3F,0x01}, /* J */
    {0x7F,0x08,0x14,0x22,0x41}, /* K */
    {0x7F,0x40,0x40,0x40,0x40}, /* L */
    {0x7F,0x02,0x0C,0x02,0x7F}, /* M */
    {0x7F,0x04,0x08,0x10,0x7F}, /* N */
    {0x3E,0x41,0x41,0x41,0x3E}, /* O */
    {0x7F,0x09,0x09,0x09,0x06}, /* P */
    {0x3E,0x41,0x51,0x21,0x5E}, /* Q */
    {0x7F,0x09,0x19,0x29,0x46}, /* R */
    {0x46,0x49,0x49,0x49,0x31}, /* S */
    {0x01,0x01,0x7F,0x01,0x01}, /* T */
    {0x3F,0x40,0x40,0x40,0x3F}, /* U */
    {0x1F,0x20,0x40,0x20,0x1F}, /* V */
    {0x3F,0x40,0x38,0x40,0x3F}, /* W */
    {0x63,0x14,0x08,0x14,0x63}, /* X */
    {0x07,0x08,0x70,0x08,0x07}, /* Y */
    {0x61,0x51,0x49,0x45,0x43}, /* Z */
    {0x00,0x7F,0x41,0x41,0x00}, /* [ */
    {0x02,0x04,0x08,0x10,0x20}, /* \ */
    {0x00,0x41,0x41,0x7F,0x00}, /* ] */
    {0x04,0x02,0x01,0x02,0x04}, /* ^ */
    {0x40,0x40,0x40,0x40,0x40}, /* _ */
    {0x00,0x01,0x02,0x04,0x00}, /* ` */
    {0x20,0x54,0x54,0x54,0x78}, /* a */
    {0x7F,0x48,0x44,0x44,0x38}, /* b */
    {0x38,0x44,0x44,0x44,0x20}, /* c */
    {0x38,0x44,0x44,0x48,0x7F}, /* d */
    {0x38,0x54,0x54,0x54,0x18}, /* e */
    {0x08,0x7E,0x09,0x01,0x02}, /* f */
    {0x0C,0x52,0x52,0x52,0x3E}, /* g */
    {0x7F,0x08,0x04,0x04,0x78}, /* h */
    {0x00,0x44,0x7D,0x40,0x00}, /* i */
    {0x20,0x40,0x44,0x3D,0x00}, /* j */
    {0x7F,0x10,0x28,0x44,0x00}, /* k */
    {0x00,0x41,0x7F,0x40,0x00}, /* l */
    {0x7C,0x04,0x18,0x04,0x78}, /* m */
    {0x7C,0x08,0x04,0x04,0x78}, /* n */
    {0x38,0x44,0x44,0x44,0x38}, /* o */
    {0x7C,0x14,0x14,0x14,0x08}, /* p */
    {0x08,0x14,0x14,0x18,0x7C}, /* q */
    {0x7C,0x08,0x04,0x04,0x08}, /* r */
    {0x48,0x54,0x54,0x54,0x20}, /* s */
    {0x04,0x3F,0x44,0x40,0x20}, /* t */
    {0x3C,0x40,0x40,0x20,0x7C}, /* u */
    {0x1C,0x20,0x40,0x20,0x1C}, /* v */
    {0x3C,0x40,0x30,0x40,0x3C}, /* w */
    {0x44,0x28,0x10,0x28,0x44}, /* x */
    {0x0C,0x50,0x50,0x50,0x3C}, /* y */
    {0x44,0x64,0x54,0x4C,0x44}, /* z */
    {0x00,0x08,0x36,0x41,0x00}, /* { */
    {0x00,0x00,0x7F,0x00,0x00}, /* | */
    {0x00,0x41,0x36,0x08,0x00}, /* } */
    {0x08,0x08,0x2A,0x1C,0x08}  /* ~ */
};

static void draw_glyph(int x, int y, char ch, int sc, u32 col)
{
    unsigned char c = (unsigned char)ch;
    if (c < 32 || c > 126)
        c = '?';
    const unsigned char *g = font5x7[c - 32];
    for (int cx = 0; cx < 5; cx++)
        for (int ry = 0; ry < 7; ry++)
            if (g[cx] & (1 << ry))
                rect(x + cx * sc, y + ry * sc, sc, sc, col);
}

static void draw_text(int x, int y, const char *s, int sc, u32 col)
{
    for (; *s; s++) {
        draw_glyph(x, y, *s, sc, col);
        x += 6 * sc;
    }
}

static int text_width(const char *s, int sc)
{
    int n = (int)strlen(s);
    return n > 0 ? n * 6 * sc - sc : 0;
}

static void draw_centered(int y, const char *s, int sc, u32 col)
{
    draw_text(((int)fb_w - text_width(s, sc)) / 2, y, s, sc, col);
}

/* ------------------------------------------------------------------ render */

static void draw_analog_bar(int x, int y, int w, const char *label, u16 raw)
{
    char buf[32];
    /* valor 0..255 mapeado para a barra; centro ~128 */
    int v = (int)raw > 255 ? 255 : (int)raw;
    rect(x, y, w, 20, C_OFF);
    rect(x, y, (v * w) / 255, 20, C_PURPLE);
    snprintf(buf, sizeof(buf), "%s %u", label, (unsigned)raw);
    draw_text(x, y - 22, buf, 2, C_GREY);
}

static void draw_pad_one(void)
{
    padData pd;
    ioPadGetData(0, &pd);

    /* grade 4x4 de botoes */
    const int cell_w = 200, cell_h = 90, x0 = 80, y0 = 190;
    for (int i = 0; i < 16; i++) {
        int col = i % 4, row = i / 4;
        int x = x0 + col * (cell_w + 20);
        int y = y0 + row * (cell_h + 14);
        const ButtonDef *b = &buttons[i];
        int pressed = (pd.len >= 4) && (pd.button[b->layer] & b->mask);
        rect(x, y, cell_w, cell_h, pressed ? C_PURPLE : C_OFF);
        draw_centered(y + 36, b->name, 2, pressed ? C_WHITE : C_GREY);
    }

    /* analogicos (valores brutos, ver README) */
    if (pd.len >= 8) {
        draw_analog_bar(860, 200, 360, "A1", pd.button[4]);
        draw_analog_bar(860, 270, 360, "A2", pd.button[5]);
        draw_analog_bar(860, 340, 360, "A3", pd.button[6]);
        draw_analog_bar(860, 410, 360, "A4", pd.button[7]);
    }
}

static void draw_pad_status(void)
{
    padInfo info;
    ioPadGetInfo(&info);

    int connected = 0;
    for (int i = 0; i < MAX_PADS; i++)
        if (info.status[i])
            connected++;

    char line[64];
    snprintf(line, sizeof(line), "Controles conectados: %d", connected);
    draw_text(60, 120, line, 2, C_WHITE);

    /* dados brutos de cada controle conectado */
    int y = 510;
    for (int i = 0; i < MAX_PADS && y < 640; i++) {
        if (!info.status[i])
            continue;
        padData pd;
        ioPadGetData(i, &pd);
        snprintf(line, sizeof(line),
                 "P%d len=%u b2=%04X b3=%04X a=%u,%u,%u,%u",
                 i + 1, (unsigned)pd.len,
                 (unsigned)pd.button[2], (unsigned)pd.button[3],
                 (unsigned)pd.button[4], (unsigned)pd.button[5],
                 (unsigned)pd.button[6], (unsigned)pd.button[7]);
        draw_text(60, y, line, 2, C_GREY);
        y += 30;
    }
}

static void render(void)
{
    gcmSetWaitFlip(ctx);

    rect(0, 0, fb_w, fb_h, C_BG);
    draw_text(60, 40, "CONTROLE TESTER", 5, C_WHITE);
    rect(60, 150, 1160, 3, C_PURPLE);

    draw_pad_status();
    draw_pad_one();

    draw_text(60, 680, "Aperte os botoes. O: sair", 2, C_GREY);

    rsxFlushBuffer(ctx);
    gcmSetFlip(ctx, cur);
    cur ^= 1;
}

/* ------------------------------------------------------------------ input */

static void sysutil_cb(u64 status, u64 param, void *userdata)
{
    (void)param;
    (void)userdata;
    if (status == SYSUTIL_EXIT_GAME)
        running = 0;
}

static void handle_input(void)
{
    padInfo info;
    padData pd;

    ioPadGetInfo(&info);
    if (!info.status[0])
        return;
    ioPadGetData(0, &pd);
    if (pd.len < 4)
        return;

    /* O (circulo) sai do app */
    if (pd.button[3] & 0x2000)
        running = 0;
}

/* ------------------------------------------------------------------ main */

int main(void)
{
    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, sysutil_cb, NULL);
    ioPadInit(MAX_PADS);

    init_video();

    while (running) {
        sysUtilCheckCallback();
        handle_input();
        render();
    }

    return 0;
}
