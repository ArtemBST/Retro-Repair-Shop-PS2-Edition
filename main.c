#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <libpad.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <gsKit.h>
#include <dmaKit.h>
#include <gsToolkit.h>

/* --- Структуры данных (без изменений) --- */
typedef struct {
    int id;
    char name[32];
    char issue[64];
    int repair_x;
    int repair_y;
    int req_tool;
    int reward;
    int is_complex;
} ConsoleLevel;

ConsoleLevel levels[18] = {
    {1, "Game Boy NINTENDO", "Clean battery contacts", 30, 10, 2, 15, 0},
    {2, "Dendy / Famicom", "Blow cartridge slot", 25, 12, 2, 10, 0},
    {3, "SEGA Mega Drive", "Fix D-Pad buttons", 20, 15, 0, 25, 0},
    {4, "PlayStation 1", "Adjust CD laser lens", 28, 11, 0, 40, 0},
    {5, "Game Boy Color", "Replace screen lens", 32, 9, 0, 20, 0},
    {6, "SEGA Dreamcast", "Replace RTC battery", 26, 13, 1, 50, 0},
    {7, "PSone", "Fix power jack", 29, 10, 1, 45, 0},
    {8, "PlayStation 2", "Lubricate DVD tray", 24, 14, 2, 35, 0},
    {9, "Xbox Original", "Replace thermal paste", 22, 12, 2, 60, 0},
    {10, "Game Boy Advance SP", "Fix hinge & battery", 31, 8, 0, 30, 0},
    {11, "PlayStation Portable", "Replace analog stick", 27, 11, 0, 50, 0},
    {12, "PlayStation 3", "Clean cooling fan", 21, 15, 2, 70, 1},
    {13, "Xbox 360", "Replace DVD drive belt", 23, 13, 0, 55, 1},
    {14, "Nintendo DS", "Calibrate touchscreen", 30, 10, 0, 40, 1},
    {15, "Nintendo Wii", "Fix Sensor Bar port", 25, 12, 1, 65, 1},
    {16, "PlayStation 4", "Deep dust cleaning", 20, 14, 2, 90, 1},
    {17, "Xbox One", "Repair the disk drive", 29, 10, 1, 120, 1},
    {18, "PlayStation Vita", "Repair the graphics chip", 31, 20, 2, 100, 0}
};

const char *tool_names[3] = { "[SCREWDRIVER]", "[SOLDERING IRON]", "[ALCOHOL BRUSH]" };
unsigned int qte_buttons[3] = {PAD_TRIANGLE, PAD_SQUARE, PAD_CIRCLE};
const char *qte_names[3] = {"TRIANGLE", "SQUARE", "CIRCLE"};

/* --- Глобальные переменные для gsKit --- */
GSGLOBAL *gsGlobal;
GSTEXTURE *fontTex;
GSFONT *gsFont;

/* Курсор для печати текста (эмуляция scr_printf) */
int printX = 10;
int printY = 10;

void my_scr_clear(void) {
    printX = 10;
    printY = 10;
    gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(0,0,0,0x80,0x00));
}

void my_scr_printf(const char *format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    for (int i = 0; buffer[i] != '\0'; i++) {
        if (buffer[i] == '\n') {
            printX = 10;
            printY += gsFont->CharHeight;
        } else {
            gsKit_font_print(gsGlobal, gsFont, printX, printY, 1, 0x80808080, buffer[i]);
            printX += gsFont->CharWidth;
        }
    }
}

/* --- Инициализация геймпада --- */
static char padBuf[256] __attribute__((aligned(64)));
int init_pad(void) {
    padInit(0);
    return padPortOpen(0, 0, padBuf);
}
unsigned int read_pad(void) {
    struct padButtonStatus buttons;
    unsigned int ret = 0;
    int state = padGetState(0, 0);
    if (state == PAD_STATE_STABLE || state == PAD_STATE_FINDCTP1) {
        if (padRead(0, 0, &buttons) != 0) {
            ret = 0xffff ^ buttons.btns;
        }
    }
    return ret;
}
int read_pad_lx(void) {
    struct padButtonStatus buttons;
    if (padRead(0, 0, &buttons) != 0) {
        return buttons.ljoy_h;
    }
    return 128;
}

