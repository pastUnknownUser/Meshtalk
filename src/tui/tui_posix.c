#include "tui.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>

#ifdef HAVE_NCURSES
#include <ncurses.h>
#include <term.h>

typedef struct {
    WINDOW* msg_win;
    WINDOW* user_win;
    WINDOW* input_win;
    WINDOW* status_win;
    int msg_rows;
    int user_rows;
    int user_width;
    int input_row;
    int input_col;

    /* Non-blocking input buffer */
    char input_buf[MAX_LINE_LEN];
    int input_len;
} tui_priv_t;

static void tui_resize(tui_t* tui) {
    tui_priv_t* p = (tui_priv_t*)tui->priv;
    int maxy, maxx;
    getmaxyx(stdscr, maxy, maxx);

    p->user_width = 20;
    p->input_row = maxy - 2;
    p->msg_rows = maxy - 3;
    p->user_rows = maxy - 3;
    p->input_col = 0;

    if (p->msg_win) delwin(p->msg_win);
    if (p->user_win) delwin(p->user_win);
    if (p->input_win) delwin(p->input_win);
    if (p->status_win) delwin(p->status_win);

    p->msg_win = newwin(p->msg_rows, maxx - p->user_width, 0, 0);
    p->user_win = newwin(p->user_rows, p->user_width, 0, maxx - p->user_width);
    p->input_win = newwin(1, maxx, p->input_row, 0);
    p->status_win = newwin(1, maxx, maxy - 1, 0);

    scrollok(p->msg_win, TRUE);
    scrollok(p->user_win, TRUE);
    wrefresh(p->msg_win);
    wrefresh(p->user_win);
    wrefresh(p->input_win);
    wrefresh(p->status_win);

    tui->width = maxx;
    tui->height = maxy;
}

int tui_init(tui_t* tui) {
    tui_priv_t* p = (tui_priv_t*)calloc(1, sizeof(tui_priv_t));
    tui->priv = p;
    tui->running = true;
    p->input_len = 0;
    p->input_buf[0] = '\0';

    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);

    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_CYAN, COLOR_BLACK);
        init_pair(2, COLOR_GREEN, COLOR_BLACK);
        init_pair(3, COLOR_YELLOW, COLOR_BLACK);
    }

    tui_resize(tui);

    wbkgd(p->status_win, A_REVERSE);
    mvwprintw(p->user_win, 0, 1, "Online Users");
    mvwprintw(p->status_win, 0, 0, " Meshtalk v1.0 | /help for commands ");
    wrefresh(p->status_win);
    wrefresh(p->user_win);

    return 0;
}

void tui_cleanup(tui_t* tui) {
    if (!tui || !tui->priv) return;
    tui_priv_t* p = (tui_priv_t*)tui->priv;
    if (p->msg_win) delwin(p->msg_win);
    if (p->user_win) delwin(p->user_win);
    if (p->input_win) delwin(p->input_win);
    if (p->status_win) delwin(p->status_win);
    endwin();
    free(p);
    tui->priv = NULL;
}

void tui_draw(tui_t* tui) { (void)tui; }

void tui_add_message(tui_t* tui, const char* sender, const char* text) {
    if (!tui || !tui->priv) return;
    tui_priv_t* p = (tui_priv_t*)tui->priv;
    (void)p;
    int maxx;
    getmaxyx(stdscr, maxx, maxx);

    if (sender && *sender) {
        wattron(p->msg_win, A_BOLD | COLOR_PAIR(1));
        wprintw(p->msg_win, "<%s> ", sender);
        wattroff(p->msg_win, A_BOLD | COLOR_PAIR(1));
    }
    wprintw(p->msg_win, "%s", text ? text : "");
    wprintw(p->msg_win, "\n");
    wrefresh(p->msg_win);
}

void tui_set_status(tui_t* tui, const char* status) {
    if (!tui || !tui->priv) return;
    tui_priv_t* p = (tui_priv_t*)tui->priv;
    int maxx;
    getmaxyx(stdscr, maxx, maxx);
    werase(p->status_win);
    mvwprintw(p->status_win, 0, 0, " %s", status ? status : "");
    wrefresh(p->status_win);
}

void tui_update_users(tui_t* tui, const char* users, int num_users) {
    if (!tui || !tui->priv) return;
    tui_priv_t* p = (tui_priv_t*)tui->priv;
    int maxy, maxx;
    getmaxyx(stdscr, maxy, maxx);

    werase(p->user_win);
    box(p->user_win, 0, 0);
    mvwprintw(p->user_win, 0, 1, "Users (%d)", num_users);

    const char* cur = users;
    int row = 1;
    while (cur && *cur && row < maxy - 2) {
        mvwprintw(p->user_win, row, 1, "%s", cur);
        row++;
        cur = strchr(cur, '\0');
        if (cur) cur++;
    }
    wrefresh(p->user_win);
}

