
#include <unistd.h> // For Sleep() to work on linux
//#include <windows.h> // For Sleep() to work on windows
#include <stdio.h>
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>

#include "mpuSim.h"

#define AXLE_SAMPLE_SIZE 40

// Prototypes
void movavg(float *array, uint8_t array_size, uint8_t window_size, float *array_to_save_values);
float average(float *array, uint8_t size);
float variance(float *array, uint8_t size);
int detect_movement(void);

float data[AXLE_SAMPLE_SIZE];

float gyro_y_sma_data[AXLE_SAMPLE_SIZE - 3];

int main(int argc, char const *argv[])
{
    // Initialize table
    int t = init_data();
    // Check for errors
    if (t)
    {
        return t;
    }
    
    float ax, ay, az, gx, gy, gz;

    // Fill table first with data
    for (int i = 0; i < sizeof(data)/sizeof(data[0]); i++)
    {
        get_eat_data(&ax, &ay, &az, &gx, &gy, &gz);

        data[i] = gy;
    }
    
    // Loop forever
    while (1)
    {
        // // Call pet data
        // get_pet_data(&ax, &ay, &az, &gx, &gy, &gz);
        // // Call eat data
        // get_eat_data(&ax, &ay, &az, &gx, &gy, &gz);
        // // Call exercise data
        // get_exe_data(&ax, &ay, &az, &gx, &gy, &gz);

        if (detect_movement() == 0) {
            printf("Doing nothing\n");
        } else if (detect_movement() == 1) {
            printf("EAT\n");
        } else if (detect_movement() == 2) {
            printf("EXERCISE\n"); 
        } else if (detect_movement() == 3) {
            printf("PET\n");
        }

        // Shift table one index and add new value to end
        for (int i = 0; i < sizeof(data)/sizeof(data[0]) - 1; i++)
        {
            data[i] = data[i+1];
        }
        int timestamp = 0;
        timestamp = get_eat_data(&ax, &ay, &az, &gx, &gy, &gz);
        // Last index
        data[sizeof(data)/sizeof(data[0]) - 1] = gy;

        printf("%d\t", timestamp);

        usleep(50 * 1000); // Time in milliseconds * 1000.

    }
    
}

// return 0 -> no movement detected
// return 1 -> EAT
// return 2 -> EXERCISE
// return 3 -> PET
// Requires a little bit more defining to x and z values
int detect_movement() {
    //movavg(data, AXLE_SAMPLE_SIZE, 4, gyro_x_sma_data);
    movavg(data, AXLE_SAMPLE_SIZE, 4, gyro_y_sma_data);
    //movavg(data, AXLE_SAMPLE_SIZE, 4, gyro_z_sma_data);

    for (int i = 0; i < AXLE_SAMPLE_SIZE - 1; i++) {
        // Detecting if eating
        if (abs(data[i]) > variance(data, AXLE_SAMPLE_SIZE) + 100) {
            return 1;
        }
    }

    return 0;
}

float average(float *array, uint8_t size) {
    float *ptr = array;
    float avg;

    for (int i = 0; i < size - 1; i++) {
        avg = avg + *ptr;
        ptr++;
    }
    avg = avg / size;

    return avg;
}

float variance(float *array, uint8_t size) {
    float *ptr = array;
    float variance; 

    for (int i = 0; i < size - 1; i++) {
        variance = (average(array, size) - *ptr) * (average(array, size) - *ptr);
        //variance = pow(average(array, size) - *ptr,2 );
        ptr++;
    }
    variance = variance / size;

    return variance;
}

// Function for calculating average and Simple Moving Average (SMA)
void movavg(float *array, uint8_t array_size, uint8_t window_size, float *array_to_save_values) {
    float *ptr_start = array; // Starting pointer for every window
    float *ptr_avg = array; // Pointer moving inside the window
    float sma_avg; // Counter for SMA value

    for (int s=0; s < array_size - window_size + 1; s++) {
        // Calculating sum of values in the window
        for (int a=0; a < window_size; a++) {
            sma_avg = sma_avg + *ptr_avg;
            ptr_avg++;
        }

        sma_avg = sma_avg / window_size;

        array_to_save_values[s] = sma_avg;

        sma_avg = 0;

        // Moving start pointer by one, setting avg pointer to it's location
        ptr_start++;
        ptr_avg = ptr_start;
    }
}