int main(int argc, char *argv[]) {
    SifInitRpc(0);
    SifLoadFileInit();

    // Инициализация графики (gsKit)
    gsGlobal = gsKit_init_global();
    gsGlobal->Mode = GS_MODE_NTSC;
    gsGlobal->Interlace = GS_INTERLACED;
    gsGlobal->Field = GS_FIELD;
    gsGlobal->Width = 640;
    gsGlobal->Height = 448;
    gsGlobal->PSM = GS_PSM_CT24;
    gsGlobal->PSMZ = GS_PSMZ_16S;
    gsGlobal->ZBuffering = GS_SETTING_OFF;
    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);
    gsKit_init_screen(gsGlobal);

    // ЗАГРУЗКА ШРИФТА (требуется файл font.raw в корне)
    // ВАЖНО: Если файла нет, игра запустится, но текста не будет!
    int font_fd = fioOpen("host:font.raw", O_RDONLY); 
    if (font_fd >= 0) {
        // Если используешь PCSX2 - host: работает
        // Если на реальной приставке - замени на "mass:font.raw" (USB) или "mc0:/font.raw"
    } else {
        font_fd = fioOpen("mass:font.raw", O_RDONLY);
    }
    
    if (font_fd >= 0) {
        fontTex = gsKit_texture_fio(gsGlobal, "font.raw", NULL);
        gsFont = gsKit_init_font(gsGlobal, fontTex);
        gsKit_font_upload(gsGlobal, gsFont);
        fioClose(font_fd);
    }

    init_pad();

    int current_level = 0;
    int cursor_x = 20;
    int cursor_y = 10;
    int fix_progress = 0;
    unsigned int last_buttons = 0;
    int current_tool = 0;
    int money = 0;
    int is_sparking = 0;
    char spark_char = ' ';
    float solder_drift_x = 0.0f;
    int qte_active = 0;
    int qte_target_idx = 0;
    int qte_timer = 0;

    while (1) {
        unsigned int buttons = read_pad();
        int lx = read_pad_lx();

        my_scr_clear();
        
        my_scr_printf("==================================================\n");
        my_scr_printf("             RETRO REPAIR SHOP 2D  v3.0           \n");
        my_scr_printf("==================================================\n\n");

        ConsoleLevel *lvl = &levels[current_level];

        my_scr_printf(" WALLET: $%d | LEVEL %d/18: %s\n", money, lvl->id, lvl->name);
        my_scr_printf(" Issue: %s\n", lvl->issue);
        my_scr_printf(" Active Tool: %s ", tool_names[current_tool]);
        
        if (current_tool == lvl->req_tool) my_scr_printf("(CORRECT)\n");
        else my_scr_printf("(WRONG TOOL!)\n");

        if ((buttons & PAD_CROSS) && current_tool == lvl->req_tool) {
            if (lvl->req_tool == 1) {
                my_scr_printf(" [MINIGAME] KEEP SOLDER IN CENTER! ");
                int offset_x = (int)solder_drift_x;
                my_scr_printf("[");
                for(int i = -5; i <= 5; i++) {
                    if (i == offset_x) my_scr_printf("O");
                    else if (i == 0) my_scr_printf("|");
                    else my_scr_printf(".");
                }
                my_scr_printf("]\n");
            }
            else if (lvl->is_complex && qte_active) {
                my_scr_printf(" [QTE EVENT] !! PRESS %s NOW !! (Time: %d)   \n", qte_names[qte_target_idx], qte_timer);
            }
            else my_scr_printf("\n");
        } else my_scr_printf("\n");

        my_scr_printf(" Fix Progress: [");
        int bars = fix_progress / 10;
        for (int i = 0; i < 10; i++) {
            if (i < bars) my_scr_printf("#");
            else my_scr_printf(".");
        }
        my_scr_printf("] %d%%\n\n", fix_progress);

        is_sparking = 0;
        for (int y = 5; y < 18; y++) {
            my_scr_printf("  ");
            for (int x = 5; x < 55; x++) {
                if ((buttons & PAD_CROSS) && abs(cursor_x - lvl->repair_x) <= 2 && abs(cursor_y - lvl->repair_y) <= 1) {
                    spark_char = (current_tool == lvl->req_tool) ? '*' : '~';
                    if (abs(x - cursor_x) <= 2 && abs(y - cursor_y) <= 1 && (rand() % 3 == 0)) is_sparking = 1;
                }
                if (x == cursor_x && y == cursor_y) my_scr_printf("+");
                else if (is_sparking && abs(x - cursor_x) <= 2 && abs(y - cursor_y) <= 1 && rand() % 2 == 0) my_scr_printf("%c", spark_char);
                else if (x == lvl->repair_x && y == lvl->repair_y) my_scr_printf("X");
                else if (x >= 15 && x <= 45 && y >= 8 && y <= 16) my_scr_printf("=");
                else my_scr_printf(" ");
            }
            my_scr_printf("\n");
        }

        my_scr_printf("\n Controls: D-Pad=Move | L1/R1=Tool | X=Repair (Hold) | START=Exit\n");

        // Отрисовка кадра
        gsKit_queue_exec(gsGlobal);
        gsKit_sync_flip(gsGlobal);

        unsigned int pressed = buttons & ~last_buttons;
        last_buttons = buttons;

        if (buttons & PAD_START) break;

        if (pressed & PAD_L1) { current_tool--; if (current_tool < 0) current_tool = 2; }
        if (pressed & PAD_R1) { current_tool++; if (current_tool > 2) current_tool = 0; }

        if (!(buttons & PAD_CROSS && current_tool == 1)) {
            if ((buttons & PAD_UP) && cursor_y > 5) cursor_y--;
            if ((buttons & PAD_DOWN) && cursor_y < 17) cursor_y++;
            if ((buttons & PAD_LEFT) && cursor_x > 5) cursor_x--;
            if ((buttons & PAD_RIGHT) && cursor_x < 54) cursor_x++;
        }

        if (buttons & PAD_CROSS) {
            if (abs(cursor_x - lvl->repair_x) <= 2 && abs(cursor_y - lvl->repair_y) <= 1) {
                if (current_tool == lvl->req_tool) {
                    if (current_tool == 1) {
                        solder_drift_x += ((rand() % 3) - 1) * 0.4f;
                        int stick_x = lx - 128;
                        if (abs(stick_x) > 20) solder_drift_x += (stick_x / 64.0f);
                        if (fabsf(solder_drift_x) <= 2.5f) fix_progress += 2;
                        else { if (fix_progress > 0) fix_progress--; }
                        if (solder_drift_x > 5.0f) solder_drift_x = 5.0f;
                        if (solder_drift_x < -5.0f) solder_drift_x = -5.0f;
                    }
                    else if (lvl->is_complex) {
                        if (!qte_active) {
                            if (rand() % 15 == 0) { qte_active = 1; qte_target_idx = rand() % 3; qte_timer = 30; }
                            fix_progress += 1;
                        } else {
                            qte_timer--;
                            if (pressed & qte_buttons[qte_target_idx]) { fix_progress += 15; qte_active = 0; }
                            else if (pressed & (PAD_TRIANGLE | PAD_SQUARE | PAD_CIRCLE)) { if (fix_progress > 5) fix_progress -= 5; qte_active = 0; }
                            else if (qte_timer <= 0) { if (fix_progress > 8) fix_progress -= 8; qte_active = 0; }
                        }
                    }
                    else fix_progress += 2;

                    if (fix_progress >= 100) {
                        money += lvl->reward;
                        fix_progress = 0; qte_active = 0; solder_drift_x = 0.0f; current_level++;
                        if (current_level >= 18) {
                            my_scr_clear();
                            my_scr_printf("\n\n  CONGRATULATIONS! YOU FIXED ALL CONSOLES!\n");
                            my_scr_printf("  TOTAL EARNED: $%d\n", money);
                            my_scr_printf("  YOU ARE THE ULTIMATE RETRO MASTER!\n");
                            gsKit_queue_exec(gsGlobal);
                            gsKit_sync_flip(gsGlobal);
                            for (volatile int i = 0; i < 100000000; i++) {}
                            break;
                        }
                    }
                } else {
                    if (fix_progress > 0) fix_progress--;
                    if (money > 0 && rand() % 10 == 0) money--;
                }
            }
        } else {
            qte_active = 0;
        }
        for (volatile int i = 0; i < 1000000; i++) {}
    }
    return 0;
}
