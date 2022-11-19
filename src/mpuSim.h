/*
This tries to simulate the mpu9250 sensor data with captured data
*/

// Initialize data to tables
int init_data(void);

// Get data from pet table
void get_pet_data(float *ax, float *ay, float *az, float *gx, float *gy, float *gz);

// Get data from eat table
int get_eat_data(float *ax, float *ay, float *az, float *gx, float *gy, float *gz);

// Get data from exercise table
void get_exe_data(float *ax, float *ay, float *az, float *gx, float *gy, float *gz);
