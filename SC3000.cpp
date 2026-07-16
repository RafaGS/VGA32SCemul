/*************************************************************/
/**                        SC3000.cpp                       **/
/**                                                         **/
/** Ver SC3000.h. Este archivo sustituye por completo al    **/
/** MSX.c original: no hay slots ni mapper, así que el mapa **/
/** de memoria y de puertos cabe en unas pocas decenas de   **/
/** líneas en vez de las ~3000 de MSX.c.                    **/
/*************************************************************/
#include "SC3000.h"
#include "VDP9918.h"
#include "SN76489.h"
#include "sc3000_config.h"
#include <string.h>
#include <stdio.h>
#include "fabgl.h"

/* ---- Wrappers de archivo (implementados en ESP32_FileIO.cpp) ---- */
extern "C" {
    extern void* arduino_fopen(const char* filename, const char* mode);
    extern int   arduino_fclose(void* stream);
    extern size_t arduino_fread(void* ptr, size_t size, size_t nmemb, void* stream);
    extern long  arduino_ftell(void* stream);
    extern int   arduino_feof(void* stream);
}

extern fabgl::PS2Controller PS2Controller;

Z80    CPU;
I8255  PPI;
byte   CartROM[SC3000_CART_SIZE];
byte   WorkRAM[SC3000_RAM_SIZE];
int    CartSize = 0;

// Usaremos enteros de 16 bits: 
// Los bits 0-7 representarán el Puerto A, y los bits 8-11 representarán el Puerto B (bits 0-3).
// Como un 1 significa pulsado, inicializamos todo a 0.
uint16_t KeyState[8] = { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF };

struct KeyMapEntry {
    fabgl::VirtualKey vk;
    uint8_t row;     // Fila (0 a 7)
    uint16_t mask;   // Máscara (0x0001 a 0x0080 para Puerto A, 0x0100 a 0x0800 para Puerto B)
};

