/*************************************************************/
/**                        VDP9918.c                        **/
/**                                                          **/
/** Ver VDP9918.h.                                           **/
/*************************************************************/
#include "VDP9918.h"
#include <string.h>

byte VDPReg[8];
byte VRAM[VDP9918_VRAM_SIZE];

/* Paleta fija real del TMS9918/9929 (no es un registro programable
 * como en el V9938 de MSX2: en el hardware real son 15 colores fijos
 * más "transparente"). Valores RGB de referencia estándar, los mismos
 * que usan MAME/blueMSX y prácticamente todo emulador de TMS9918. */
const byte VDPPalette[16][3] =
{
    {   0,   0,   0 }, /*  0: Transparente (se trata como negro) */
    {   0,   0,   0 }, /*  1: Negro */
    {  33, 200,  66 }, /*  2: Verde medio */
    {  94, 220, 120 }, /*  3: Verde claro */
    {  84,  85, 237 }, /*  4: Azul oscuro */
    { 125, 118, 252 }, /*  5: Azul claro */
    { 212,  82,  77 }, /*  6: Rojo oscuro */
    {  66, 235, 245 }, /*  7: Cian */
    { 252,  85,  84 }, /*  8: Rojo medio */
    { 255, 121, 120 }, /*  9: Rojo claro */
    { 212, 193,  84 }, /* 10: Amarillo oscuro */
    { 230, 206, 128 }, /* 11: Amarillo claro */
    {  33, 176,  59 }, /* 12: Verde oscuro */
    { 201,  91, 186 }, /* 13: Magenta */
    { 204, 204, 204 }, /* 14: Gris */
    { 255, 255, 255 }  /* 15: Blanco */
};

/* Latch de direcciones de 2 escrituras, protocolo real del TMS9918 */
static unsigned short Addr;
static byte           AddrLatch;
static byte           LatchByte;
static byte           ReadBuffer;

static byte Status;         /* Bit7 = F (frame flag / VBlank pendiente) */
static int  ScanLine;        /* 0..VDP9918_TOTAL_LINES-1 */
static byte IRQPulse;         /* Ver VDPWantsIRQ() */

void ResetVDP9918(void)
{
    memset(VRAM, 0, sizeof(VRAM));
    memset(VDPReg, 0, sizeof(VDPReg));
    Addr = 0; AddrLatch = 0; LatchByte = 0; ReadBuffer = 0;
    Status = 0; ScanLine = 0; IRQPulse = 0;
}

byte RdDataVDP(void)
{
    byte V = ReadBuffer;
    ReadBuffer = VRAM[Addr & (VDP9918_VRAM_SIZE - 1)];
    Addr = (Addr + 1) & (VDP9918_VRAM_SIZE - 1);
    AddrLatch = 0;
    return V;
}

void WrDataVDP(byte Value)
{
    VRAM[Addr & (VDP9918_VRAM_SIZE - 1)] = Value;
    ReadBuffer = Value;
    Addr = (Addr + 1) & (VDP9918_VRAM_SIZE - 1);
    AddrLatch = 0;
}

byte RdCtrlVDP(void)
{
    byte V = Status;
    Status &= 0x7F;   /* Leer el estado limpia el flag F (VBlank) */
    AddrLatch = 0;
    return V;
}

void WrCtrlVDP(byte Value)
{
    if (!AddrLatch) { LatchByte = Value; AddrLatch = 1; return; }

    AddrLatch = 0;
    if (Value & 0x80)
    {
        VDPReg[Value & 0x07] = LatchByte;   /* Escritura de registro VR0-VR7 */
    }
    else
    {
        Addr = (unsigned short)(((Value & 0x3F) << 8) | LatchByte);
        if (!(Value & 0x40))
        {
            /* Preparado para lectura: precarga el buffer */
            ReadBuffer = VRAM[Addr & (VDP9918_VRAM_SIZE - 1)];
            Addr = (Addr + 1) & (VDP9918_VRAM_SIZE - 1);
        }
    }
}

