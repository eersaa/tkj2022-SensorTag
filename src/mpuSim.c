#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "mpuSim.h"

#define MAXCHAR 50

// Define structure for mpu sensor datapoint
struct dataPoint {
    uint32_t timestamp;
    float ax;
    float ay;
    float az;
    float gx;
    float gy;
    float gz;
} exe_data[250];

// Local Prototypes
int parseStruct(char *str, struct dataPoint *dataPoint);
int readDataToArray(char *path, struct dataPoint *dataPoint, uint struct_len);

int init_data(void) {
    return 0;
}

void mpu_get_data(float *ax, float *ay, float *az, float *gx, float *gy, float *gz) {
    ;
}

// Parse one data line to structure
int parseStruct(char *str, struct dataPoint *dataPoint) {
    const char sep[] = ",";
    int i = 0;
    char *token; // Pointer to current location
   
    // Separate the first part and save to structure
    token = strtok(str, sep);
    dataPoint->timestamp = (uint32_t)strtoul(token, 0, 10);

    // Separate rest of the parts
    while( token != NULL ) {
        printf("%s\n",token);

        token = strtok(NULL, sep);

        if (i == 0)
        {
            dataPoint->ax = (float)strtof(token, 0);
            i++;
        }
        else if (i == 1)
        {
            dataPoint->ay = (float)strtof(token, 0);
            i++;
        }
        else if (i == 2)
        {
            dataPoint->az = (float)strtof(token, 0);
            i++;
        }
        else if (i == 3)
        {
            dataPoint->gx = (float)strtof(token, 0);
            i++;
        }
        else if (i == 4)
        {
            dataPoint->gy = (float)strtof(token, 0);
            i++;
        }
        else if (i == 5)
        {
            dataPoint->gz = (float)strtof(token, 0);
            i++;
        }
    }

    if (i < 5)
    {
        printf ("Not all data objects found");
        return 1;
    }
    else
    {
        return 0;
    }
    
    
}

// Read csv file to structured array
int readDataToArray(char *path, struct dataPoint *dataPoint, uint sizeofArray) {
    FILE *fp;
    char row[MAXCHAR];
    struct dataPoint *endptr;
    if (sizeofArray == 0)
    {
        printf ("Size of array needs to be greater than zero");
        return -1;
    }
    
    endptr = dataPoint + sizeofArray - 1;

    fp = fopen(path,"r");
    // Read the header row
    fgets(row, MAXCHAR, fp);

    while (feof(fp) != true &&
            dataPoint < endptr + 1)
    {
        // Read first data row
        fgets(row, MAXCHAR, fp);
        printf("Row: %s\n", row);

        if (parseStruct(row, dataPoint))
        {
            dataPoint++;
        }
    }

    return 0;
}