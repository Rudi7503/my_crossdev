#ifndef ADPCM_UART_ASM_H
#define ADPCM_UART_ASM_H
void uart_init(void);
void uart_puts(const char *s);
void uart_flush(void);
#endif
