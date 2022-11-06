/* C Standard library */
#include <stdio.h>

/* XDCtools files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/drivers/PIN.h>
#include <ti/drivers/pin/PINCC26XX.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/i2c/I2CCC26XX.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>
#include <ti/drivers/UART.h>

/* Board Header files */
#include "Board.h"
#include "wireless/comm_lib.h"
#include "sensors/opt3001.h"
#include "sensors/mpu9250.h"

/* Task */
#define STACKSIZE 2048
Char uartTaskStack[STACKSIZE];
Char mpuTaskStack[STACKSIZE];

// Definition of the state machine states
enum state
{
    WAITING_HOME = 1, //
    WAITING_READ,   // Waiting the time interruption to read the MPU sensor
    READ_MPU,       // Reads the MPU sensor
    WRITE_UART      // Write data to UART
};
enum state programState = WAITING_HOME;

//Global variable for ambient light
double ambientLight = -1000.0;
char merkkijono[50];

// RTOS:n globaalit muuttujat pinnien käyttöön
static PIN_Handle buttonHandle;
static PIN_State buttonState;
static PIN_Handle ledHandle;
static PIN_State ledState;

// RTOS:n kellomuuttujat
static Clock_Handle clkHandle;
static Clock_Params clkParams;

// Pinnien alustukset, molemmille pinneille oma konfiguraatio
// Vakio BOARD_BUTTON_0 vastaa toista painonappia
PIN_Config buttonConfig[] = {
Board_BUTTON0 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
                              PIN_TERMINATE // Asetustaulukko lopetetaan aina tällä vakiolla
        };

// Vakio Board_LED0 vastaa toista lediä

PIN_Config ledConfig[] = {
Board_LED0 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
                           PIN_TERMINATE // Asetustaulukko lopetetaan aina tällä vakiolla
        };

// MPU power pin global variables
static PIN_Handle hMpuPin;
static PIN_State  MpuPinState;

// MPU power pin
static PIN_Config MpuPinConfig[] = {
    Board_MPU_POWER  | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE
};

// MPU uses its own I2C interface
static const I2CCC26XX_I2CPinCfg i2cMPUCfg = {
    .pinSDA = Board_I2C0_SDA1,
    .pinSCL = Board_I2C0_SCL1
};

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

// Define variables for data capture
struct dataPoint mpuData[5]; // Table for datapoints
struct dataPoint *dataPtr; // Pointer for data writing
uint32_t firstTimeStamp; // Timestamp of first datapoint
int8_t timeStampSet = FALSE; // Flag to for remembering that capture started
int8_t dataReady = FALSE; // Flag to for remembering that capture done

// Napinpainalluksen keskeytyksen käsittelijäfunktio
void buttonFxn(PIN_Handle handle, PIN_Id pinId)
{
    // Luetaan ledin tila
    uint_t pinValue = PIN_getOutputValue( Board_LED0);

    if (programState == WAITING_HOME) {

        //Vaihdetaan ledin tila
        pinValue = 1;

        Clock_start(clkHandle); // Start the clock interruption
        // Prepare for new data capture
        timeStampSet = FALSE;

        programState = WAITING_READ;

    } else if (!programState == WRITE_UART){

        //Vaihdetaan ledin tila
        pinValue = 0;

        // Stop the clock interruption
        Clock_stop(clkHandle);

        programState = WAITING_HOME;
    }

    PIN_setOutputValue(ledHandle, Board_LED0, pinValue);

}

// Kellokeskeytyksen käsittelijä
void clkFxn(UArg arg0) {

   if (programState == WAITING_READ) {
       programState = READ_MPU;
   }

}

