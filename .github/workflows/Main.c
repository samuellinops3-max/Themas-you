/*
 * Yoi Themes - app homebrew para PS3 (PSL1GHT) - versao offline
 *
 * - Lista os arquivos .p3t da pasta /themes/ dentro do proprio PKG
 * - CIMA/BAIXO escolhe, X copia o tema para /dev_hdd0/theme/, O volta/sai
 * - Nenhum acesso à rede
 *
 * Formato do nome do arquivo (opcional): "Autor - Nome do Tema.p3t"
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <malloc.h>
#include <ppu-types.h>

#include <sysutil/sysutil.h>
#include <sysutil/video.h>
#include <io/pad.h>
#include <rsx/rsx.h>

/* ------------------------------------------------------------------ config */

#define APP_ID          "YOIT00001"
#define THEMES_DIR      "/dev_hdd0/game/" APP_ID "/USRDIR/themes"
#define THEME_DEST      "/dev_hdd0/theme/"
#define MAX_THEMES      128
#define NAME_LEN        128
#define TITLE_MAX       38

#define MSG_DONE        "Download concluido! Va em Configuracoes > Tema para aplicar"

#define HOST_SIZE       (32 * 1024 * 1024)
#define CMD_SIZE        (1024 * 1024)
#define COPY_CHUNK      65536

#define CARD_H          90
#define CARD_GAP        10
#define LIST_Y          175
#define VISIBLE         5

/* cores em XRGB */
#define C_BG            0x00101014
#define C_CARD          0x001C1C26
#define C_CARD_SEL      0x002A2A3A
#define C_PURPLE        0x009B4DFF
#define C_WHITE         0x00FFFFFF
#define C_GREY          0x00A0A0B8
#define C_GREEN         0x0050E080
#define C_RED           0x00FF5060
#define C_BAR_BG        0x00303040

/* botoes digitais (pad.button[2]) */
#define BTN_UP          0x0010
#define BTN_DOWN        0x0040
#define BTN_CROSS       0x4000
#define BTN_CIRCLE      0x2000

/* ------------------------------------------------------------------ tipos */

typedef struct {
    char file[NAME_LEN];    /* nome do arquivo, ex: "Yoi - Dark.p3t" */
    char title[NAME_LEN];   /* nome exibido */
    char author[NAME_LEN];  /* autor exibido */
    char size[16];          /* tamanho formatado */
    u32  bytes;             /* tamanho em bytes */
} Theme;

typedef enum { ST_LIST, ST_COPYING, ST_MESSAGE } State;

/* ------------------------------------------------------------------ estado */

static Theme themes[MAX_THEMES];
static int   theme_count = 0;
static int   sel = 0;
static State state = ST_LIST;
static char  status_msg[256] = "";
static int   msg_is_error = 0;
static u32   prog_done = 0, prog_total = 0;
static volatile int running = 1;
static u16   prev_buttons = 0;
static u8    copy_buf[COPY_CHUNK];

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

static void draw_card(int idx, int y, int selected)
{
    const int x = 60, w = 1160, h = CARD_H;
    Theme *t = &themes[idx];
    char buf[96];

    rect(x, y, w, h, selected ? C_CARD_SEL : C_CARD);
    if (selected)
        outline(x, y, w, h, 4, C_PURPLE);

    /* preview: bloco colorido com cor derivada do nome do arquivo */
    u32 hash = 0;
    for (const char *p = t->file; *p; p++)
        hash = hash * 31 + (unsigned char)*p;
    u32 pc = (((hash & 0x7F) + 0x30) << 16)
           | ((((hash >> 7) & 0x7F) + 0x30) << 8)
           | (((hash >> 14) & 0x7F) + 0x60);
    rect(x + 10, y + 10, 150, 70, pc);
    draw_text(x + 44, y + 34, "PREVIEW", 2, C_WHITE);

    draw_text(x + 180, y + 12, t->title, 3, C_WHITE);

    snprintf(buf, sizeof(buf), "Autor: %s", t->author);
    draw_text(x + 180, y + 50, buf, 2, C_GREY);

    int tw = text_width(t->size, 2);
    draw_text(x + w - 20 - tw, y + 14, t->size, 2, C_GREY);
}

