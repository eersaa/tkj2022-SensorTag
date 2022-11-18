
#include <unistd.h> // For Sleep() to work on linux
//#include <windows.h> // For Sleep() to work on windows
#include <stdio.h>

#include "mpuSim.h"


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

    // Loop forever
    while (1)
    {
        get_pet_data(&ax, &ay, &az, &gx, &gy, &gz);

        printf("Data: %4.2f,%4.2f,%4.2f,%4.2f,%4.2f,%4.2f\n", ax, ay, az, gx, gy, gz);

        sleep(1);
    }
    
}
