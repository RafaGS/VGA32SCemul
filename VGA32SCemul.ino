/*
 * Emulador SC-3000 para placas FabGL (TTGO VGA32 / Olimex FabGL)
 * Fase 0: esqueleto — Z80 + mapa de memoria/puertos real del
 * SC-3000, VDP y PSG en stub. Sin vídeo ni sonido todavía.
 *
 * Mantenido por RafaG
 * Basado en el port MSX/FabGL propio (VGA32SC3000emul) y en el
 * core Z80/EMULib de Marat Fayzullin.
 *
 * https://minibots.wordpress.com/retroinformatica/
 */

#include "esp_system.h"

enum BoardType {
    BOARD_UNKNOWN,
    BOARD_LILYGO_TTGO_VGA32,
    BOARD_OLIMEX_FABGL
};

BoardType detectarPlaca() {
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    if (chip_info.revision >= 3) return BOARD_OLIMEX_FABGL;
    if (chip_info.revision == 1) return BOARD_LILYGO_TTGO_VGA32;
    return BOARD_UNKNOWN;
}

#include "fabgl.h"
#include <SPI.h>
#include <SD.h>
#include "sc3000_config.h"
#include "SC3000.h"

fabgl::VGA16Controller VGAController;
fabgl::PS2Controller   PS2Controller;
fabgl::Canvas          *Canvas;

int pin_sd_cs   = SD_CS;
int pin_sd_mosi = SD_MOSI;
int pin_sd_miso = SD_MISO_TTGO;
int pin_sd_clk  = SD_CLK;

bool emuRunning = false;
const int MAX_FILES = 200;
const int ITEMS_PER_PAGE = 20;

void runFileBrowser();

void setup() {
    setCpuFrequencyMhz(240);
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n=== SC-3000 Emulator - Fase 0 (esqueleto) ===");

    /* Gotcha conocido de FabGL: la generación de la señal VGA por
     * interrupciones puede acaparar el núcleo 0 e impedir que su
     * tarea idle resetee el watchdog, incluso con yield() en el
     * núcleo 1. Se desactiva en ambos núcleos, como hacen la
     * mayoría de sketches de ejemplo de FabGL. */
    disableCore0WDT();
    disableCore1WDT();

    BoardType miPlaca = detectarPlaca();
    if (miPlaca == BOARD_LILYGO_TTGO_VGA32) {
        Serial.println("Placa: TTGO VGA32");
        pin_sd_miso = SD_MISO_TTGO;
    } else if (miPlaca == BOARD_OLIMEX_FABGL) {
        Serial.println("Placa: Olimex FabGL");
        pin_sd_miso = SD_MISO_OLIMEX;
    } else {
        Serial.println("Placa no reconocida, usando pines TTGO por defecto");
    }

    if (psramInit()) {
        Serial.printf("PSRAM: %d bytes (libres: %d)\n", ESP.getPsramSize(), ESP.getFreePsram());
    } else {
        Serial.println("ERROR: PSRAM no disponible");
        while (1) delay(1000);
    }

    PS2Controller.begin(PS2Preset::KeyboardPort0_MousePort1);
    PS2Controller.keyboard()->setLayout(&fabgl::SpanishLayout);
    Serial.println("PS2Controller OK");

    VGAController.begin();
    Serial.println("VGAController.begin() OK");
    VGAController.setResolution(VGA_512x384_60Hz);
    Serial.println("VGAController.setResolution() OK");

    Canvas = new fabgl::Canvas(&VGAController);
    Serial.println("Canvas creado");
    Canvas->setBrushColor(Color::Black);
    Canvas->clear();
    Canvas->setPenColor(Color::White);
    Canvas->drawText(10, 10, "SC-3000 Emulator - Fase 0");
    Canvas->waitCompletion();
    Serial.println("Primer dibujo en pantalla OK");

    Serial.println("Montando SD...");
    SPI.begin(pin_sd_clk, pin_sd_miso, pin_sd_mosi, pin_sd_cs);
    if (!SD.begin(pin_sd_cs)) {
        Serial.println("ERROR: fallo al montar la SD");
        Canvas->setPenColor(Color::Red);
        Canvas->drawText(10, 30, "Fallo al montar la SD");
        Canvas->waitCompletion();
        while (1) delay(1000);
    }
    Serial.println("SD montada OK");

    Serial.println("Llamando a InitMachine()...");
    if (!InitMachine()) {
        Serial.println("ERROR: InitMachine() devolvio 0");
        Canvas->setPenColor(Color::Red);
        Canvas->drawText(10, 30, "Fallo en InitMachine()");
        Canvas->waitCompletion();
        while (1) delay(1000);
    }
    Serial.println("InitMachine() OK");

    delay(500);
    runFileBrowser();
}

