#ifndef LPIT_H
#define LPIT_H

#include <stdint.h>
#include <stdbool.h>
void LPIT_Init(uint32_t periodUs);
/* Returns true once per elapsed period and clears the hardware flag. */
bool LPIT_HasElapsed(void);
void LPIT_Process(const uint8_t *message, uint32_t length);

#endif /* LPIT_H */
