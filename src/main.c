
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
        // Call pet data
        get_pet_data(&ax, &ay, &az, &gx, &gy, &gz);
        // Call eat data
        get_eat_data(&ax, &ay, &az, &gx, &gy, &gz);
        // Call exercise data
        get_exe_data(&ax, &ay, &az, &gx, &gy, &gz);

        sleep(1); // Time in seconds 2 = two seconds, 0.2 = 200 milliseconds

    }
    
}