void loop() {
    if (emuRunning) {
        Serial.println("Arrancando SC-3000...");
        Canvas->clear();

        StartSC3000();   /* No retorna hasta INT_QUIT (aún no implementado) */

        emuRunning = false;
        VGAController.setResolution(VGA_512x384_60Hz);
        Canvas->clear();
        runFileBrowser();
    }
    delay(10);
}

/* Selector de cartucho simplificado: solo lista y carga el volcado,
 * sin menú MSX (BASIC/disco/etc.) que aquí no aplica. */
void runFileBrowser() {
    File root = SD.open("/");
    if (!root) {
        Canvas->drawText(10, 50, "No se pudo abrir la raiz de la SD");
        return;
    }

    int totalFiles = 0;
    static String files[MAX_FILES];

    File file = root.openNextFile();
    while (file && totalFiles < MAX_FILES) {
        String fname = String(file.name());
        if (fname.startsWith("/")) fname = fname.substring(1);
        String upper = fname; upper.toUpperCase();
        if (upper.endsWith(".SG") || upper.endsWith(".SC") ||
            upper.endsWith(".BIN") || upper.endsWith(".ROM")) {
            files[totalFiles++] = fname;
        }
        file = root.openNextFile();
    }
    root.close();

    fabgl::Keyboard *kb = PS2Controller.keyboard();
    int selected = 0, topItem = 0;
    bool redraw = true, selecting = true;
    unsigned long lastKeyTime = 0;
    const unsigned long KEY_DELAY = 150;

    if (totalFiles == 0) {
        Canvas->clear();
        Canvas->setPenColor(Color::Yellow);
        Canvas->drawText(10, 10, "No se encontraron cartuchos (.sg/.sc/.bin/.rom)");
        Canvas->waitCompletion();
        return;
    }

    while (selecting) {
        if (redraw) {
            Canvas->setBrushColor(Color::Black);
            Canvas->clear();
            Canvas->setPenColor(Color::Cyan);
            Canvas->drawText(10, 10, "Selecciona cartucho (Arriba/Abajo/Enter):");
            Canvas->setPenColor(Color::BrightMagenta);
            Canvas->drawLine(10, 25, 500, 25);

            for (int i = 0; i < ITEMS_PER_PAGE; i++) {
                int idx = topItem + i;
                if (idx >= totalFiles) break;
                int y = 35 + i * 14;
                Canvas->setPenColor(idx == selected ? Color::Yellow : Color::White);
                if (idx == selected) Canvas->drawText(5, y, ">");
                Canvas->drawText(20, y, files[idx].c_str());
            }
            redraw = false;
        }

        if (kb->virtualKeyAvailable()) {
            fabgl::VirtualKey key = kb->getNextVirtualKey();
            unsigned long now = millis();
            if (now - lastKeyTime >= KEY_DELAY) {
                lastKeyTime = now;
                if (key == fabgl::VK_UP && selected > 0) {
                    selected--;
                    if (selected < topItem) topItem = selected;
                    redraw = true;
                } else if (key == fabgl::VK_DOWN && selected < totalFiles - 1) {
                    selected++;
                    if (selected >= topItem + ITEMS_PER_PAGE) topItem = selected - ITEMS_PER_PAGE + 1;
                    redraw = true;
                } else if (key == fabgl::VK_RETURN) {
                    String path = "/" + files[selected];
                    Canvas->clear();
                    Canvas->drawText(10, 10, "Cargando...");
                    Canvas->drawText(10, 30, path.c_str());
                    if (LoadCartridge(path.c_str())) {
                        Serial.printf("Cartucho cargado: %s (%d bytes)\n", path.c_str(), CartSize);
                        Serial.print("Primeros bytes: ");
                        for (int b = 0; b < 16 && b < CartSize; b++) {
                            Serial.printf("%02X ", CartROM[b]);
                        }
                        Serial.println();
                        emuRunning = true;
                        selecting = false;
                    } else {
                        Canvas->setPenColor(Color::Red);
                        Canvas->drawText(10, 50, "Error al cargar el cartucho");
                        Canvas->waitCompletion();
                        delay(1500);
                        redraw = true;
                    }
                }
            }
        }
        delay(10);
    }
}