/* ------------------------------------------------------------------
 * Modo de pantalla, a partir de M1 (VR1 bit4), M2 (VR1 bit3) y
 * M3 (VR0 bit1). Verificado contra SetScreen() de MSX.c: para el
 * TMS9918 real (sin las extensiones de modo del V9938):
 *   M1=0,M2=0,M3=0 -> Graphics I
 *   M1=0,M2=0,M3=1 -> Graphics II
 *   M1=0,M2=1,M3=0 -> Multicolor
 *   M1=1,M2=0,M3=0 -> Text
 * ---------------------------------------------------------------- */
byte VDPGetMode(void)
{
    byte M1 = (VDPReg[1] & 0x10) ? 1 : 0;
    byte M2 = (VDPReg[1] & 0x08) ? 1 : 0;
    byte M3 = (VDPReg[0] & 0x02) ? 1 : 0;

    if (M1) return VDP_MODE_TEXT;
    if (M2) return VDP_MODE_MULTICOLOR;
    if (M3) return VDP_MODE_GRAPHICS2;
    return VDP_MODE_GRAPHICS1;
}

byte *VDPNameTable(void)     { return VRAM + (((int)(VDPReg[2] & 0x0F)) << 10); }
byte *VDPPatternTable(void)  { return VRAM + (((int)(VDPReg[4] & 0x07)) << 11); }
byte *VDPColorTable(void)    { return VRAM + (((int)VDPReg[3]) << 6); }
byte *VDPPatternTableG2(void){ return VRAM + (((int)(VDPReg[4] & 0x04)) << 11); }
byte *VDPColorTableG2(void)  { return VRAM + (((int)(VDPReg[3] & 0x80)) << 6); }
byte *VDPSpriteAttrTable(void)    { return VRAM + (((int)(VDPReg[5] & 0x7F)) << 7); }
byte *VDPSpritePatternTable(void) { return VRAM + (((int)(VDPReg[6] & 0x07)) << 11); }

byte VDPBackdropColor(void) { return VDPReg[7] & 0x0F; }
int  VDPSpriteSize16(void)  { return (VDPReg[1] & 0x02) ? 1 : 0; }
int  VDPSpriteMag(void)     { return (VDPReg[1] & 0x01) ? 1 : 0; }
int  VDPDisplayEnabled(void){ return (VDPReg[1] & 0x40) ? 1 : 0; }

/* ------------------------------------------------------------------
 * Avance de línea: dibuja la línea activa correspondiente (0-191) y,
 * al llegar al final del área visible, marca VBlank y vuelca el
 * framebuffer completo a la pantalla.
 * ---------------------------------------------------------------- */
void VDPEndOfScanline(void)
{
    IRQPulse = 0;

    if (ScanLine < VDP9918_ACTIVE_LINES)
    {
        if (!VDPDisplayEnabled())
        {
            ClearLineVDP((byte)ScanLine);
        }
        else
        {
            switch (VDPGetMode())
            {
                case VDP_MODE_TEXT:       RefreshLineText((byte)ScanLine); break;
                case VDP_MODE_GRAPHICS1:  RefreshLineGraphics1((byte)ScanLine); break;
                case VDP_MODE_GRAPHICS2:  RefreshLineGraphics2((byte)ScanLine); break;
                default:                  ClearLineVDP((byte)ScanLine); break; /* Multicolor: Fase futura */
            }
        }
    }
    else if (ScanLine == VDP9918_ACTIVE_LINES)
    {
        /* Empieza el VBlank: vuelca el frame completo y marca F */
        RefreshScreenVDP();
        Status |= 0x80;
        if (VDPReg[1] & 0x20) IRQPulse = 1;   /* IE (bit5 de VR1) */
    }

    ScanLine++;
    if (ScanLine >= VDP9918_TOTAL_LINES) ScanLine = 0;
}

int VDPWantsIRQ(void)
{
    return IRQPulse;
}
