/*************************************************************/
/**                      ESP32_VDP.cpp                      **/
/**                                                          **/
/** Capa de plataforma (FabGL) para el VDP TMS9918/9929.     **/
/** VDP9918.c (agnóstico) llama a estas funciones; aquí es   **/
/** donde vive todo lo específico de FabGL/VGA16Controller,  **/
/** siguiendo el mismo patrón ya probado en el proyecto MSX  **/
/** (ESP32_Port.cpp: framebuffer XBuf en PSRAM + fabgl::Bitmap **/
/** + Canvas->drawBitmap()).                                  **/
/*************************************************************/
#include "fabgl.h"
#include <Arduino.h>
#include <string.h>
#include "sc3000_config.h"
#include "VDP9918.h"

extern fabgl::VGA16Controller VGAController;
extern fabgl::Canvas          *Canvas;

static fabgl::VGA16Controller *vga16 = nullptr;
static inline fabgl::VGA16Controller* ensureVga16() {
    if (!vga16) vga16 = &VGAController;
    return vga16;
}

static uint8_t        *XBuf = nullptr;      /* 1 byte/píxel, índice de paleta 0-15 */
static fabgl::Bitmap   *vdpScreen = nullptr;

extern "C" void VDPPlatformInit(void)
{
    if (!ensureVga16()) return;

    for (int i = 0; i < 16; i++) {
        fabgl::RGB888 color;
        color.R = VDPPalette[i][0];
        color.G = VDPPalette[i][1];
        color.B = VDPPalette[i][2];
        vga16->setPaletteItem(i, color);
    }

    size_t bufSize = (size_t)VDP9918_WIDTH * VDP9918_HEIGHT;
    XBuf = (uint8_t*)ps_malloc(bufSize);
    if (!XBuf) {
        Serial.println("ERROR: no se pudo reservar el framebuffer del VDP en PSRAM");
        return;
    }
    memset(XBuf, 0, bufSize);

    vdpScreen = new fabgl::Bitmap(VDP9918_WIDTH, VDP9918_HEIGHT, XBuf,
                                   fabgl::PixelFormat::Native, false);
}

extern "C" void VDPPlatformTrash(void)
{
    if (vdpScreen) { delete vdpScreen; vdpScreen = nullptr; }
    if (XBuf)      { free(XBuf); XBuf = nullptr; }
}

extern "C" void RefreshScreenVDP(void)
{
    /* RunZ80() corre para siempre dentro de loop() y nunca vuelve al
     * sistema: sin este yield(), el watchdog de FreeRTOS reiniciaría
     * la placa a los pocos segundos. Este es el punto natural para
     * cederlo, una vez por frame (~16ms). */
    yield();

    static uint32_t FrameCount = 0;

    if (FrameCount == 0) Serial.println("VDP: primer frame renderizado");
    FrameCount++;
    if ((FrameCount % 60) == 0) {
        Serial.printf("VDP: frame %u, backdrop=%d, VR0=%02X VR1=%02X\n",
                       (unsigned)FrameCount, VDPBackdropColor(), VDPReg[0], VDPReg[1]);
    }

    if (Canvas && vdpScreen) Canvas->drawBitmap(VGA_OFFSET_X, VGA_OFFSET_Y, vdpScreen);
}

extern "C" void ClearLineVDP(byte Y)
{
    if (!XBuf || Y >= VDP9918_HEIGHT) return;
    memset(XBuf + Y * VDP9918_WIDTH, VDPBackdropColor(), VDP9918_WIDTH);
}

/* ------------------------------------------------------------------
 * Text mode: 40 columnas x 6px + 8px de borde a cada lado = 256px.
 * Un único par FG/BG para toda la pantalla (VR7), sin tabla de color
 * ni sprites (el TMS9918 real no muestra sprites en este modo).
 * ---------------------------------------------------------------- */
extern "C" void RefreshLineText(byte Y)
{
    if (!XBuf || Y >= VDP9918_HEIGHT) return;
    uint8_t *P = XBuf + Y * VDP9918_WIDTH;
    byte BG = VDPBackdropColor();
    byte FG = VDPReg[7] >> 4;

    memset(P, BG, 8); P += 8;

    byte *NameTab = VDPNameTable();
    byte *PatTab  = VDPPatternTable();
    int Row  = (Y >> 3) * 40;
    int Line = Y & 7;

    for (int X = 0; X < 40; X++) {
        int  CharCode = NameTab[Row + X];
        byte Pattern  = PatTab[CharCode * 8 + Line];
        for (int i = 0; i < 6; i++) {
            *P++ = (Pattern & 0x80) ? FG : BG;
            Pattern <<= 1;
        }
    }
    memset(P, BG, 8);
}

/* ------------------------------------------------------------------
 * Graphics I: 32x24 caracteres de 8x8, color por grupos de 8 chars.
 * ---------------------------------------------------------------- */
