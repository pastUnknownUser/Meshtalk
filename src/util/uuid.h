#pragma once

#include <stdint.h>
#include <stdbool.h>

void uuid_generate(char* out_str);

bool uuid_validate(const char* str);
