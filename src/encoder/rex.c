#include "rex.h"

uint8_t rex_byte(bool w, bool r, bool x, bool b) {
    return (uint8_t)(0x40 | (w ? 0x08 : 0)
                          | (r ? 0x04 : 0)
                          | (x ? 0x02 : 0)
                          | (b ? 0x01 : 0));
}

bool rex_required(bool w, bool r, bool x, bool b) {
    return w || r || x || b;
}