static constexpr KeyMapEntry KeyMap[] = {
    // --- FILA 0 ---
    { fabgl::VK_1, 0, 0x0001 }, { fabgl::VK_EXCLAIM, 0, 0x0001 },
    { fabgl::VK_q, 0, 0x0002 }, { fabgl::VK_Q, 0, 0x0002 },
    { fabgl::VK_a, 0, 0x0004 }, { fabgl::VK_A, 0, 0x0004 },
    { fabgl::VK_z, 0, 0x0008 }, { fabgl::VK_Z, 0, 0x0008 },
    { fabgl::VK_GRAVEACCENT, 0, 0x0010 }, // ENG DIER'S
    { fabgl::VK_COMMA, 0, 0x0020 }, { fabgl::VK_LESS, 0, 0x0020 },
    { fabgl::VK_k, 0, 0x0040 }, { fabgl::VK_K, 0, 0x0040 },
    { fabgl::VK_i, 0, 0x0080 }, { fabgl::VK_I, 0, 0x0080 },
    { fabgl::VK_8, 0, 0x0100 }, { fabgl::VK_LEFTPAREN, 0, 0x0100 }, // Port B D0

    // --- FILA 1 ---
    { fabgl::VK_2, 1, 0x0001 }, { fabgl::VK_QUOTEDBL, 1, 0x0001 },
    { fabgl::VK_w, 1, 0x0002 }, { fabgl::VK_W, 1, 0x0002 },
    { fabgl::VK_s, 1, 0x0004 }, { fabgl::VK_S, 1, 0x0004 },
    { fabgl::VK_x, 1, 0x0008 }, { fabgl::VK_X, 1, 0x0008 },
    { fabgl::VK_SPACE, 1, 0x0010 }, 
    { fabgl::VK_PERIOD, 1, 0x0020 }, { fabgl::VK_GREATER, 1, 0x0020 },
    { fabgl::VK_l, 1, 0x0040 }, { fabgl::VK_L, 1, 0x0040 },
    { fabgl::VK_o, 1, 0x0080 }, { fabgl::VK_O, 1, 0x0080 },
    { fabgl::VK_9, 1, 0x0100 }, { fabgl::VK_RIGHTPAREN, 1, 0x0100 }, // Port B D1

    // --- FILA 2 ---
    { fabgl::VK_3, 2, 0x0001 }, { fabgl::VK_HASH, 2, 0x0001 },
    { fabgl::VK_e, 2, 0x0002 }, { fabgl::VK_E, 2, 0x0002 },
    { fabgl::VK_d, 2, 0x0004 }, { fabgl::VK_D, 2, 0x0004 },
    { fabgl::VK_c, 2, 0x0008 }, { fabgl::VK_C, 2, 0x0008 },
    { fabgl::VK_HOME, 2, 0x0010 }, // HOME CLR
    { fabgl::VK_SLASH, 2, 0x0020 }, { fabgl::VK_QUESTION, 2, 0x0020 },
    { fabgl::VK_SEMICOLON, 2, 0x0040 }, { fabgl::VK_PLUS, 2, 0x0040 },
    { fabgl::VK_p, 2, 0x0080 }, { fabgl::VK_P, 2, 0x0080 },
    { fabgl::VK_0, 2, 0x0100 },

    // --- FILA 3 ---
    { fabgl::VK_4, 3, 0x0001 }, { fabgl::VK_DOLLAR, 3, 0x0001 },
    { fabgl::VK_r, 3, 0x0002 }, { fabgl::VK_R, 3, 0x0002 },
    { fabgl::VK_f, 3, 0x0004 }, { fabgl::VK_F, 3, 0x0004 },
    { fabgl::VK_v, 3, 0x0008 }, { fabgl::VK_V, 3, 0x0008 },
    { fabgl::VK_INSERT, 3, 0x0010 }, { fabgl::VK_DELETE, 3, 0x0010 }, { fabgl::VK_BACKSPACE, 3, 0x0010 }, // INS DEL
    { fabgl::VK_BACKSLASH, 3, 0x0020 }, // PI (Arbitrario)
    { fabgl::VK_COLON, 3, 0x0040 }, { fabgl::VK_ASTERISK, 3, 0x0040 },
    { fabgl::VK_AT, 3, 0x0080 }, 
    { fabgl::VK_MINUS, 3, 0x0100 }, { fabgl::VK_EQUALS, 3, 0x0100 },

    // --- FILA 4 ---
    { fabgl::VK_5, 4, 0x0001 }, { fabgl::VK_PERCENT, 4, 0x0001 },
    { fabgl::VK_t, 4, 0x0002 }, { fabgl::VK_T, 4, 0x0002 },
    { fabgl::VK_g, 4, 0x0004 }, { fabgl::VK_G, 4, 0x0004 },
    { fabgl::VK_b, 4, 0x0008 }, { fabgl::VK_B, 4, 0x0008 },
    { fabgl::VK_DOWN, 4, 0x0020 }, // Down arrow
    { fabgl::VK_RIGHTBRACKET, 4, 0x0040 }, { fabgl::VK_LEFTBRACKET, 4, 0x0080 }, { fabgl::VK_CARET, 4, 0x0100 },

    // --- FILA 5 ---
    { fabgl::VK_6, 5, 0x0001 }, { fabgl::VK_AMPERSAND, 5, 0x0001 },
    { fabgl::VK_y, 5, 0x0002 }, { fabgl::VK_Y, 5, 0x0002 },
    { fabgl::VK_h, 5, 0x0004 }, { fabgl::VK_H, 5, 0x0004 },
    { fabgl::VK_n, 5, 0x0008 }, { fabgl::VK_N, 5, 0x0008 },
    { fabgl::VK_LEFT, 5, 0x0020 }, // Left arrow
    { fabgl::VK_RETURN, 5, 0x0040 }, { fabgl::VK_LALT, 5, 0x0800 }, // FUNC (Port B D3)

    // --- FILA 6 ---
    { fabgl::VK_7, 6, 0x0001 }, { fabgl::VK_QUOTE, 6, 0x0001 }, 
    { fabgl::VK_u, 6, 0x0002 }, { fabgl::VK_U, 6, 0x0002 },
    { fabgl::VK_j, 6, 0x0004 }, { fabgl::VK_J, 6, 0x0004 },
    { fabgl::VK_m, 6, 0x0008 }, { fabgl::VK_M, 6, 0x0008 },
    { fabgl::VK_RIGHT, 6, 0x0020 }, // Right arrow
    { fabgl::VK_UP, 6, 0x0040 }, { fabgl::VK_ESCAPE, 6, 0x0100 }, // BREAK (Port B D0)
    { fabgl::VK_RALT, 6, 0x0200 }, // GRAPH (Port B D1)
    { fabgl::VK_LCTRL, 6, 0x0400 }, { fabgl::VK_RCTRL, 6, 0x0400 }, // CTRL (Port B D2)
    { fabgl::VK_LSHIFT, 6, 0x0800 }, { fabgl::VK_RSHIFT, 6, 0x0800 } // SHIFT (Port B D3)
};

