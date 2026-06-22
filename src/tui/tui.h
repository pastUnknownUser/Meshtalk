#pragma once

#include "../common.h"

typedef struct {
    int width;
    int height;
    volatile bool running;
    void* priv;
} tui_t;

int  tui_init(tui_t* tui);
void tui_cleanup(tui_t* tui);

void tui_draw(tui_t* tui);
void tui_add_message(tui_t* tui, const char* sender, const char* text);
void tui_set_status(tui_t* tui, const char* status);
void tui_update_users(tui_t* tui, const char* users, int num_users);

int  tui_get_input(tui_t* tui, char* buf, size_t size);


