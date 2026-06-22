#include "tui.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>

typedef struct {
    HANDLE h_stdin;
    HANDLE h_stdout;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    int msg_count;
    int user_count;
    char msg_lines[256][512];
} tui_priv_t;

int tui_init(tui_t* tui) {
    tui_priv_t* p = (tui_priv_t*)calloc(1, sizeof(tui_priv_t));
    tui->priv = p;
    tui->running = true;

    p->h_stdin = GetStdHandle(STD_INPUT_HANDLE);
    p->h_stdout = GetStdHandle(STD_OUTPUT_HANDLE);

    GetConsoleScreenBufferInfo(p->h_stdout, &p->csbi);

    DWORD mode;
    GetConsoleMode(p->h_stdout, &mode);
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(p->h_stdout, mode);

    GetConsoleMode(p->h_stdin, &mode);
    mode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);
    SetConsoleMode(p->h_stdin, mode);

    printf("\033[2J\033[H");
    printf("=== Meshtalk v1.0 ===\r\n");
    printf("/help for commands\r\n\r\n");
    fflush(stdout);

    return 0;
}

void tui_cleanup(tui_t* tui) {
    if (!tui || !tui->priv) return;
    printf("\033[2J\033[H");
    fflush(stdout);
    free(tui->priv);
    tui->priv = NULL;
}

void tui_draw(tui_t* tui) { (void)tui; }

void tui_add_message(tui_t* tui, const char* sender, const char* text) {
    if (!tui || !tui->priv) return;
    tui_priv_t* p = (tui_priv_t*)tui->priv;

    if (p->msg_count < 256) {
        if (sender && *sender)
            snprintf(p->msg_lines[p->msg_count], sizeof(p->msg_lines[0]),
                     "<%s> %s", sender, text ? text : "");
        else
            snprintf(p->msg_lines[p->msg_count], sizeof(p->msg_lines[0]),
                     "%s", text ? text : "");
        p->msg_count++;
    }

    if (sender && *sender) printf("<%s> ", sender);
    printf("%s\r\n", text ? text : "");
    fflush(stdout);
}

void tui_set_status(tui_t* tui, const char* status) {
    (void)tui;
    printf("\033[2K\r[%s]\r\n", status ? status : "");
    fflush(stdout);
}

void tui_update_users(tui_t* tui, const char* users, int num_users) {
    (void)tui;
    (void)users;
    (void)num_users;
}

int tui_get_input(tui_t* tui, char* buf, size_t size) {
    (void)tui;
    printf("> ");
    fflush(stdout);

    DWORD events = 0;
    GetNumberOfConsoleInputEvents(GetStdHandle(STD_INPUT_HANDLE), &events);
    if (events == 0) return 0;

    INPUT_RECORD rec[128];
    DWORD read_count = 0;
    ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), rec, 128, &read_count);

    size_t pos = 0;
    for (DWORD i = 0; i < read_count && pos < size - 1; i++) {
        if (rec[i].EventType == KEY_EVENT && rec[i].Event.KeyEvent.bKeyDown) {
            char c = rec[i].Event.KeyEvent.uChar.AsciiChar;
            if (c == '\r') break;
            if (c == '\b' && pos > 0) { pos--; }
            else if (c >= 32) { buf[pos++] = c; }
        }
    }
    buf[pos] = '\0';
    return (int)pos;
}

#else
/* Not Windows - stub */
int tui_init(tui_t* tui) { (void)tui; return -1; }
void tui_cleanup(tui_t* tui) { (void)tui; }
void tui_draw(tui_t* tui) { (void)tui; }
void tui_add_message(tui_t* tui, const char* s, const char* t) { (void)tui; (void)s; (void)t; }
void tui_set_status(tui_t* tui, const char* s) { (void)tui; (void)s; }
void tui_update_users(tui_t* tui, const char* u, int n) { (void)tui; (void)u; (void)n; }
int tui_get_input(tui_t* tui, char* b, size_t s) { (void)tui; (void)b; (void)s; return -1; }
#endif
