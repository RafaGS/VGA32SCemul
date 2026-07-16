/*************************************************************/
/**                         SC3000.h                        **/
/**                                                          **/
/** Núcleo de la máquina SC-3000. A diferencia de MSX.c, no  **/
/** hay slots, mappers, ni BIOS: el cartucho ocupa            **/
/** directamente $0000-$7FFF y los 32KB de RAM de la          **/
/** expansión Basic Level III ocupan $8000-$FFFF, sin reflejo. **/
/*************************************************************/
#ifndef SC3000_H
#define SC3000_H

#include "Z80.h"
#include "I8255.h"

#ifdef __cplusplus
extern "C" {
#endif

/* $0000-$7FFF: contenido del cartucho (32KB máx), siempre ROM.
 * $8000-$FFFF: 32KB de RAM lineal, SIN reflejo.
 *
 * Este es el mapa de un SC-3000 con la expansión BASIC Level III
 * (32KB de RAM en el propio cartucho, ocupando la mitad superior del
 * espacio de direcciones). Sustituye al mapa "solo cartucho de hasta
 * 48KB + 2KB de RAM reflejada" de la Fase 0/1: se cambió porque la
 * ROM de diagnóstico de prueba (`sc3000.rom`) verifica memoria en
 * todo el rango $8000-$FFFF como un bloque contiguo de 32KB, lo que
 * solo tiene sentido con esta expansión presente.
 *
 * Cartuchos de juego "a secas" de más de 32KB (que en un SC-3000
 * base ocuparían hasta $BFFF sin RAM extra) ya no encajan con este
 * mapa tal cual — si hace falta volver a ese caso, es cuestión de
 * ajustar estas dos constantes y el corte en Rd/WrZ80. */
#define SC3000_CART_SIZE  0x8000
#define SC3000_RAM_SIZE   0x8000

extern Z80    CPU;
extern I8255  PPI;
extern byte   CartROM[SC3000_CART_SIZE];
extern byte   WorkRAM[SC3000_RAM_SIZE];
extern int    CartSize;      /* Tamaño real cargado, para rellenar el resto a 0xFF */

/** InitMachine()/TrashMachine() *********************************/
/** Reservan/liberan los recursos de la máquina (PSRAM, VDP,     **/
/** PSG, PPI). No cargan ningún cartucho todavía.                **/
/*****************************************************************/
int  InitMachine(void);
void TrashMachine(void);

/** LoadCartridge() ***********************************************/
/** Carga un volcado de cartucho (.sg/.sc/.bin/.rom) desde la    **/
/** SD a CartROM[]. Devuelve 1 si ok, 0 si error.                **/
/*****************************************************************/
int LoadCartridge(const char *FileName);

/** ResetSC3000() ***************************************************/
/** Resetea CPU, VDP, PSG y PPI a su estado de encendido.           **/
/*****************************************************************/
void ResetSC3000(void);

/** StartSC3000() *****************************************************/
/** Arranca la ejecución (RunZ80) hasta que LoopZ80() devuelva      **/
/** INT_QUIT. Punto de entrada llamado desde el .ino.               **/
/*****************************************************************/
int StartSC3000(void);

#ifdef __cplusplus
}
#endif
#endif /* SC3000_H */
