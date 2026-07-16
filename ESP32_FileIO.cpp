/*************************************************************/
/**                    ESP32_FileIO.cpp                     **/
/**                                                          **/
/** Puente mínimo entre la librería SD (C++) de Arduino y    **/
/** el mundo C de SC3000.c. Es el mismo patrón que ya usa    **/
/** el proyecto MSX en ESP32_Port.cpp, pero aislado en su    **/
/** propio archivo para no arrastrar nada específico de MSX. **/
/*************************************************************/
#include <Arduino.h>
#include <SD.h>

extern "C" {

void* arduino_fopen(const char* filename, const char* mode)
{
    if (!filename) return NULL;
    String path = String(filename);
    if (!path.startsWith("/")) path = "/" + path;
    File tempFile = SD.open(path, FILE_READ);
    if (!tempFile) return NULL;
    File* fptr = new File(tempFile);
    return (void*)fptr;
}

int arduino_fclose(void* stream)
{
    if (!stream) return 0;
    File* f = (File*)stream;
    f->close();
    delete f;
    return 0;
}

size_t arduino_fread(void* ptr, size_t size, size_t nmemb, void* stream)
{
    if (!stream || !ptr || size == 0 || nmemb == 0) return 0;
    File* f = (File*)stream;
    size_t bytesRead = f->read((uint8_t*)ptr, size * nmemb);
    return bytesRead / size;
}

long arduino_ftell(void* stream)
{
    if (!stream) return -1;
    File* f = (File*)stream;
    return f->position();
}

int arduino_feof(void* stream)
{
    if (!stream) return 1;
    File* f = (File*)stream;
    return !f->available();
}

} // extern "C"
