#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "mpuSim.h"

// Define structure for mpu sensor datapoint
struct dataPoint {
    uint32_t timestamp;
    float ax;
    float ay;
    float az;
    float gx;
    float gy;
    float gz;
};

int init_data(void) {
    return 0;
}

void mpu_get_data(float *ax, float *ay, float *az, float *gx, float *gy, float *gz) {
    ;
}