/*************************************************************/
/**                        SN76489.c                        **/
/**                                                          **/
/** Ver SN76489.h. Fase 0: guarda el byte recibido para que   **/
/** el bus no quede indefinido, pero no genera audio todavía. **/
/*************************************************************/
#include "SN76489.h"

static byte LastValue;

void ResetPSG(void)
{
    LastValue = 0;
}

void WrPSG(byte Value)
{
    /* FASE 2: aquí irá el decodificado real de los bytes de
     * latch/data (bit7=1 => byte de latch, selecciona canal y
     * si es volumen o frecuencia; bit7=0 => byte de datos,
     * completa los 6 bits altos de frecuencia de un canal de tono). */
    LastValue = Value;
}
