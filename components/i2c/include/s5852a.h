#ifndef S5852A_H
#define S5852A_H

#include <stdint.h>
#include "sdk_errors.h"

void s5852a_init(void);
ret_code_t s5852a_get(float *temperature);

#endif //S5852A_H
