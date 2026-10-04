// Menu de entrada y de pausa, con los motivos del arte de la estepa (estelas,
// placas de oro con ciervos de astas de ave, grifos, lobos enroscados):
//  - titulo: nueva partida, cargar partida, instructivo, salir;
//  - cargar / guardar: tres huecos con minifoto, fecha, dia de juego y tribu;
//  - instructivo: laminas con los controles y las reglas del juego;
//  - pausa (Esc en el juego): continuar, guardar, cargar, instructivo, al titulo.
#ifndef ESTEPA_TITLE_MENU_H
#define ESTEPA_TITLE_MENU_H

#include <stdbool.h>

#include "game/save_game.h"
#include "raylib.h"

typedef enum { MENU_HIDDEN, MENU_TITLE, MENU_PAUSE, MENU_LOAD, MENU_SAVE, MENU_HELP } MenuScreen;

typedef enum {
    MENU_NONE,
    MENU_NEW_GAME,
    MENU_CONTINUE,  // volver al juego
    MENU_LOAD_SLOT, // cargar el hueco slot
    MENU_SAVE_SLOT, // guardar en el hueco slot
    MENU_TO_TITLE,
    MENU_QUIT,
    MENU_LANG, // cambiar el idioma de la interfaz
} MenuAction;

#define MENU_PLATES 7

#define GAME_TITLE "SCAVENGERS THRIVE"
#define GAME_SUBTITLE "THEY COME FROM THE STEPPES"

typedef struct {
    MenuScreen screen, back; // pantalla actual y a la que vuelve con Esc
    int cursor, page;
    int slot;                // hueco elegido (MENU_LOAD_SLOT / MENU_SAVE_SLOT)
    SaveInfo slots[SAVE_SLOTS];
    bool slots_loaded;
    char message[128];
    float message_timer;
    // Arte.
    Texture2D background, emblem, plates[MENU_PLATES];
    float time;
    int w, h; // tamaño de la ultima pantalla dibujada (para el raton)
} TitleMenu;

void menu_init(TitleMenu *m);
void menu_unload(TitleMenu *m);
void menu_open(TitleMenu *m, MenuScreen s);
bool menu_visible(const TitleMenu *m);
void menu_message(TitleMenu *m, const char *text);
// Teclado: flechas, Enter, Esc. Devuelve lo que el juego tiene que hacer.
MenuAction menu_update(TitleMenu *m, float dt);
// in_game: hay una partida debajo (se dibuja oscurecida); version: texto del pie.
void menu_draw(TitleMenu *m, bool in_game, const char *version, int w, int h);
// Las ayudas de controles (tambien para F1 en el juego).
void menu_draw_controls(int x, int y, int w, int h);

#endif
