#ifndef S5852A_H
#define S5852A_H

#include <stdint.h>

void s5852a_init(void);
esp_err_t s5852a_get(float *temperature);

#endif //S5852A_H