static constexpr int KeyMapSize = sizeof(KeyMap) / sizeof(KeyMap[0]);

void ProcessSC3000Key(fabgl::VirtualKey vk, bool down) {
    for (int i = 0; i < KeyMapSize; i++) {
        if (KeyMap[i].vk == vk) {
            if (down) {
                KeyState[KeyMap[i].row] &= ~KeyMap[i].mask;  // Poner bit a 1 (Pulsado)
            } else {
                KeyState[KeyMap[i].row] |= KeyMap[i].mask; // Poner bit a 0 (Soltado)
            }
        }
    }
}

void UpdateKeyboardState() {
    fabgl::Keyboard *kb = PS2Controller.keyboard();
    if (!kb) return;

    while (kb->virtualKeyAvailable()) {
        bool down;
        fabgl::VirtualKey vk = kb->getNextVirtualKey(&down);
        ProcessSC3000Key(vk, down);
    }
}

/* ------------------------------------------------------------------
 * Mapa de memoria: sin slots. $0000-$7FFF = cartucho (ROM, 32KB),
 * $8000-$FFFF = 32KB de RAM lineal sin reflejo (expansión Basic
 * Level III). Ver la nota en SC3000.h sobre por qué se cambió desde
 * el mapa "solo cartucho + 2KB reflejados" de la Fase 0/1.
 * ---------------------------------------------------------------- */
extern "C" byte RdZ80(Z80_WORD A)
{
    if (A < SC3000_CART_SIZE) return CartROM[A];
    return WorkRAM[A - SC3000_CART_SIZE];
}

extern "C" void WrZ80(Z80_WORD A, byte V)
{
    /* $0000-$7FFF es siempre ROM del cartucho; $8000-$FFFF es RAM
     * lineal de 32KB (expansión Basic Level III), sin reflejo. */
    if (A >= SC3000_CART_SIZE) WorkRAM[A - SC3000_CART_SIZE] = V;
}

/* ------------------------------------------------------------------
 * Mapa de puertos real del SC-3000 (decodificación parcial de
 * dirección, no un decodificador completo de 8 bits). Cada bloque
 * de 0x20 puertos habilita una combinación distinta de PPI/VDP/PSG.
 * Fuente: notas de hardware de Charles MacDonald, sección
 * "Z80 port map".
 * ---------------------------------------------------------------- */
typedef struct { byte ppi, vdp, psg; } TPortBlock;

static const TPortBlock PortMap[8] =
{
    /* $00-1F */ { 1, 1, 1 },
    /* $20-3F */ { 0, 1, 1 },
    /* $40-5F */ { 1, 0, 1 },
    /* $60-7F */ { 0, 0, 1 },
    /* $80-9F */ { 1, 1, 0 },
    /* $A0-BF */ { 0, 1, 0 },
    /* $C0-DF */ { 1, 0, 0 },
    /* $E0-FF */ { 0, 0, 0 }
};