static void redraw_input(tui_priv_t* p) {
    int maxx;
    getmaxyx(stdscr, maxx, maxx);
    werase(p->input_win);
    mvwprintw(p->input_win, 0, 0, "> %s", p->input_buf);
    wclrtoeol(p->input_win);
    wmove(p->input_win, 0, (int)strlen(p->input_buf) + 2);
    wrefresh(p->input_win);
}

int tui_get_input(tui_t* tui, char* buf, size_t size) {
    if (!tui || !tui->priv) return -1;
    tui_priv_t* p = (tui_priv_t*)tui->priv;

    int ch = getch();
    if (ch == ERR) return 0;

    if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
        int len = p->input_len;
        if (len > 0 && (size_t)len < size) {
            memcpy(buf, p->input_buf, (size_t)len);
            buf[len] = '\0';
        }
        p->input_len = 0;
        p->input_buf[0] = '\0';
        redraw_input(p);
        return len > 0 && (size_t)len < size ? len : 0;
    }

    if (ch == 127 || ch == KEY_BACKSPACE || ch == '\b') {
        if (p->input_len > 0) {
            p->input_len--;
            p->input_buf[p->input_len] = '\0';
        }
        redraw_input(p);
        return 0;
    }

    if (ch >= 32 && ch <= 126 && p->input_len < (int)size - 2) {
        p->input_buf[p->input_len++] = (char)ch;
        p->input_buf[p->input_len] = '\0';
        redraw_input(p);
    }

    return 0;
}

#else
/* Fallback for systems without ncurses — non-blocking terminal I/O */

#include <termios.h>
#include <unistd.h>
#include <sys/select.h>

typedef struct {
    char input_buf[MAX_LINE_LEN];
    int input_len;
} tui_priv_t;

static struct termios oldt;

int tui_init(tui_t* tui) {
    tui_priv_t* p = (tui_priv_t*)calloc(1, sizeof(tui_priv_t));
    tui->priv = p;
    tui->running = true;
    p->input_len = 0;
    p->input_buf[0] = '\0';

    struct termios t;
    tcgetattr(STDIN_FILENO, &oldt);
    t = oldt;
    t.c_lflag &= ~(ICANON | ECHO);
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 1;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);

    printf("\033[2J\033[H");
    printf("=== Meshtalk v1.0 ===\n");
    printf("/help for commands\n\n");
    fflush(stdout);

    return 0;
}

void tui_cleanup(tui_t* tui) {
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    printf("\033[2J\033[H");
    fflush(stdout);
    free(tui->priv);
    tui->priv = NULL;
}

void tui_draw(tui_t* tui) { (void)tui; }

void tui_add_message(tui_t* tui, const char* sender, const char* text) {
    (void)tui;
    printf("\033[2K\r");
    if (sender && *sender) printf("<%s> ", sender);
    printf("%s\n> %s", text ? text : "", ((tui_priv_t*)tui->priv)->input_buf);
    fflush(stdout);
}

void tui_set_status(tui_t* tui, const char* status) {
    (void)tui;
    printf("\033[2K\r[%s]\n", status ? status : "");
    fflush(stdout);
}

void tui_update_users(tui_t* tui, const char* users, int num_users) {
    (void)tui; (void)users; (void)num_users;
}

int tui_get_input(tui_t* tui, char* buf, size_t size) {
    if (!tui || !tui->priv) return -1;
    tui_priv_t* p = (tui_priv_t*)tui->priv;

    char c;
    if (read(STDIN_FILENO, &c, 1) != 1) return 0;

    if (c == '\n' || c == '\r') {
        int len = p->input_len;
        if (len > 0 && (size_t)len < size) {
            memcpy(buf, p->input_buf, (size_t)len);
            buf[len] = '\0';
        }
        p->input_len = 0;
        p->input_buf[0] = '\0';
        return len > 0 && (size_t)len < size ? len : 0;
    }

    if (c == 127 || c == '\b') {
        if (p->input_len > 0) {
            p->input_len--;
            p->input_buf[p->input_len] = '\0';
            printf("\b \b");
            fflush(stdout);
        }
        return 0;
    }

    if (c >= 32 && c <= 126 && p->input_len < (int)size - 2) {
        p->input_buf[p->input_len++] = c;
        p->input_buf[p->input_len] = '\0';
        putchar(c);
        fflush(stdout);
    }

    return 0;
}
#endif
