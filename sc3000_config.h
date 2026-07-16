#ifndef SC3000_CONFIG_H
#define SC3000_CONFIG_H

/* ------------------------------------------------------------------
 * Resolución nativa del TMS9918/9929: 256x192, igual que el MSX1.
 * Reutilizamos el mismo layout de VGA_OFFSET ya probado en el
 * proyecto MSX (VGA32SC3000emul/config.h).
 * ---------------------------------------------------------------- */
#define SC3000_WIDTH   256
#define SC3000_HEIGHT  192

#define VGA_OFFSET_X   128
#define VGA_OFFSET_Y   96

/* ------------------------------------------------------------------
 * Pines de SD por placa. Verificados contra el .ino del proyecto
 * MSX ya probado en tu TTGO VGA32/Olimex: solo el MISO cambia entre
 * placas, el resto son fijos.
 * ---------------------------------------------------------------- */
#define SD_MOSI  12
#define SD_CLK   14
#define SD_CS    13

#define SD_MISO_TTGO    2
#define SD_MISO_OLIMEX  35

/* ------------------------------------------------------------------
 * Reloj de CPU real del SC-3000/SC-3000H:
 * 10.73863 MHz / 3 = 3.579545 MHz (fuente: notas de hardware de
 * Charles MacDonald). Lo dejamos como referencia para el
 * temporizado de Fase 1 (VDP) y Fase 4 (casete).
 * ---------------------------------------------------------------- */
#define SC3000_CPU_CLOCK_HZ   3579545UL

/* Ciclos de Z80 por línea de vídeo (NTSC, 262 líneas/frame, ~59.92Hz):
 * 3579545 / (262 * 59.92) =~ 227.7 -> 228, la aproximación estándar
 * usada por otros emuladores de SG-1000/SC-3000. Se revisará junto
 * con el temporizado exacto del TMS9918 en la Fase 1. */
#define SC3000_CYCLES_PER_LINE  228

#endif /* SC3000_CONFIG_H */
