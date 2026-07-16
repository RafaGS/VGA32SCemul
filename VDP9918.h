/*************************************************************/
/**                        VDP9918.h                        **/
/**                                                          **/
/** VDP TMS9918A/TMS9929A real del SC-3000: 16KB de VRAM,    **/
/** modos Graphics I, Graphics II, Text y sprites. Fórmulas  **/
/** de direccionamiento verificadas contra el SetScreen() ya **/
/** probado de MSX.c (mismo chip base que el MSX1).          **/
/**                                                          **/
/** Modo Multicolor: no implementado (igual que en tu propio **/
/** port MSX, donde Screen 3 tampoco está hecho). Es un modo **/
/** raramente usado en software real; se deja como hueco     **/
/** conocido para una fase futura si hace falta.             **/
/*************************************************************/
#ifndef VDP9918_H
#define VDP9918_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BYTE_TYPE_DEFINED
#define BYTE_TYPE_DEFINED
typedef unsigned char byte;
#endif

#define VDP9918_VRAM_SIZE     0x4000
#define VDP9918_WIDTH         256
#define VDP9918_HEIGHT        192
#define VDP9918_ACTIVE_LINES  192
#define VDP9918_TOTAL_LINES   262   /* NTSC */

#define VDP_MODE_GRAPHICS1   0
#define VDP_MODE_TEXT        1
#define VDP_MODE_GRAPHICS2   2
#define VDP_MODE_MULTICOLOR  3   /* Detectado pero no dibujado (ver nota arriba) */

extern byte VDPReg[8];                     /* VR0-VR7 */
extern byte VRAM[VDP9918_VRAM_SIZE];
extern const byte VDPPalette[16][3];        /* Paleta fija real del TMS9918 (RGB) */

/** API llamada desde SC3000.c (InZ80/OutZ80/LoopZ80) ***********/
void ResetVDP9918(void);
byte RdDataVDP(void);
void WrDataVDP(byte Value);
byte RdCtrlVDP(void);
void WrCtrlVDP(byte Value);
void VDPEndOfScanline(void);
int  VDPWantsIRQ(void);

/** Accesores de tablas/registro, para uso de la capa de plataforma */
byte  VDPGetMode(void);
byte *VDPNameTable(void);
byte *VDPPatternTable(void);       /* Graphics I / Text */
byte *VDPColorTable(void);         /* Graphics I */
byte *VDPPatternTableG2(void);     /* Graphics II: base 0x0000 u 0x2000 */
byte *VDPColorTableG2(void);       /* Graphics II: base 0x0000 u 0x2000 */
byte *VDPSpriteAttrTable(void);
byte *VDPSpritePatternTable(void);
byte  VDPBackdropColor(void);
int   VDPSpriteSize16(void);
int   VDPSpriteMag(void);
int   VDPDisplayEnabled(void);     /* Bit BL de VR1 */

/** Ganchos que debe implementar la capa de plataforma (FabGL).  **/
/** Ver ESP32_VDP.cpp.                                            **/
void VDPPlatformInit(void);
void VDPPlatformTrash(void);
void RefreshLineText(byte Y);
void RefreshLineGraphics1(byte Y);
void RefreshLineGraphics2(byte Y);
void ClearLineVDP(byte Y);
void DrawSpritesLine(byte Y, byte *LineBuffer);
void RefreshScreenVDP(void);

#ifdef __cplusplus
}
#endif
#endif /* VDP9918_H */
