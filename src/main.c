
#include <unistd.h> // For Sleep() to work on linux
//#include <windows.h> // For Sleep() to work on windows
#include <stdio.h>
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>

#include "mpuSim.h"

#define AXLESAMPLESIZE 5
#define WINDOWSIZE 3

float mpuData[6][AXLESAMPLESIZE];
enum axle {axle_x_acc = 0, axle_y_acc = 1, axle_z_acc = 2, axle_x_gyro = 3, axle_y_gyro = 4, axle_z_gyro = 5};

// Prototypes
void movavg(float *array, uint8_t array_size, uint8_t window_size, float *array_to_save_values);
float average(float *array, uint8_t size);
float variance(float *array, uint8_t size);
int detect_movement(void);


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
    for (int i = 0; i < sizeof(mpuData[0])/sizeof(mpuData[0][0]); i++)
    {
        get_pet_data(&ax, &ay, &az, &gx, &gy, &gz);

        mpuData[axle_x_acc][i] = ax;
        mpuData[axle_y_acc][i] = ay;
        mpuData[axle_z_acc][i] = az;

        mpuData[axle_x_gyro][i] = gx;
        mpuData[axle_y_gyro][i] = gy;
        mpuData[axle_z_gyro][i] = gz;

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
        for (int i = 0; i < sizeof(mpuData[0])/sizeof(mpuData[0][0]) - 1; i++)
        {
            mpuData[axle_x_acc][i] = mpuData[axle_x_acc][i+1]; 
            mpuData[axle_y_acc][i] = mpuData[axle_y_acc][i+1]; 
            mpuData[axle_z_acc][i] = mpuData[axle_z_acc][i+1]; 

            mpuData[axle_x_gyro][i] = mpuData[axle_x_gyro][i+1];
            mpuData[axle_y_gyro][i] = mpuData[axle_y_gyro][i+1];
            mpuData[axle_z_gyro][i] = mpuData[axle_z_gyro][i+1];
        }
        int timestamp = 0;
        timestamp = get_pet_data(&ax, &ay, &az, &gx, &gy, &gz);
        // Get new value to last index
        mpuData[axle_x_acc][sizeof(mpuData[0])/sizeof(mpuData[0][0]) - 1] = ax;
        mpuData[axle_y_acc][sizeof(mpuData[0])/sizeof(mpuData[0][0])] = ay;
        mpuData[axle_z_acc][sizeof(mpuData[0])/sizeof(mpuData[0][0])] = az;

        mpuData[axle_x_gyro][sizeof(mpuData[0])/sizeof(mpuData[0][0])] = gx;
        mpuData[axle_y_gyro][sizeof(mpuData[0])/sizeof(mpuData[0][0])] = gy;
        mpuData[axle_z_gyro][sizeof(mpuData[0])/sizeof(mpuData[0][0])] = gz;

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

    for (int i = 0; i < AXLESAMPLESIZE; i++) {

        // Detect eating 
        // Needs to also check if the button is pressed
        if (mpuData[axle_y_gyro][i] < -120 && mpuData[axle_z_acc][i] > 1.75) {
            return 1;
        }

        // // Detect excercising
        // // For some unknown reason, test data doesnt 
        // if (mpuData[axle_z_acc][i]*9.81 < 0.8) {
        //     return 2;
        // }

        // Detect petting
        if ((mpuData[axle_y_acc][i]*9.81 > 0.85 && mpuData[axle_x_gyro][i] > 200) ||
            (mpuData[axle_z_acc][i] > 0.85 && mpuData[axle_x_gyro][i] > 200)) {
            return 3;
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

// Function for calculating Simple Moving Average (SMA)
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
