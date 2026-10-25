#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <libpad.h>
#include <debug.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Структура консоли для ремонта — БЕЗ ИЗМЕНЕНИЙ */
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

const char *tool_names[3] = {
    "[SCREWDRIVER]",
    "[SOLDERING IRON]",
    "[ALCOHOL BRUSH]"
};

unsigned int qte_buttons[3] = {PAD_TRIANGLE, PAD_SQUARE, PAD_CIRCLE};
const char *qte_names[3] = {"TRIANGLE", "SQUARE", "CIRCLE"};

/* === Инициализация геймпада PS2 === */
static char padBuf[256] __attribute__((aligned(64)));

int init_pad(void) {
    int ret;
    padInit(0);
    ret = padPortOpen(0, 0, padBuf);
    if (!ret) return 0;
    return 1;
}

/* Возвращает битовую маску нажатых кнопок (аналог pad.Buttons) */
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

/* Возвращает X аналогового стика (0..255, центр 128) */
int read_pad_lx(void) {
    struct padButtonStatus buttons;
    if (padRead(0, 0, &buttons) != 0) {
        return buttons.ljoy_h;
    }
    return 128;
}

int main(int argc, char *argv[]) {
    /* Инициализация PS2 */
    SifInitRpc(0);
    SifLoadFileInit();
    init_scr();
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
        scr_clear();
        scr_setXY(0, 0);
        scr_printf("==================================================\n");
        scr_printf("             RETRO REPAIR SHOP 2D  v3.0           \n");
        scr_printf("==================================================\n\n");

        ConsoleLevel *lvl = &levels[current_level];

        scr_printf(" WALLET: $%d | LEVEL %d/18: %s\n", money, lvl->id, lvl->name);
        scr_printf(" Issue: %s\n", lvl->issue);

        scr_printf(" Active Tool: %s ", tool_names[current_tool]);
        if (current_tool == lvl->req_tool) scr_printf("(CORRECT)\n");
        else scr_printf("(WRONG TOOL!)\n");

        unsigned int buttons = read_pad();
        int lx = read_pad_lx();

        if ((buttons & PAD_CROSS) && current_tool == lvl->req_tool) {
            if (lvl->req_tool == 1) {
                scr_printf(" [MINIGAME] KEEP SOLDER IN CENTER! ");
                int offset_x = (int)solder_drift_x;
                scr_printf("[");
                for(int i = -5; i <= 5; i++) {
                    if (i == offset_x) scr_printf("O");
                    else if (i == 0) scr_printf("|");
                    else scr_printf(".");
                }
                scr_printf("]\n");
            }
            else if (lvl->is_complex && qte_active) {
                scr_printf(" [QTE EVENT] !! PRESS %s NOW !! (Time: %d)   \n",
                           qte_names[qte_target_idx], qte_timer);
            }
            else scr_printf("\n");
        } else scr_printf("\n");

        scr_printf(" Fix Progress: [");
        int bars = fix_progress / 10;
        for (int i = 0; i < 10; i++) {
            if (i < bars) scr_printf("#");
            else scr_printf(".");
        }
        scr_printf("] %d%%\n\n", fix_progress);

        is_sparking = 0;
        for (int y = 5; y < 18; y++) {
            scr_printf("  ");
            for (int x = 5; x < 55; x++) {
                if ((buttons & PAD_CROSS) && abs(cursor_x - lvl->repair_x) <= 2
                    && abs(cursor_y - lvl->repair_y) <= 1) {
                    spark_char = (current_tool == lvl->req_tool) ? '*' : '~';
                    if (abs(x - cursor_x) <= 2 && abs(y - cursor_y) <= 1 && (rand() % 3 == 0)) {
                        is_sparking = 1;
                    }
                }

                if (x == cursor_x && y == cursor_y) scr_printf("+");
                else if (is_sparking && abs(x - cursor_x) <= 2 && abs(y - cursor_y) <= 1 && rand() % 2 == 0)
                    scr_printf("%c", spark_char);
                else if (x == lvl->repair_x && y == lvl->repair_y) scr_printf("X");
                else if (x >= 15 && x <= 45 && y >= 8 && y <= 16) scr_printf("=");
                else scr_printf(" ");
            }
            scr_printf("\n");
        }

        scr_printf("\n Controls: D-Pad=Move | L1/R1=Tool | X=Repair (Hold) | START=Exit\n");

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
                            if (rand() % 15 == 0) {
                                qte_active = 1;
                                qte_target_idx = rand() % 3;
                                qte_timer = 30;
                            }
                            fix_progress += 1;
                        } else {
                            qte_timer--;
                            if (pressed & qte_buttons[qte_target_idx]) {
                                fix_progress += 15;
                                qte_active = 0;
                            } else if (pressed & (PAD_TRIANGLE | PAD_SQUARE | PAD_CIRCLE)) {
                                if (fix_progress > 5) fix_progress -= 5;
                                qte_active = 0;
                            } else if (qte_timer <= 0) {
                                if (fix_progress > 8) fix_progress -= 8;
                                qte_active = 0;
                            }
                        }
                    }
                    else fix_progress += 2;

                    if (fix_progress >= 100) {
                        money += lvl->reward;
                        fix_progress = 0;
                        qte_active = 0;
                        solder_drift_x = 0.0f;
                        current_level++;

                        if (current_level >= 18) {
                            scr_clear();
                            scr_printf("\n\n  CONGRATULATIONS! YOU FIXED ALL CONSOLES!\n");
                            scr_printf("  TOTAL EARNED: $%d\n", money);
                            scr_printf("  YOU ARE THE ULTIMATE RETRO MASTER!\n");
                            /* Пауза 5 сек через задержку */
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

        /* Ограничитель ~30 FPS через простую задержку */
        for (volatile int i = 0; i < 1000000; i++) {}
    }

    return 0;
}
