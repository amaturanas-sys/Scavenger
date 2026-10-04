// Dialogos con opciones (el druida, el orfebre, el guardian del campamento):
// quien habla, lo que dice y una cuadricula de opciones con icono. El nombre y el
// detalle de cada opcion salen en la leyenda al pasar por encima (src/ui/icons.h).
// Flechas o raton para elegir, Enter o clic para aceptar, Esc para volver.
#ifndef ESTEPA_DIALOG_H
#define ESTEPA_DIALOG_H

#include <stdbool.h>

#include "ui/icons.h"

#define DLG_OPTIONS 24
#define DLG_BACK (-2) // Esc
#define DLG_NONE (-1)

typedef struct {
    IconId icon;
    char label[64], hint[160];
    char badge[8]; // cifra o grado en la esquina ("II", "x3")
    bool enabled, marked; // marked: ya hecho / elegido antes (se ve distinto)
    int value;            // lo que devuelve al elegirla
} DlgOption;

typedef struct {
    bool open;
    char speaker[48];
    IconId portrait;
    char text[320];
    DlgOption opt[DLG_OPTIONS];
    int n, cursor;
} Dialog;

// Empieza (o rehace) el dialogo; conserva el cursor si sigue en rango.
void dlg_begin(Dialog *d, const char *speaker, IconId portrait, const char *text);
DlgOption *dlg_option(Dialog *d, IconId icon, const char *label, const char *hint, bool enabled, int value);
void dlg_close(Dialog *d);
// Teclado y raton: devuelve el value de la opcion aceptada, DLG_BACK con Esc, o DLG_NONE.
int dlg_update(Dialog *d);
void dlg_draw(const Dialog *d, int w, int h);

#endif