extern "C" byte InZ80(Z80_WORD Port)
{
    byte P = (byte)Port;
    const TPortBlock *B = &PortMap[P >> 5];

    /* Cuando PPI y VDP están habilitados a la vez, el hardware real
     * devuelve el valor del PPI con algunos bits corrompidos (por el
     * VDP intentando escribir el bus al mismo tiempo). Aproximamos
     * devolviendo directamente el valor del PPI, que es lo que
     * importa para la compatibilidad con el software real. */
    if (B->ppi)
    {
        // El Puerto C (Rout[2]) selecciona la fila activa en los bits 0-2
        byte selectedRow = PPI.Rout[2] & 0x07; 
        
        // Asignamos el estado de las teclas de la fila seleccionada a los puertos de entrada
        PPI.Rin[0] = (byte)(KeyState[selectedRow] & 0xFF);         // Puerto A (bits 0-7)
        
        // Puerto B: El bit 4 devuelve 1, bits 5-7 devuelven 0-0-1 según el arranque.
        // Combinamos el estado de las teclas del Puerto B (bits 8-11 del uint16) con los valores fijos.
        PPI.Rin[1] = (byte)((KeyState[selectedRow] >> 8) & 0x0F) | 0x90; 

        return Read8255(&PPI, P & 0x03);
    }    
    if (B->vdp) return (P & 0x01) ? RdCtrlVDP() : RdDataVDP();

    /* Ni PPI ni VDP habilitados ($60-7F, $E0-FF): en el hardware real
     * el bus devuelve un byte del cartucho indexado por el registro R
     * del Z80. No lo modelamos (el PSG no tiene lectura y ningún
     * software depende de esto para funcionar); aproximamos con bus
     * flotante a 0xFF. */
    return 0xFF;
}

extern "C" void OutZ80(Z80_WORD Port, byte Value)
{
    byte P = (byte)Port;
    const TPortBlock *B = &PortMap[P >> 5];

    /* "For each location, data written goes to all devices that are
     * enabled" (Charles MacDonald): en escritura SÍ puede haber más
     * de un dispositivo activo a la vez y hay que escribir en todos. */
    if (B->ppi) Write8255(&PPI, P & 0x03, Value);
    if (B->vdp) { if (P & 0x01) WrCtrlVDP(Value); else WrDataVDP(Value); }
    if (B->psg) WrPSG(Value);
}

/* ------------------------------------------------------------------
 * PatchZ80(): sin parches de BIOS (el SC-3000 no tiene BIOS que
 * interceptar, todo el arranque vive en el propio cartucho).
 * ---------------------------------------------------------------- */
extern "C" void PatchZ80(Z80 *R) { (void)R; }

/* ------------------------------------------------------------------
 * LoopZ80(): FASE 0 — de momento solo hace avanzar el VDP línea a
 * línea y nunca pide IRQ. En la Fase 1, VDPWantsIRQ() empezará a
 * devolver 1 en VBlank; basta con devolver INT_IRQ aquí, RunZ80()
 * ya se encarga de llamar a IntZ80() internamente con ese vector.
 * ---------------------------------------------------------------- */
extern "C" Z80_WORD LoopZ80(Z80 *R)
{
    (void)R;

    // Dar tiempo a las tareas de FreeRTOS/FabGL
    yield();

    // Actualizar la matriz del teclado con las teclas físicas pulsadas
    UpdateKeyboardState();

    VDPEndOfScanline();

    if (VDPWantsIRQ())
        return INT_IRQ;

    return INT_NONE;
}

/* ------------------------------------------------------------------ */
int InitMachine(void)
{
    memset(CartROM, 0xFF, sizeof(CartROM));
    memset(WorkRAM, 0x00, sizeof(WorkRAM));

    VDPPlatformInit();   /* Asigna framebuffer/paleta UNA sola vez */
    ResetVDP9918();
    ResetPSG();
    Reset8255(&PPI);

    /* IMPORTANTE: ResetZ80() calcula ICount=IPeriod, así que IPeriod
     * hay que fijarlo ANTES de resetear la CPU, no después. */
    CPU.IPeriod = SC3000_CYCLES_PER_LINE;
    ResetZ80(&CPU);
    return 1;
}

void TrashMachine(void)
{
    VDPPlatformTrash();
}

void ResetSC3000(void)
{
    ResetVDP9918();
    ResetPSG();
    Reset8255(&PPI);
    ResetZ80(&CPU);
}

int LoadCartridge(const char *FileName)
{
    void *f = arduino_fopen(FileName, "rb");
    if (!f) return 0;

    memset(CartROM, 0xFF, sizeof(CartROM));
    CartSize = (int)arduino_fread(CartROM, 1, SC3000_CART_SIZE, f);
    arduino_fclose(f);

    return CartSize > 0;
}

int StartSC3000(void)
{
    ResetSC3000();
    RunZ80(&CPU);
    return 1;
}
