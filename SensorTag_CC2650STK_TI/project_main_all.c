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

/* Board and peripheral Header files */
#include "Board.h"
#include "wireless/comm_lib.h"
#include "sensors/opt3001.h"
#include "sensors/mpu9250.h"
#include "peripherals/buzzer.h"

/* Task */
#define STACKSIZE 2048
Char sensorTaskStack[STACKSIZE];
Char uartTaskStack[STACKSIZE];
Char mpuTaskStack[STACKSIZE];

#define B_MAX_LEN 80    // Define maximum length for buffer.

// Definition of the state machine states
enum state
{
    WAITING_HOME = 1, //
    WAITING_READ,   // Waiting the time interruption to read the MPU sensor
    READ_MPU,       // Reads the MPU sensor
    WRITE_UART     // Write data to UART
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
static PIN_Handle hBuzzer;
static PIN_State sBuzzer;


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

// Pin config for buzzer
PIN_Config cBuzzer[] = {
  Board_BUZZER | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
  PIN_TERMINATE
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

// Define structure for activity data
struct activity {
    uint8_t eat;
    uint8_t exercise;
    uint8_t pet;
};

// Define global variable for activity data
struct activity activity = {0, 0, 0};

// Function prototypes

// Make beep sound with buzzer
// Durations in milliseconds
void buzzerBeep(uint16_t beepDuration, uint16_t pauseDuration, uint16_t beeps);

// Create message to be sent to host
void createMessage(uint8_t *deviceID, struct activity *activity, char *message);


//State machine pseudo code

/* States:
 * WAITING
 * DETECT_MOVEMENT
 * PROCESS_MESSAGE
 * COUNT_REPEATS
 * DETECT_EATING
 * UPDATE_STATUS_TO_HOST
 *
 * MPU States:
 * MPU_WAITING
 * MPU_READ
 *
 * Communication states:
 * COMM_WAITING
 * PROCESS MESSAGE
 */
//Main:

//  If(programState == WAITING)
//      Wait for request from button0 or communication

//      if(button0)
//          Turn on led
//          (make beep)
//          programState = DETECT_MOVEMENT

//      else if(newDataReceived)
//          programState = PROCESS_MESSAGE

//      (Possible extra function:
//      Read other sensors and send data.)

// Interruptions
//clockFxn:
//  if(programState == DETECT_MOVEMENT ||
//      programState == COUNT_REPEATS ||
//      programState == DETECT_EATING)

//      requestRead = true // or maybe another option is to call directly mpu_get_data()

    // If multiples of clock wanted just make request on certain counter intervals
//      if(counter1 > 1)
//          request1 = true // Clear request where used
//          counter1 = 0
//      else
//          counter1++

//      if(counter2 > 2)
//          request2 = true // Clear request where used
//          counter2 = 0
//      else
//          counter2++

//buttonFxn:
//  If(programState == WAITING)
//      Request state change to detect movement
//      or change state directly? Which one is better.

//  Else if(programState == COUNT_REPEATS)
//      Request state change to update status to host.

//  else if(programState == DETECT_MOVEMENT)
//      Request state change to DETECT_EATING

//Normal tasks
//commTask:
//  if(programState == PROCESS_MESSAGE)
        // Process incoming/received message
//      processMessage()

//  else if(programState == UPDATE_STATUS_TO_HOST)
//      createMessage
//      sendMessageUart

//      if(wirelessEnabled) // Optional
//          sendMessage6LowPan


//sensorTask:
//  if(programState == DETECT_MOVEMENT)
//      detectMovement()

//      if(activity.pet > 0 ||
//          activity.exercise > 0)
//          programState == COUNT_REPEATS

//      else if(button?)
//          programState == DETECT_EATING

//  else if(programState == COUNT_REPEATS)
//      if(button)
//          programState = UPDATE_STATUS_TO_HOST

//      else if(activity.pet > 0 &&
//              detectMovement() == pet)
//          activity.pet++

//      else if(activity.exercise > 0 &&
//              detectMovement() == exercise)
//          activity.exercise++

//  else if(programState == DETECT_EATING)
//      if(button)
//          programState = UPDATE_STATUS_TO_HOST

//      else if(detectMovement() == eat)
//          activity.eat++




// Napinpainalluksen keskeytyksen käsittelijäfunktio
void buttonFxn(PIN_Handle handle, PIN_Id pinId)
{
    // Luetaan ledin tila
    uint_t pinValue = PIN_getOutputValue( Board_LED0);

    if (programState == WAITING_HOME) {

        //Vaihdetaan ledin tila
        pinValue = 1;
        programState = WAITING_READ;

    } else {

        //Vaihdetaan ledin tila
        pinValue = 0;
        programState = WAITING_HOME;
    }

    PIN_setOutputValue(ledHandle, Board_LED0, pinValue);

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

        // Print out the sensor data to uart
        if (programState == WRITE_UART)
        {

            sprintf(merkkijono, "Uart:%f\n", ambientLight);
            System_printf(merkkijono);
            System_flush();

            // Write the string to uart
            sprintf(merkkijono, "%s\r", merkkijono);
            UART_write(uart, merkkijono, strlen(merkkijono));


            //Check the state before update
            if (programState == WRITE_UART) {
                programState = WAITING_READ;
            } else {
                programState = WAITING_HOME;
            }

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
        Task_sleep(100000 / Clock_tickPeriod);
    }
}

Void sensorTaskFxn(UArg arg0, UArg arg1)
{

    I2C_Handle i2c;
    I2C_Params i2cParams;

    // Alustetaan i2c-väylä
    I2C_Params_init(&i2cParams);
    i2cParams.bitRate = I2C_400kHz;



    while (1)
    {

        // Read the ambient light sensor to variable
        if (programState == WAITING_READ)
        {
            // Avataan yhteys
            i2c = I2C_open(Board_I2C_TMP, &i2cParams);
            if (i2c == NULL)
            {
                System_abort("Error Initializing I2C\n");
            }

            Task_sleep(100000 / Clock_tickPeriod);

            //Initialize opt3001 light sensor
            opt3001_setup(&i2c);

            //Delay the data read to get successful reading
            Task_sleep(1000000 / Clock_tickPeriod);

            ambientLight = opt3001_get_data(&i2c);

            I2C_close(i2c);

            sprintf(merkkijono, "Sensor:%f\n", ambientLight);
            System_printf(merkkijono);
            System_flush();

            //Check the state before update
            if (programState == WAITING_READ) {
                programState = READ_MPU;
            } else {
                programState = WAITING_HOME;
            }

        }
        /*
        //debug
        sprintf(merkkijono,"programState %d\n",programState);
        System_printf(merkkijono);
        System_printf("SensorTask\n");
        System_flush();
        */

        // Once per second, you can modify this
        Task_sleep(1000000 / Clock_tickPeriod);
    }
}

Void mpuSensorFxn(UArg arg0, UArg arg1) {

    float ax, ay, az, gx, gy, gz;

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

    // Loop forever
    while (1) {

        if (programState == READ_MPU) {
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

            // MPU ask data
            mpu9250_get_data(&i2cMPU, &ax, &ay, &az, &gx, &gy, &gz);

            // MPU close i2c
            I2C_close(i2cMPU);

            sprintf(merkkijono, "mpu:%+4.2f,%+4.2f,%+4.2f,%+4.2f,%+4.2f,%+4.2f\n", ax, ay, az, gx, gy, gz);
            System_printf(merkkijono);
            System_flush();

            //Check the state before update
            if (programState == READ_MPU) {
                programState = WRITE_UART;
            } else {
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

    // Task variables
    Task_Handle sensorTaskHandle;
    Task_Params sensorTaskParams;
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

    // Buzzer
    hBuzzer = PIN_open(&sBuzzer, cBuzzer);
    if (hBuzzer == NULL) {
      System_abort("Buzzer pin open failed!");
    }

    // Asetetaan painonappi-pinnille keskeytyksen käsittelijäksi
    // funktio buttonFxn
    if (PIN_registerIntCb(buttonHandle, &buttonFxn) != 0)
    {
        System_abort("Error registering button callback function");
    }

    /* Task */
    Task_Params_init(&sensorTaskParams);
    sensorTaskParams.stackSize = STACKSIZE;
    sensorTaskParams.stack = &sensorTaskStack;
    sensorTaskParams.priority = 2;
    sensorTaskHandle = Task_create(sensorTaskFxn, &sensorTaskParams, NULL);
    if (sensorTaskHandle == NULL)
    {
        System_abort("Sensor task create failed!");
    }

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
    mpuTask = Task_create((Task_FuncPtr)mpuSensorFxn, &mpuTaskParams, NULL);
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

void createMessage(uint8_t *deviceID, struct activity *activity, char *message) {
    // To know if output empty message
    int8_t emptyMessage = true;

    // Place the id in the begin of string
    snprintf(message, B_MAX_LEN,"id:%d", *deviceID);

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