static void draw_list(void)
{
    int first = sel - VISIBLE / 2;
    if (first > theme_count - VISIBLE)
        first = theme_count - VISIBLE;
    if (first < 0)
        first = 0;

    for (int i = 0; i < VISIBLE && first + i < theme_count; i++) {
        int idx = first + i;
        draw_card(idx, LIST_Y + i * (CARD_H + CARD_GAP), idx == sel);
    }
}

static void draw_progress(void)
{
    const int bx = 240, by = 380, bw = 800, bh = 28;
    char buf[96];

    rect(bx, by, bw, bh, C_BAR_BG);
    if (prog_total > 0) {
        u64 fill = (u64)prog_done * bw / prog_total;
        rect(bx, by, (int)fill, bh, C_PURPLE);
        snprintf(buf, sizeof(buf), "%u%%  (%u / %u KB)",
                 (unsigned)((u64)prog_done * 100 / prog_total),
                 (unsigned)(prog_done / 1024), (unsigned)(prog_total / 1024));
    } else {
        snprintf(buf, sizeof(buf), "%u KB", (unsigned)(prog_done / 1024));
    }
    draw_centered(by + 45, buf, 2, C_GREY);
}

static void render(void)
{
    gcmSetWaitFlip(ctx);

    rect(0, 0, fb_w, fb_h, C_BG);
    draw_text(60, 40, "YOI THEMES", 5, C_WHITE);
    draw_text(60, 110, "Temas para o XMB", 2, C_GREY);
    rect(60, 150, 1160, 3, C_PURPLE);

    switch (state) {
    case ST_LIST:
        if (theme_count == 0)
            draw_centered(330, "Nenhum tema encontrado no pacote", 3, C_GREY);
        else
            draw_list();
        break;
    case ST_COPYING:
        draw_centered(300, "Copiando tema...", 3, C_WHITE);
        draw_progress();
        break;
    case ST_MESSAGE:
        draw_centered(330, status_msg, 3, msg_is_error ? C_RED : C_GREEN);
        break;
    }

    draw_text(60, 670, "CIMA/BAIXO: escolher     X: copiar     O: voltar", 2, C_GREY);

    rsxFlushBuffer(ctx);
    gcmSetFlip(ctx, cur);
    cur ^= 1;
}

/* ------------------------------------------------------------------ logica */

static void show_message(const char *text, int is_error)
{
    snprintf(status_msg, sizeof(status_msg), "%s", text);
    msg_is_error = is_error;
    state = ST_MESSAGE;
}

static int ends_with_p3t(const char *name)
{
    size_t l = strlen(name);
    return l > 4 && strcasecmp(name + l - 4, ".p3t") == 0;
}

static void format_size(u32 bytes, char *out, size_t cap)
{
    u32 kb = bytes / 1024;
    if (kb >= 1024)
        snprintf(out, cap, "%u.%uMB", kb / 1024, ((kb % 1024) * 10) / 1024);
    else
        snprintf(out, cap, "%uKB", kb);
}

/* "Autor - Nome.p3t" -> author/title; senao autor desconhecido e nome do arquivo */
static void parse_title_author(const char *file, Theme *t)
{
    char base[NAME_LEN];
    snprintf(base, sizeof(base), "%s", file);
    size_t l = strlen(base);
    if (l > 4)
        base[l - 4] = '\0';

    const char *sep = strstr(base, " - ");
    if (sep) {
        size_t alen = (size_t)(sep - base);
        if (alen >= sizeof(t->author))
            alen = sizeof(t->author) - 1;
        memcpy(t->author, base, alen);
        t->author[alen] = '\0';
        snprintf(t->title, sizeof(t->title), "%s", sep + 3);
    } else {
        snprintf(t->author, sizeof(t->author), "Desconhecido");
        snprintf(t->title, sizeof(t->title), "%s", base);
    }

    for (char *p = t->title; *p; p++)
        if (*p == '_')
            *p = ' ';

    if (strlen(t->title) > TITLE_MAX)
        t->title[TITLE_MAX] = '\0';
}

