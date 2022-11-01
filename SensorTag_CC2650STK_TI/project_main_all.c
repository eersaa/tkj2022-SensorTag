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
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>
#include <ti/drivers/UART.h>

/* Board Header files */
#include "Board.h"
#include "wireless/comm_lib.h"
#include "sensors/opt3001.h"

/* Task */
#define STACKSIZE 2048
Char sensorTaskStack[STACKSIZE];
Char uartTaskStack[STACKSIZE];

// JTKJ: Teht�v� 3. Tilakoneen esittely
// JTKJ: Exercise 3. Definition of the state machine
enum state
{
    WAITING_HOME = 1, //
    WAITING_READ,   // Waiting the time interruption to read the MPU sensor
    READ_MPU,       // Reads the MPU sensor
    WRITE_UART      // Write data to UART
};
enum state programState = WAITING_HOME;

// JTKJ: Teht�v� 3. Valoisuuden globaali muuttuja
// JTKJ: Exercise 3. Global variable for ambient light
double ambientLight = -1000.0;
char merkkijono[20];

// RTOS:n globaalit muuttujat pinnien käyttöön
static PIN_Handle buttonHandle;
static PIN_State buttonState;
static PIN_Handle ledHandle;
static PIN_State ledState;

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

// Napinpainalluksen keskeytyksen käsittelijäfunktio
void buttonFxn(PIN_Handle handle, PIN_Id pinId)
{
    // Luetaan ledin tila
    uint_t pinValue = PIN_getOutputValue( Board_LED0);

    if (programState == WAITING_HOME) {

        //Vaihdetaan ledin tila
        pinValue = !pinValue;
        programState = WAITING_READ;

    } else if (programState == WAITING_READ) {

        //Vaihdetaan ledin tila
        pinValue = !pinValue;
        programState = WAITING_HOME;
    }

    PIN_setOutputValue(ledHandle, Board_LED0, pinValue);

}

/* Task Functions */
Void uartTaskFxn(UArg arg0, UArg arg1)
{

    // JTKJ: Teht�v� 4. Lis�� UARTin alustus: 9600,8n1
    // JTKJ: Exercise 4. Setup here UART connection as 9600,8n1
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

        // JTKJ: Teht�v� 3. Kun tila on oikea, tulosta sensoridata merkkijonossa debug-ikkunaan
        //       Muista tilamuutos
        // JTKJ: Exercise 3. Print out sensor data as string to debug window if the state is correct
        //       Remember to modify state
        if (programState == WRITE_UART)
        {

            sprintf(merkkijono, "Uart:%f\n", ambientLight);
            System_printf(merkkijono);
            System_flush();

            // JTKJ: Teht�v� 4. L�het� sama merkkijono UARTilla
            // JTKJ: Exercise 4. Send the same sensor data string with UART
            sprintf(merkkijono, "%s\r", merkkijono);
            UART_write(uart, merkkijono, strlen(merkkijono));

            programState = WAITING_READ;

        }
        /*
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

    // JTKJ: Teht�v� 2. Avaa i2c-v�yl� taskin k�ytt��n
    // JTKJ: Exercise 2. Open the i2c bus

    // Alustetaan i2c-väylä
    I2C_Params_init(&i2cParams);
    i2cParams.bitRate = I2C_400kHz;

    // Avataan yhteys
    i2c = I2C_open(Board_I2C_TMP, &i2cParams);
    if (i2c == NULL)
    {
        System_abort("Error Initializing I2C\n");
    }

    // JTKJ: Teht�v� 2. Alusta sensorin OPT3001 setup-funktiolla
    //       Laita enne funktiokutsua eteen 100ms viive (Task_sleep)
    // JTKJ: Exercise 2. Setup the OPT3001 sensor for use
    //       Before calling the setup function, insertt 100ms delay with Task_sleep

    Task_sleep(100000 / Clock_tickPeriod);

    //Initialize opt3001 light sensor
    opt3001_setup(&i2c);

    while (1)
    {

        // JTKJ: Teht�v� 2. Lue sensorilta dataa ja tulosta se Debug-ikkunaan merkkijonona
        // JTKJ: Exercise 2. Read sensor data and print it to the Debug window as string

        // JTKJ: Teht�v� 3. Tallenna mittausarvo globaaliin muuttujaan
        //       Muista tilamuutos
        // JTKJ: Exercise 3. Save the sensor value into the global variable
        //       Remember to modify state
        if (programState == WAITING_READ)
        {

            ambientLight = opt3001_get_data(&i2c);

            sprintf(merkkijono, "Sensor:%f\n", ambientLight);
            System_printf(merkkijono);
            System_flush();

            programState = WRITE_UART;
        }
        /*
         sprintf(merkkijono,"programState %d\n",programState);
         System_printf(merkkijono);

         System_printf("SensorTask\n");
         System_flush();
         */
        // Once per second, you can modify this
        Task_sleep(1000000 / Clock_tickPeriod);
    }
}

Int main(void)
{

    // Task variables
    Task_Handle sensorTaskHandle;
    Task_Params sensorTaskParams;
    Task_Handle uartTaskHandle;
    Task_Params uartTaskParams;

    // Initialize board
    Board_initGeneral();
    Init6LoWPAN();

    // JTKJ: Teht�v� 2. Ota i2c-v�yl� k�ytt��n ohjelmassa
    // JTKJ: Exercise 2. Initialize i2c bus
    Board_initI2C();

    // JTKJ: Teht�v� 4. Ota UART k�ytt��n ohjelmassa
    // JTKJ: Exercise 4. Initialize UART
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
        System_abort("Task create failed!");
    }

    Task_Params_init(&uartTaskParams);
    uartTaskParams.stackSize = STACKSIZE;
    uartTaskParams.stack = &uartTaskStack;
    uartTaskParams.priority = 2;
    uartTaskHandle = Task_create(uartTaskFxn, &uartTaskParams, NULL);
    if (uartTaskHandle == NULL)
    {
        System_abort("Task create failed!");
    }

    /* Sanity check */
    System_printf("Hello world!\n");
    System_flush();

    /* Start BIOS */
    BIOS_start();

    return (0);
}
