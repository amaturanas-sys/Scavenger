// Idioma de la interfaz: español (el del codigo) o inglés.
//
// Cada texto visible se escribe en español dentro de T(texto); con el inglés
// activo, T busca su traduccion en assets/i18n/en.tsv (si falta, queda el
// español). Los formatos (%d, %s...) se traducen enteros y deben conservar
// los mismos especificadores: lo comprueba tools/assets/i18n.py check.
//
// N_(texto) marca un texto que se traduce mas tarde, al mostrarlo (nombres en
// tablas estaticas: acciones, especies, oficios...): se muestra con T(nombre).
//
// Puro C, sin raylib: el nucleo (src/sim) tambien traduce lo que redacta.
#ifndef ESTEPA_LANG_H
#define ESTEPA_LANG_H

#include <stdbool.h>

typedef enum { LANG_ES, LANG_EN, LANG_COUNT } Lang;

void lang_set(Lang l);
Lang lang_get(void);
const char *lang_name(Lang l); // "Español", "English"
const char *lang_code(Lang l); // "es", "en"
// Carga una tabla "español<TAB>traducción" por linea (# comenta; \n y \t escapados).
// Devuelve cuantas entradas leyo. Reemplaza la tabla anterior de ese idioma.
int lang_load(Lang l, const char *tsv_text);
void lang_free(void);
// El texto en el idioma activo (el mismo puntero si es español o falta la traduccion).
const char *tr(const char *es);
int lang_missing(void); // cuantos textos pedidos no tenian traduccion (para pruebas)

#define T(s) tr(s)
#define N_(s) (s)

#endif
