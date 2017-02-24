#include <stdint.h>
#include <Inc/SPIRIT_Radio.h>
#include "mbed.h"

// hardware ssel (where applicable)
SPI spirit1(PTB22, PTB23, PTB21); // mosi, miso, sclk, ssel
DigitalOut spirit1ChipSelect(PTB20);
DigitalOut spirit1Shutdown(PTA18);

void dbg_dump(const char *prefix, const uint8_t *b, size_t size) {
    for (int i = 0; i < size; i += 16) {
        if (prefix && strlen(prefix) > 0) printf("%s %06x: ", prefix, i);
        for (int j = 0; j < 16; j++) {
            if ((i + j) < size) printf("%02x", b[i + j]); else printf("  ");
            if ((j + 1) % 2 == 0) putchar(' ');
        }
        putchar(' ');
        for (int j = 0; j < 16 && (i + j) < size; j++) {
            putchar(b[i + j] >= 0x20 && b[i + j] <= 0x7E ? b[i + j] : '.');
        }
        printf("\r\n");
    }
}

extern "C" {

/* list of the command codes of SPIRIT1 */
#define    COMMAND_TX                                          ((uint8_t)(0x60)) /*!< Start to transmit; valid only from READY */
#define    COMMAND_RX                                          ((uint8_t)(0x61)) /*!< Start to receive; valid only from READY */
#define    COMMAND_READY                                       ((uint8_t)(0x62)) /*!< Go to READY; valid only from STANDBY or SLEEP or LOCK */
#define    COMMAND_STANDBY                                     ((uint8_t)(0x63)) /*!< Go to STANDBY; valid only from READY */
#define    COMMAND_SLEEP                                       ((uint8_t)(0x64)) /*!< Go to SLEEP; valid only from READY */
#define    COMMAND_LOCKRX                                      ((uint8_t)(0x65)) /*!< Go to LOCK state by using the RX configuration of the synth; valid only from READY */
#define    COMMAND_LOCKTX                                      ((uint8_t)(0x66)) /*!< Go to LOCK state by using the TX configuration of the synth; valid only from READY */
#define    COMMAND_SABORT                                      ((uint8_t)(0x67)) /*!< Force exit form TX or RX states and go to READY state; valid only from TX or RX */
#define    COMMAND_SRES                                        ((uint8_t)(0x70)) /*!< Reset of all digital part, except SPI registers */
#define    COMMAND_FLUSHRXFIFO                                 ((uint8_t)(0x71)) /*!< Clean the RX FIFO; valid from all states */
#define    COMMAND_FLUSHTXFIFO                                 ((uint8_t)(0x72)) /*!< Clean the TX FIFO; valid from all states */

#define HEADER_WRITE_MASK     0x00 /*!< Write mask for header byte*/
#define HEADER_READ_MASK      0x01 /*!< Read mask for header byte*/
#define HEADER_ADDRESS_MASK   0x00 /*!< Address mask for header byte*/
#define HEADER_COMMAND_MASK   0x80 /*!< Command mask for header byte*/

#define LINEAR_FIFO_ADDRESS 0xFF  /*!< Linear FIFO address*/

#define BUILT_HEADER(add_comm, w_r) (add_comm | w_r)  /*!< macro to build the header byte*/
#define WRITE_HEADER    BUILT_HEADER(HEADER_ADDRESS_MASK, HEADER_WRITE_MASK) /*!< macro to build the write header byte*/
#define READ_HEADER     BUILT_HEADER(HEADER_ADDRESS_MASK, HEADER_READ_MASK)  /*!< macro to build the read header byte*/
#define COMMAND_HEADER  BUILT_HEADER(HEADER_COMMAND_MASK, HEADER_WRITE_MASK) /*!< macro to build the command header byte*/

void SpiritBaseConfiguration(void);
void SpiritVcoCalibration(void);

StatusBytes RadioSpiWriteRegisters(uint8_t address, uint8_t n_regs, uint8_t *buffer) {
    static uint16_t tmpstatus;

    StatusBytes* status=(StatusBytes*)&tmpstatus;

    spirit1.lock();
    spirit1ChipSelect = 0;

    tmpstatus = (uint16_t) (spirit1.write(WRITE_HEADER) << 8 | spirit1.write(address));

//    printf("WRTE %04x=%x (%d)\r\n", address, status, n_regs);
    uint8_t response[n_regs];
    for (int i = 0; i < n_regs; i++) response[i] = (uint8_t) spirit1.write(buffer[i]);
//    dbg_dump("WRTE", response, n_regs);

    spirit1ChipSelect = 1;
    spirit1.unlock();

    return *status;
}

StatusBytes RadioSpiReadRegisters(uint8_t address, uint8_t n_regs, uint8_t *buffer) {
    static uint16_t tmpstatus;

    StatusBytes* status=(StatusBytes*)&tmpstatus;
    spirit1.lock();
    spirit1ChipSelect = 0;

    tmpstatus = (uint16_t) (spirit1.write(READ_HEADER) << 8 | spirit1.write(address));

//    printf("READ %04x=%x (%d)\r\n", address, status, n_regs);
    for (int i = 0; i < n_regs; i++) buffer[i] = (uint8_t) spirit1.write(0);
//    dbg_dump("READ", buffer, n_regs);

    spirit1ChipSelect = 1;
    spirit1.unlock();

    return *status;

}

StatusBytes RadioSpiCommandStrobes(uint8_t cmd_code) {
    static uint16_t tmpstatus;

    StatusBytes* status=(StatusBytes*)&tmpstatus;

    spirit1.lock();
    spirit1ChipSelect = 0;

    tmpstatus = (uint16_t) (spirit1.write(COMMAND_HEADER) << 8 | spirit1.write(cmd_code));

    spirit1ChipSelect = 1;
    spirit1.unlock();

    return *status;
}

StatusBytes RadioSpiWriteFifo(uint8_t n_regs, uint8_t* buffer){

    static uint16_t tmpstatus;

    StatusBytes* status=(StatusBytes*)&tmpstatus;

    spirit1.lock();
    spirit1ChipSelect = 0;

    tmpstatus = (uint16_t) (spirit1.write(WRITE_HEADER) << 8 | spirit1.write(LINEAR_FIFO_ADDRESS));

//    printf("WRTE %04x=%x (%d)\r\n", address, status, n_regs);
    uint8_t response[n_regs];
    for (int i = 0; i < n_regs; i++) response[i] = (uint8_t) spirit1.write(buffer[i]);
//    dbg_dump("WRTE", response, n_regs);

    spirit1ChipSelect = 1;
    spirit1.unlock();

    return *status;
}

StatusBytes RadioSpiReadFifo(uint8_t n_regs, uint8_t* buffer){
    static uint16_t tmpstatus;

    StatusBytes* status=(StatusBytes*)&tmpstatus;

    spirit1.lock();
    spirit1ChipSelect = 0;

    tmpstatus = (uint16_t) (spirit1.write(READ_HEADER) << 8 | spirit1.write(LINEAR_FIFO_ADDRESS));

    printf("READ %04x=%x (%d)\r\n", LINEAR_FIFO_ADDRESS, status, n_regs);
    for (int i = 0; i < n_regs; i++) buffer[i] = (uint8_t) spirit1.write(0);
    dbg_dump("READ", buffer, n_regs);

    spirit1ChipSelect = 1;
    spirit1.unlock();

    return *status;
}

void SP1_Enter_Pers_RX_mode(void)
{
   SpiritRadioPersistenRx(S_ENABLE);
   SpiritCmdStrobeRx();
}

DigitalOut    led1(LED1);

void led_thread(void const *args) {
    while (true) {
        led1 = !led1;
        Thread::wait(1000);
    }
}

osThreadDef(led_thread,   osPriorityNormal, DEFAULT_STACK_SIZE);

int main() {
    osThreadCreate(osThread(led_thread), NULL);

    printf("SPIRIT1 read from trackle\r\n");
    // set the
    spirit1.format(8);

    // reset the module
    SpiritSpiCommandStrobes(COMMAND_SRES);

    // read all registers and dump them
    uint8_t regs[0xf2];
    SpiritSpiReadRegisters(0x00, 0xf2, regs);
    dbg_dump("RESET", regs, 0xf2);

    // configure the module
    SpiritBaseConfiguration();

    // read all registers again to check they are correct
    SpiritSpiReadRegisters(0x00, 0xf2, regs);
    dbg_dump("CONFI", regs, 0xf2);

    wait(1);

    uint8_t buffer[96];

printf("now persistent mode \n");
//    SpiritTimerSetRxTimeoutMs();
//    SP1_Enter_Pers_RX_mode();

    SET_INFINITE_RX_TIMEOUT();

    SpiritTimerSetRxTimeoutStopCondition(SQI_ABOVE_THRESHOLD);


//    SpiritTimerSetRxTimeoutStopCondition(SQI_ABOVE_THRESHOLD);


    while (1) {
        SpiritCmdStrobeRx();

        SpiritSpiReadLinearFifo(96, buffer);
        led1 = !led1;
        Thread::wait(2000);
    }
//    uint32_t runBootloaderAddress = **(uint32_t **) (0x1c00001c);
//    ((void (*)(void *arg)) runBootloaderAddress)(NULL);
}
}

//TODO put the device into ready state and red in blocking / non blocking mode
//