/* Task Functions */
Void uartTaskFxn(UArg arg0, UArg arg1)
{

    // UART-kirjaston asetukset
    UART_Handle uart;
    UART_Params uartParams;

    // Alustetaan sarjaliikenne
    UART_Params_init(&uartParams);
    uartParams.writeDataMode = UART_DATA_TEXT;
    uartParams.readDataMode = UART_DATA_TEXT;
    uartParams.readEcho = UART_ECHO_OFF;
    uartParams.readMode = UART_MODE_BLOCKING;
    uartParams.writeMode = UART_MODE_BLOCKING;
    uartParams.baudRate = 9600; // nopeus 9600baud
    uartParams.dataLength = UART_LEN_8; // 8
    uartParams.parityType = UART_PAR_NONE; // n
    uartParams.stopBits = UART_STOP_ONE; // 1

    // Avataan yhteys laitteen sarjaporttiin vakiossa Board_UART0
    uart = UART_open(Board_UART0, &uartParams);
    if (uart == NULL)
    {
        System_abort("Error opening the UART");
    }

    while (1)
    {
        if (programState == WAITING_HOME &&
                dataReady)
        {
            programState = WRITE_UART;
        }
        // Print out the sensor data to uart
        else if (programState == WRITE_UART)
        {
            // Print data as csv format
            sprintf(merkkijono, "timestamp,acc_x,acc_y,acc_z,gyro_x,gyro_y,gyro_z\n");
            System_printf(merkkijono); // Column headers
            System_flush();

            // Write the string to uart
            sprintf(merkkijono, "%s\r", merkkijono);
            UART_write(uart, merkkijono, strlen(merkkijono));

            // Datapoints
            for (dataPtr = mpuData; dataPtr < &mpuData[sizeof(mpuData)/sizeof(mpuData[0])]; ++dataPtr) {

                sprintf(merkkijono, "%d,%4.2f,%4.2f,%4.2f,%4.2f,%4.2f,%4.2f\n",
                        dataPtr->timestamp,
                        dataPtr->ax,
                        dataPtr->ay,
                        dataPtr->az,
                        dataPtr->gx,
                        dataPtr->gy,
                        dataPtr->gz
                        );
                System_printf(merkkijono);
                System_flush();

                // Write the string to uart
                sprintf(merkkijono, "%s\r", merkkijono);
                UART_write(uart, merkkijono, strlen(merkkijono));
            }

            dataReady = FALSE;  // Reset after data write
            programState = WAITING_HOME;

        }
        /*
        //debug
         // Print program state
         sprintf(merkkijono,"programState %d\n",programState);
         System_printf(merkkijono);

         // Just for sanity check for exercise, you can comment this out
         System_printf("uartTask\n");
         System_flush();
         */

        // Once per second, you can modify this
        Task_sleep(1000000 / Clock_tickPeriod);
    }
}

Void mpuSensorFxn(UArg arg0, UArg arg1) {

    I2C_Handle i2cMPU; // Own i2c-interface for MPU9250 sensor
    I2C_Params i2cMPUParams;

    I2C_Params_init(&i2cMPUParams);
    i2cMPUParams.bitRate = I2C_400kHz;
    // Note the different configuration below
    i2cMPUParams.custom = (uintptr_t)&i2cMPUCfg;

    // MPU power on
    PIN_setOutputValue(hMpuPin,Board_MPU_POWER, Board_MPU_POWER_ON);

    // Wait 100ms for the MPU sensor to power up
    Task_sleep(100000 / Clock_tickPeriod);
    System_printf("MPU9250: Power ON\n");
    System_flush();

    // MPU open i2c
    i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);
    if (i2cMPU == NULL) {
        System_abort("Error Initializing I2CMPU\n");
    }

    // MPU setup and calibration
//            System_printf("MPU9250: Setup and calibration...\n");
//            System_flush();

    mpu9250_setup(&i2cMPU);

