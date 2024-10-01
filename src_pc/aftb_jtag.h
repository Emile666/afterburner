#ifndef _AFTB_JTAG_H_
#define _AFTB_JTAG_H_
#include <stdbool.h>
#include <stdint.h>

#define JTAG_ID (0xFF)

bool     processJtagInfo(void);
bool     processJtagErase(void);
bool     processJtagWrite(void);
bool     processJtag(void);

#endif /* _AFTB_JTAG_H_ */
