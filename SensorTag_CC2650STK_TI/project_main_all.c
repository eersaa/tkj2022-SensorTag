/* C Standard library */
#include <stdio.h>
#include <stdlib.h>

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

/* Board and peripheral Header files */
#include "Board.h"
#include "wireless/comm_lib.h"
#include "sensors/opt3001.h"
#include "sensors/mpu9250.h"
#include "peripherals/buzzer.h"

/* Task */
#define STACKSIZE 2048
Char mainTaskStack[STACKSIZE];
Char sensorTaskStack[STACKSIZE];
Char commTaskStack[STACKSIZE];

#define B_MAX_LEN 80    // Define maximum length for buffer.
#define CYCLE 50        // Reading rage for mpu9259 sensor in milliseconds
#define BOARD_ID 2232

// Definition of the state machine states
enum states
{
    WAITING = 1,    // States for main program
    OPEN_MPU_I2C,
    DETECT_MOVEMENT,
    DETECT_EATING,
    CLOSE_MPU_I2C,
    UPDATE_STATUS_TO_HOST,

    SENSORS_WAITING, //States for reading data from other sensors
    OPEN_I2C,
    READ_OTHER_SENSOR,
    CLOSE_I2C,

    UART_WAITING,   // States for uart communication
    UART_PROCESS_MESSAGE
};
// Three state machines
enum states programState = WAITING;
enum states commState = UART_WAITING;
enum states sensorState = SENSORS_WAITING;

//Global variables
double ambientLight = -1000.0;
char uartBuffer[B_MAX_LEN];     // Buffer for uart interruption
char inMessage[B_MAX_LEN];      // Memory for incoming message
char outMessage[B_MAX_LEN];     // Memory for outgoing message
char merkkijono[B_MAX_LEN];     // String for prints


// Variables for interruptions
uint8_t button0Int = false;
uint8_t button1Int = false;
uint8_t clkInt = false;

// RTOSs global variables for PINs
static PIN_Handle button0Handle;
static PIN_State button0State;
static PIN_Handle button1Handle;
static PIN_State button1State;
static PIN_Handle led0Handle;
static PIN_State led0State;
static PIN_Handle led1Handle;
static PIN_State led1State;
static PIN_Handle hBuzzer;
static PIN_State sBuzzer;
static PIN_Handle hMpuPin;
static PIN_State  MpuPinState;

// RTOSs clock variables
static Clock_Handle clkHandle;
static Clock_Params clkParams;

// Pinnien alustukset, molemmille pinneille oma konfiguraatio
// Vakio BOARD_BUTTON_0 vastaa toista painonappia
PIN_Config button0Config[] = {
Board_BUTTON0 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
                              PIN_TERMINATE // Asetustaulukko lopetetaan aina tällä vakiolla
        };

PIN_Config button1Config[] = {
Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
                              PIN_TERMINATE // Asetustaulukko lopetetaan aina tällä vakiolla
        };

// Initialize first board led
PIN_Config led0Config[] = {Board_LED0 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX, PIN_TERMINATE};

// Initialize second board led
PIN_Config led1Config[] = {Board_LED1 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX, PIN_TERMINATE};

// Pin config for buzzer
PIN_Config cBuzzer[] = {Board_BUZZER | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,PIN_TERMINATE};

// MPU power pin
static PIN_Config MpuPinConfig[] = {Board_MPU_POWER  | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX, PIN_TERMINATE};

// MPU uses its own I2C interface
static const I2CCC26XX_I2CPinCfg i2cMPUCfg = {
    .pinSDA = Board_I2C0_SDA1,
    .pinSCL = Board_I2C0_SCL1
};

// Define structure for activity data
struct activity {
    uint8_t eat;
    uint8_t exercise;
    uint8_t pet;
};

// Define global variable for activity data
struct activity activity = {1, 1, 1};

// Function prototypes

// Make beep sound with buzzer
// Durations in milliseconds
void buzzerBeep(uint16_t beepDuration, uint16_t pauseDuration, uint16_t beeps);

// Create message to be sent to host
void createMessage(uint16_t deviceID, struct activity *activity, char *message);

// Process incoming message
void processMessage(char *message);


// Button press interruption handler
void button0Fxn(PIN_Handle handle, PIN_Id pinId){

    if (programState == WAITING) {
        button0Int = true;
    }
    else if(programState == OPEN_MPU_I2C){
        button0Int = true;
    }
    else if(programState == DETECT_MOVEMENT){
        button0Int = true;
    }

}

