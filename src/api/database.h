#pragma once

#include <sqlite3.h>
#include <stdint.h>

uint16_t database_error(sqlite3 *database, int result);