extern "C" void RefreshLineGraphics1(byte Y)
{
    if (!XBuf || Y >= VDP9918_HEIGHT) return;
    uint8_t *P = XBuf + Y * VDP9918_WIDTH;

    byte *NameTab = VDPNameTable();
    byte *PatTab  = VDPPatternTable();
    byte *ColTab  = VDPColorTable();
    int  Row  = (Y >> 3) * 32;
    int  Line = Y & 7;
    byte Backdrop = VDPBackdropColor();

    for (int X = 0; X < 32; X++) {
        int  CharCode = NameTab[Row + X];
        byte Pattern  = PatTab[CharCode * 8 + Line];
        byte Color    = ColTab[CharCode >> 3];
        byte FG = Color >> 4;
        byte BG = Color & 0x0F;
        if (FG == 0) FG = Backdrop;
        if (BG == 0) BG = Backdrop;
        for (int i = 0; i < 8; i++) {
            *P++ = (Pattern & 0x80) ? FG : BG;
            Pattern <<= 1;
        }
    }
    DrawSpritesLine(Y, XBuf + Y * VDP9918_WIDTH);
}

/* ------------------------------------------------------------------
 * Graphics II: 32x24 chars, pero con patrón y color propios por cada
 * línea de cada carácter (3 "zonas" de 64 líneas cada una).
 * ---------------------------------------------------------------- */
extern "C" void RefreshLineGraphics2(byte Y)
{
    if (!XBuf || Y >= VDP9918_HEIGHT) return;
    uint8_t *P = XBuf + Y * VDP9918_WIDTH;

    byte *NameTab = VDPNameTable();
    byte *PatTab  = VDPPatternTableG2();
    byte *ColTab  = VDPColorTableG2();
    int  Zone = Y / 64;
    int  Row  = (Y >> 3) * 32;
    int  Line = Y & 7;
    byte Backdrop = VDPBackdropColor();

    for (int X = 0; X < 32; X++) {
        int  CharCode = NameTab[Row + X] + Zone * 256;
        byte Pattern  = PatTab[CharCode * 8 + Line];
        byte Color    = ColTab[CharCode * 8 + Line];
        byte FG = Color >> 4;
        byte BG = Color & 0x0F;
        if (FG == 0) FG = Backdrop;
        if (BG == 0) BG = Backdrop;
        for (int i = 0; i < 8; i++) {
            *P++ = (Pattern & 0x80) ? FG : BG;
            Pattern <<= 1;
        }
    }
    DrawSpritesLine(Y, XBuf + Y * VDP9918_WIDTH);
}

/* ------------------------------------------------------------------
 * Sprites: hasta 32, 8x8 o 16x16, con o sin magnificación x2. No se
 * implementa el límite real de 4 sprites/línea ni la colisión (igual
 * que en tu propio port MSX); es una simplificación deliberada que
 * no afecta a la jugabilidad, solo a un efecto de parpadeo raro.
 * ---------------------------------------------------------------- */
extern "C" void DrawSpritesLine(byte Y, byte *LineBuffer)
{
    int Size = VDPSpriteSize16() ? 16 : 8;
    int Mag  = VDPSpriteMag();
    byte *SprTab = VDPSpriteAttrTable();
    byte *SprGen = VDPSpritePatternTable();

    for (int i = 0; i < 32; i++) {
        byte *Attr = SprTab + i * 4;
        int Sy = Attr[0];
        if (Sy == 208) break;         /* Marca de fin de lista */
        if (Sy > 240) Sy -= 256;
        Sy++;

        int Diff = Y - Sy;
        if (Mag) Diff >>= 1;

        if (Diff >= 0 && Diff < Size) {
            int  Sx     = Attr[1];
            int  PatIdx = Attr[2];
            byte Color  = Attr[3] & 0x0F;
            if (Attr[3] & 0x80) Sx -= 32;
            if (Color == 0) continue;   /* Color 0 = sprite invisible */
            if (Size == 16) PatIdx &= 0xFC;

            byte PatLine = SprGen[PatIdx * 8 + Diff];
            for (int p = 0; p < 8; p++) {
                int DrawX = Sx + (Mag ? p * 2 : p);
                if ((PatLine & 0x80) && DrawX >= 0 && DrawX < VDP9918_WIDTH) {
                    LineBuffer[DrawX] = Color;
                    if (Mag && DrawX + 1 < VDP9918_WIDTH) LineBuffer[DrawX + 1] = Color;
                }
                PatLine <<= 1;
            }
            if (Size == 16) {
                PatLine = SprGen[PatIdx * 8 + Diff + 16];
                for (int p = 0; p < 8; p++) {
                    int DrawX = Sx + (Mag ? (p + 8) * 2 : (p + 8));
                    if ((PatLine & 0x80) && DrawX >= 0 && DrawX < VDP9918_WIDTH) {
                        LineBuffer[DrawX] = Color;
                        if (Mag && DrawX + 1 < VDP9918_WIDTH) LineBuffer[DrawX + 1] = Color;
                    }
                    PatLine <<= 1;
                }
            }
        }
    }
}
