/*
This tries to simulate the mpu9250 sensor data with captured data
*/

int init_data(void);

void mpu_get_data(float *ax, float *ay, float *az, float *gx, float *gy, float *gz);