// Button press interruption handler
void button1Fxn(PIN_Handle handle, PIN_Id pinId){
    ;

}

// Clock interruption handler
void clkFxn(UArg arg0) {
    // Request interruption
    if(programState == DETECT_MOVEMENT) {
        clkInt = true;

   }
}

// Uart exception handler
static void uartFxn(UART_Handle handle, void *rxBuf, size_t len) {
      if (commState == UART_WAITING) {
          //snprintf(inMessage, B_MAX_LEN, "%s", rxBuf);
          memcpy(inMessage, rxBuf, B_MAX_LEN);

          commState = UART_PROCESS_MESSAGE;
      }
}

/* Task Functions */
// Main task to handle start point for tasks
Void mainTask(UArg arg0, UArg arg1)
{

    while (1)
    {
        if(programState == WAITING)

            // Prepare sensors and inform user
            if(button0Int){
                button0Int = false;
//              led1On()
                buzzerBeep(500, 0, 1);
                Clock_start(clkHandle); // Start clock interruptions

                programState = OPEN_MPU_I2C;
            }

//      (Possible extra function:
//      Read other sensors and send data.)

        // Sleep 100ms
        Task_sleep(100000 / Clock_tickPeriod);

    }
}

Void commTask(UArg arg0, UArg arg1)
{

    // UART-kirjaston asetukset
    UART_Handle uartHandle;
    UART_Params uartParams;

    // Alustetaan sarjaliikenne
    UART_Params_init(&uartParams);
    uartParams.writeDataMode = UART_DATA_TEXT;
    uartParams.readDataMode = UART_DATA_TEXT;
    uartParams.readEcho = UART_ECHO_OFF;
    uartParams.readMode = UART_MODE_CALLBACK;
    uartParams.writeMode = UART_MODE_BLOCKING;
    uartParams.readCallback  = &uartFxn;
    uartParams.baudRate = 9600; // nopeus 9600baud
    uartParams.dataLength = UART_LEN_8; // 8
    uartParams.parityType = UART_PAR_NONE; // n
    uartParams.stopBits = UART_STOP_ONE; // 1

    // Avataan yhteys laitteen sarjaporttiin vakiossa Board_UART0
    uartHandle = UART_open(Board_UART0, &uartParams);
    if (uartHandle == NULL)
    {
        System_abort("Error opening the UART");
    }

    // Nyt tarvitsee käynnistää datan odotus
    UART_read(uartHandle, uartBuffer, B_MAX_LEN);

    System_printf("UART started\n");
    System_flush();

    while (1)
    {
        // Process incoming message
        if(commState == UART_PROCESS_MESSAGE){

            processMessage(inMessage);

            // Start listening the UART again
            UART_read(uartHandle, uartBuffer, B_MAX_LEN);

            commState = UART_WAITING;

        }

        // Update status to host
        if (programState == UPDATE_STATUS_TO_HOST &&
              !(commState == UART_PROCESS_MESSAGE)) {


            createMessage(BOARD_ID, &activity, outMessage);
            UART_write(uartHandle, outMessage, strlen(outMessage));
        }

        // Sleep 100ms
        Task_sleep(100000 / Clock_tickPeriod);
    }
}

