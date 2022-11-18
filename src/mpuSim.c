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
} pet_data[250];

// Pointer to walk trough the data tables 
struct dataPoint *pet_data_ptr = &pet_data[0];

// Local Prototypes
int parseStruct(char *str, struct dataPoint *dataPoint);
int readDataToArray(char *path, struct dataPoint *dataPoint, uint tableLen);
int get_x_data(struct dataPoint *dataTable, struct dataPoint *nextdp, uint tableLen, struct dataPoint *dataPoint);

// Initializes the data to arrays
int init_data(void) {
    if (readDataToArray("../misc/pet_swinging1.csv", pet_data, sizeof(pet_data)/sizeof(pet_data[0])))
    {
        printf("Initializing pet data failed\n");
        return -1;
    }
    else
    {
        return 0;
    }
    
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
    char *p;

    // Replace the newline character with the null character if found
    p = strchr(str, '\n');
    if (p) {
      *p = '\0';
    }

        // Separate the first part and test            
    token = strtok(str, sep);
    if (token != NULL)
    {
        // Save timestamp to structure
        dataPoint->timestamp = (uint32_t)strtoul(token, 0, 10);

        // Separate rest of the parts
        while( token != NULL ) {
            //printf("%s\n",token);

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
            printf ("Not all data objects found\n");
            return 1;
        }
        else
        {
            return 0;
        }
    }
    else
    {
        printf("Empty line\n");
        return 1;
    }
}

// Read csv file to structured array
int readDataToArray(char *path, struct dataPoint *dataPoint, uint tableLen) {
    FILE *fp;
    char row[MAXCHAR];
    struct dataPoint *endptr;
    if (tableLen == 0)
    {
        printf ("Size of array needs to be greater than zero\n");
        return -1;
    }
    
    endptr = dataPoint + tableLen - 1;

    fp = fopen(path,"r");
    // Test that file opened successfully
    if (fp == NULL)
    {
        printf("Cannot open file\n");
        return -1;
    }
    

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
            break;
        }
        else
        {
            dataPoint++;
        }
        
    }

    return 0;
}

int get_x_data(struct dataPoint *dataTable, struct dataPoint *nextdp, uint tableLen, struct dataPoint *dataPoint) {

    // Get the end pointer
    struct dataPoint const *endptr = dataTable + tableLen - 1;

    // Check that pointer can move one step forward
    if ((dataPoint + 1) <= endptr)
    {
        nextdp = dataPoint + 1;
        dataPoint = nextdp - 1;
        return 0;
    }
    // Else move to start
    else
    {
        nextdp = dataTable;
        dataPoint = (struct dataPoint *)endptr; // Cast the constant to variable
        return 0;
    }
}
