/*************************************************************/
/**                        SN76489.h                        **/
/**                                                          **/
/** Interfaz del generador de sonido programable SN76489     **/
/** del SC-3000 real (3 canales de tono + 1 de ruido, sin    **/
/** envolventes: es más simple que el AY8910 del MSX).       **/
/**                                                          **/
/** FASE 0 (actual): stub, ignora las escrituras.            **/
/** FASE 2 (pendiente): 3 osciladores de tono + LFSR de       **/
/**   ruido, mezclados y enviados por I2S/DAC.               **/
/*************************************************************/
#ifndef SN76489_H
#define SN76489_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BYTE_TYPE_DEFINED
#define BYTE_TYPE_DEFINED
typedef unsigned char byte;
#endif

/** ResetPSG() ***************************************************/
/** Inicializa los registros del PSG a su estado de encendido.   **/
/** Nota de hardware: el SN76489 real del SC-3000 NO se resetea  **/
/** al encender la máquina (no tiene pin de reset conectado), lo **/
/** emulamos igualmente a silencio por comodidad/determinismo.   **/
/*****************************************************************/
void ResetPSG(void);

/** WrPSG() ******************************************************/
/** Escribe un byte en el único puerto de escritura del PSG.      **/
/*****************************************************************/
void WrPSG(byte Value);

#ifdef __cplusplus
}
#endif
#endif /* SN76489_H */