Void sensorTask(UArg arg0, UArg arg1)
{

    float ax, ay, az, gx, gy, gz;

    I2C_Handle i2c;
    I2C_Params i2cParams;
    I2C_Handle i2cMPU; // Own i2c-interface for MPU9250 sensor
    I2C_Params i2cMPUParams;

    // Initialize i2c-buses
    I2C_Params_init(&i2cParams);
    i2cParams.bitRate = I2C_400kHz;
    I2C_Params_init(&i2cMPUParams);
    i2cMPUParams.bitRate = I2C_400kHz;

    // Mpu i2c bus has its own configurationNote the different configuration below
    i2cMPUParams.custom = (uintptr_t)&i2cMPUCfg;

    // MPU power on
    PIN_setOutputValue(hMpuPin,Board_MPU_POWER, Board_MPU_POWER_ON);

    // Wait 100ms for the MPU sensor to power up
    Task_sleep(100000 / Clock_tickPeriod);
    System_printf("MPU9250: Power ON\n");
    System_flush();


    while (1)
    {
        // Open mpu i2c bus
        if (programState == OPEN_MPU_I2C) {

            i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);
            if (i2cMPU == NULL) {
                System_abort("Error Initializing I2CMPU\n");
            }
            // Setup mpu sensor
            mpu9250_setup(&i2cMPU);

            //Sleep 1 second
            Task_sleep(1000000 / Clock_tickPeriod);

            programState = DETECT_MOVEMENT;

        }
        // Read accelometer data and detect movement
        else if (programState == DETECT_MOVEMENT){

            if(button0Int){
                button0Int = false;

                programState = CLOSE_MPU_I2C;
            }

            // MPU ask data
            mpu9250_get_data(&i2cMPU, &ax, &ay, &az, &gx, &gy, &gz);

    //      if(button2Interruption)
    //          buttonInterruption = false
    //          led2On()
    //          buzzerBeep()
    //          programState == DETECT_EATING
        }

        // Close mpu i2c bus
        else if (programState == CLOSE_MPU_I2C) {
            I2C_close(i2cMPU);
            Clock_stop(clkHandle); // Stop clock interruptions

            // Sleep 1 second
            Task_sleep(1000000 / Clock_tickPeriod);

            programState = UPDATE_STATUS_TO_HOST;
        }

        // Open i2c bus for other sensors
        if (sensorState == OPEN_I2C){
            i2c = I2C_open(Board_I2C_TMP, &i2cParams);
            if (i2c == NULL)
            {
                System_abort("Error Initializing I2C\n");
            }

            // Sleep 1 second
            Task_sleep(1000000 / Clock_tickPeriod);

            sensorState = READ_OTHER_SENSOR;
        }
        // Read data from other sensors
        else if (sensorState == READ_OTHER_SENSOR) {
            //Initialize opt3001 light sensor
            opt3001_setup(&i2c);

            //Delay the data read to get successful reading
            Task_sleep(1000000 / Clock_tickPeriod);

            ambientLight = opt3001_get_data(&i2c);

            sprintf(merkkijono, "Sensor:%f\n", ambientLight);
            System_printf(merkkijono);
            System_flush();

            sensorState = CLOSE_I2C;

        }

        //Close i2c bus
        else if (sensorState == CLOSE_I2C) {
            I2C_close(i2c);

            // Sleep 1 second
            Task_sleep(1000000 / Clock_tickPeriod);
        }

        // Sleep 10 milliseconds
        Task_sleep(10000 / Clock_tickPeriod);
    }
}


Int main(void)
{

    // Task variables
    Task_Handle mainTaskHandle;
    Task_Params mainTaskParams;
    Task_Handle sensorTaskHandle;
    Task_Params sensorTaskParams;
    Task_Handle commTaskHandle;
    Task_Params commTaskParams;

    // Initialize board
    Board_initGeneral();
    Init6LoWPAN();

    // Initialize i2c bus
    Board_initI2C();

    // Initialize UART
    Board_initUART();

    // Initialize clock
    Clock_Params_init(&clkParams);
    clkParams.period = CYCLE * 1000 / Clock_tickPeriod;
    clkParams.startFlag = FALSE;

    // Take the clock into use in program
    clkHandle = Clock_create((Clock_FuncPtr)clkFxn, 1000000 / Clock_tickPeriod, &clkParams, NULL);
    if (clkHandle == NULL) {
       System_abort("Clock create failed");
    }

    // Open button pins
    button0Handle = PIN_open(&button0State, button0Config);
    if (!button0Handle)
    {
        System_abort("Error initializing button0 pins\n");
    }

    button1Handle = PIN_open(&button1State, button1Config);
    if (!button1Handle)
    {
        System_abort("Error initializing button1 pins\n");
    }

    // Open led pins
    led0Handle = PIN_open(&led0State, led0Config);
    if (!led0Handle)
    {
        System_abort("Error initializing LED0 pins\n");
    }

    led1Handle = PIN_open(&led1State, led1Config);
    if (!led1Handle)
    {
        System_abort("Error initializing LED1 pins\n");
    }

    // Open MPU power pin
    hMpuPin = PIN_open(&MpuPinState, MpuPinConfig);
    if (hMpuPin == NULL) {
        System_abort("Pin open failed!");
    }

    // Buzzer
    hBuzzer = PIN_open(&sBuzzer, cBuzzer);
    if (hBuzzer == NULL) {
      System_abort("Buzzer pin open failed!");
    }

    // Set interruption for button0
    if (PIN_registerIntCb(button0Handle, &button0Fxn) != 0)
    {
        System_abort("Error registering button0 callback function");
    }

    // Set interruption for button1
    if (PIN_registerIntCb(button1Handle, &button1Fxn) != 0)
    {
        System_abort("Error registering button1 callback function");
    }

    /* Tasks */
    Task_Params_init(&mainTaskParams);
    mainTaskParams.stackSize = STACKSIZE;
    mainTaskParams.stack = &mainTaskStack;
    mainTaskParams.priority = 2;
    mainTaskHandle = Task_create(mainTask, &mainTaskParams, NULL);
    if (mainTaskHandle == NULL)
    {
        System_abort("Main task create failed!");
    }

    Task_Params_init(&sensorTaskParams);
    sensorTaskParams.stackSize = STACKSIZE;
    sensorTaskParams.stack = &sensorTaskStack;
    sensorTaskParams.priority = 2;
    sensorTaskHandle = Task_create(sensorTask, &sensorTaskParams, NULL);
    if (sensorTaskHandle == NULL)
    {
        System_abort("Sensor task create failed!");
    }

    Task_Params_init(&commTaskParams);
    commTaskParams.stackSize = STACKSIZE;
    commTaskParams.stack = &commTaskStack;
    commTaskParams.priority = 2;
    commTaskHandle = Task_create(commTask, &commTaskParams, NULL);
    if (commTaskHandle == NULL)
    {
        System_abort("Communication task create failed!");
    }

    /* Sanity check */
    System_printf("Hello world!\n");
    System_flush();

    /* Start BIOS */
    BIOS_start();

    return (0);
}