static int cmp_theme(const void *a, const void *b)
{
    return strcasecmp(((const Theme *)a)->file, ((const Theme *)b)->file);
}

static void scan_themes(void)
{
    theme_count = 0;
    sel = 0;

    DIR *d = opendir(THEMES_DIR);
    if (!d) {
        state = ST_LIST;   /* lista vazia; a mensagem aparece no meio da tela */
        return;
    }

    struct dirent *e;
    while (theme_count < MAX_THEMES && (e = readdir(d)) != NULL) {
        if (!ends_with_p3t(e->d_name))
            continue;

        Theme *t = &themes[theme_count];
        memset(t, 0, sizeof(*t));
        snprintf(t->file, sizeof(t->file), "%s", e->d_name);

        char path[512];
        snprintf(path, sizeof(path), "%s/%s", THEMES_DIR, e->d_name);
        struct stat st;
        if (stat(path, &st) == 0)
            t->bytes = (u32)st.st_size;
        format_size(t->bytes, t->size, sizeof(t->size));

        parse_title_author(e->d_name, t);
        theme_count++;
    }
    closedir(d);

    if (theme_count > 1)
        qsort(themes, theme_count, sizeof(Theme), cmp_theme);

    state = ST_LIST;
}

/* Copia src -> dst em blocos, atualizando a barra de progresso */
static int copy_file(const char *src, const char *dst)
{
    FILE *in = fopen(src, "rb");
    if (!in)
        return -1;

    FILE *out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return -2;
    }

    int err = 0;
    size_t n;
    prog_done = 0;
    while ((n = fread(copy_buf, 1, sizeof(copy_buf), in)) > 0) {
        if (fwrite(copy_buf, 1, n, out) != n) {
            err = -3;
            break;
        }
        prog_done += (u32)n;
        render();
    }

    fclose(in);
    fclose(out);
    return err;
}

static void copy_selected(void)
{
    Theme *t = &themes[sel];
    char src[512], dst[256];

    snprintf(src, sizeof(src), "%s/%s", THEMES_DIR, t->file);
    snprintf(dst, sizeof(dst), "%s%s", THEME_DEST, t->file);

    prog_total = t->bytes;
    prog_done = 0;
    state = ST_COPYING;
    render();

    int r = copy_file(src, dst);
    if (r == -1) {
        show_message("Erro ao ler o tema dentro do pacote", 1);
        return;
    }
    if (r == -2) {
        show_message("Nao foi possivel gravar em /dev_hdd0/theme/", 1);
        return;
    }
    if (r != 0) {
        show_message("Erro ao gravar o arquivo do tema", 1);
        return;
    }
    show_message(MSG_DONE, 0);
}

/* ------------------------------------------------------------------ input */

static void handle_input(void)
{
    padInfo info;
    padData pd;

    ioPadGetInfo(&info);
    if (!info.status[0])
        return;
    ioPadGetData(0, &pd);
    if (pd.len < 3)
        return;

    u16 btn = pd.button[2];
    u16 pressed = btn & (u16)~prev_buttons;
    prev_buttons = btn;

    switch (state) {
    case ST_LIST:
        if (theme_count > 0) {
            if (pressed & BTN_UP)
                sel = (sel + theme_count - 1) % theme_count;
            if (pressed & BTN_DOWN)
                sel = (sel + 1) % theme_count;
            if (pressed & BTN_CROSS)
                copy_selected();
        }
        if (pressed & BTN_CIRCLE)
            running = 0;
        break;
    case ST_MESSAGE:
        if (pressed & (BTN_CROSS | BTN_CIRCLE))
            state = ST_LIST;
        break;
    default:
        break;
    }
}

static void sysutil_cb(u64 status, u64 param, void *userdata)
{
    (void)param;
    (void)userdata;
    if (status == SYSUTIL_EXIT_GAME)
        running = 0;
}

/* ------------------------------------------------------------------ main */

int main(void)
{
    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, sysutil_cb, NULL);
    ioPadInit(7);

    init_video();
    scan_themes();

    while (running) {
        sysUtilCheckCallback();
        handle_input();
        render();
    }

    return 0;
}