//            System_printf("MPU9250: Setup and calibration OK\n");
//            System_flush();

    // Loop forever
    while (1) {

        if (programState == READ_MPU) {

            if (!timeStampSet) {

                // Clear the table
                for (dataPtr = mpuData; dataPtr < &mpuData[sizeof(mpuData)/sizeof(mpuData[0])]; ++dataPtr) {
                    dataPtr->timestamp = 0;
                    dataPtr->ax = 0.0;
                    dataPtr->ay = 0.0;
                    dataPtr->az = 0.0;
                    dataPtr->gx = 0.0;
                    dataPtr->gy = 0.0;
                    dataPtr->gz = 0.0;
                }
                // Set the pointer again to start of table
                dataPtr = mpuData;

                firstTimeStamp = (Clock_getTicks()*Clock_tickPeriod)/1000; // In milliseconds
                timeStampSet = TRUE;
            } else {
                dataPtr->timestamp = (Clock_getTicks()*Clock_tickPeriod)/1000 - firstTimeStamp; // In milliseconds
            }

            // MPU ask data
            mpu9250_get_data(&i2cMPU,
                             &dataPtr->ax,
                             &dataPtr->ay,
                             &dataPtr->az,
                             &dataPtr->gx,
                             &dataPtr->gy,
                             &dataPtr->gz
                             );

//            // Debug
//            sprintf(merkkijono, "mpu:%+4.2f,%+4.2f,%+4.2f,%+4.2f,%+4.2f,%+4.2f\n",
//                    dataPtr->ax,
//                    dataPtr->ay,
//                    dataPtr->az,
//                    dataPtr->gx,
//                    dataPtr->gy,
//                    dataPtr->gz
//                    );
//            System_printf(merkkijono);
//            System_flush();

            dataPtr++; //Increase pointer address

            //Check the state and address limit before update
            if (programState == READ_MPU &&
                dataPtr < &mpuData[sizeof(mpuData)/sizeof(mpuData[0])]) {

                programState = WAITING_READ;
            } else {
                dataReady = TRUE;
                programState = WAITING_HOME;
            }

        }

        // Sleep 100ms
        Task_sleep(100000 / Clock_tickPeriod);
    }

    // Program never gets here..
    // MPU close i2c
    // I2C_close(i2cMPU);
    // MPU power off
    // PIN_setOutputValue(hMpuPin,Board_MPU_POWER, Board_MPU_POWER_OFF);
}

Int main(void)
{
    uint16_t cycle = 500; // milliseconds

    // Task variables
    Task_Handle uartTaskHandle;
    Task_Params uartTaskParams;
    Task_Handle mpuTask;
    Task_Params mpuTaskParams;

    // Initialize board
    Board_initGeneral();
    Init6LoWPAN();

    // Initialize i2c bus
    Board_initI2C();

    // Initialize UART
    Board_initUART();

    // Alustetaan kello
    Clock_Params_init(&clkParams);
    clkParams.period = cycle * 1000 / Clock_tickPeriod;
    clkParams.startFlag = FALSE;

    // Otetaan käyttöön ohjelmassa
    clkHandle = Clock_create((Clock_FuncPtr)clkFxn, 1000000 / Clock_tickPeriod, &clkParams, NULL);
    if (clkHandle == NULL) {
       System_abort("Clock create failed");
    }

    // Otetaan pinnit käyttöön ohjelmassa
    buttonHandle = PIN_open(&buttonState, buttonConfig);
    if (!buttonHandle)
    {
        System_abort("Error initializing button pins\n");
    }
    ledHandle = PIN_open(&ledState, ledConfig);
    if (!ledHandle)
    {
        System_abort("Error initializing LED pins\n");
    }

    // Open MPU power pin
    hMpuPin = PIN_open(&MpuPinState, MpuPinConfig);
    if (hMpuPin == NULL) {
        System_abort("Pin open failed!");
    }

    // Asetetaan painonappi-pinnille keskeytyksen käsittelijäksi
    // funktio buttonFxn
    if (PIN_registerIntCb(buttonHandle, &buttonFxn) != 0)
    {
        System_abort("Error registering button callback function");
    }

    /* Task */
    Task_Params_init(&uartTaskParams);
    uartTaskParams.stackSize = STACKSIZE;
    uartTaskParams.stack = &uartTaskStack;
    uartTaskParams.priority = 2;
    uartTaskHandle = Task_create(uartTaskFxn, &uartTaskParams, NULL);
    if (uartTaskHandle == NULL)
    {
        System_abort("UART task create failed!");
    }

    // mpu9250 task
    Task_Params_init(&mpuTaskParams);
    mpuTaskParams.stackSize = STACKSIZE;
    mpuTaskParams.stack = &mpuTaskStack;
    mpuTaskParams.priority = 2;
    mpuTask = Task_create(mpuSensorFxn, &mpuTaskParams, NULL);
    if (mpuTask == NULL) {
        System_abort("MPU task create failed!");
    }

    /* Sanity check */
    System_printf("Hello world!\n");
    System_flush();

    /* Start BIOS */
    BIOS_start();

    return (0);
}