void buzzerBeep(uint16_t beepDuration, uint16_t pauseDuration, uint16_t beeps) {
    uint16_t *var = &beeps;
    for (; *var > 0; --*var) {
        buzzerOpen(hBuzzer);
        buzzerSetFrequency(1500);
        Task_sleep(beepDuration * 1000 / Clock_tickPeriod);
        buzzerClose();

        Task_sleep(pauseDuration * 1000 / Clock_tickPeriod);
    }

}

void createMessage(uint16_t deviceID, struct activity *activity, char *message) {
    // To know if output empty message
    int8_t emptyMessage = true;

    // Place the id in the begin of string
    snprintf(message, B_MAX_LEN,"id:%d", deviceID);

    // If all values have some data
    if (activity->eat > 0 &&
        activity->exercise > 0 &&
        activity->pet > 0)
    {
        snprintf(message + strlen(message),
                B_MAX_LEN,
                ",ACTIVATE:%d;%d;%d",
                activity->eat,
                activity->exercise,
                activity->pet);

        emptyMessage = false;
    }
    // If two or less have data
    else
    {
        // Append eat data
        if (activity->eat > 0)
        {
            snprintf(message + strlen(message),
                    B_MAX_LEN,
                    ",EAT:%d",
                    activity->eat);

            emptyMessage = false;
        }

        // Append exercise data
        if (activity->exercise > 0)
        {
            snprintf(message + strlen(message),
                    B_MAX_LEN,
                    ",EXERCISE:%d",
                    activity->exercise);

            emptyMessage = false;
        }

        // Append pet data
        if (activity->pet > 0)
        {
            snprintf(message + strlen(message),
                    B_MAX_LEN,
                    ",PET:%d",
                    activity->pet);

            emptyMessage = false;
        }

    }

    if(emptyMessage)
    {
        snprintf(message + strlen(message),
                B_MAX_LEN,
                ",ping");
    }
}

void processMessage(char *message) {

    int16_t msg_id;
    char *token;

    // Try to extract the id from the message
    token = strtok(message, ",");

    if (token == NULL) {
        System_printf("First token NULL, Cannot find ID\n");
        System_flush();
        return;
    }

    // Convert id to integer
    msg_id = atoi(token);

    if (msg_id == BOARD_ID) {
        token = strtok(NULL, ",");

        if (token == NULL) {
            System_printf("Second token NULL, Cannot find BEEP\n");
            System_flush();
            return;
        }

        if (strstr(token, "BEEP") != NULL) {
            buzzerBeep(200, 200, 3);
        }
    }

}
