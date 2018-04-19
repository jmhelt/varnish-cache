#ifndef UINT32_VOIDSTAR_TABLE_H
#define UINT32_VOIDSTAR_TABLE_H

#include <stdint.h>

#define CUCKOO_TABLE_NAME uint32_void_tbl
#define CUCKOO_KEY_TYPE uint32_t
#define CUCKOO_MAPPED_TYPE void *

#include "cuckoo_table_template.h"

#endif // UINT32_VOIDSTAR_TABLE_H
