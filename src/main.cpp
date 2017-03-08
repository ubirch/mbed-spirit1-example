#include <stdint.h>
#include <Inc/SPIRIT_Radio.h>
#include "mbed.h"

// hardware ssel (where applicable)
SPI spirit1(PTB22, PTB23, PTB21); // mosi, miso, sclk, ssel
DigitalOut spirit1ChipSelect(PTB20);
DigitalOut spirit1Shutdown(PTA18);

DigitalOut    led1(LED1);

//PTC7 == GPIO1 -- SPIRIT i/o 1

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

#define POWER_DBM -5


//+++++++++++++++++++
//imported from example proj

/*  Packet configuration parameters  */
#define PREAMBLE_LENGTH             PKT_PREAMBLE_LENGTH_08BYTES
#define SYNC_LENGTH                 PKT_SYNC_LENGTH_4BYTES
#define SYNC_WORD                   0x88888888
#define LENGTH_TYPE                 PKT_LENGTH_VAR
#define LENGTH_WIDTH                8
#define CRC_MODE                    PKT_CRC_MODE_8BITS
#define CONTROL_LENGTH              PKT_CONTROL_LENGTH_0BYTES
#define EN_ADDRESS                  S_DISABLE
#define EN_FEC                      S_DISABLE
#define EN_WHITENING                S_ENABLE

/*  Addresses configuration parameters  */
#define EN_FILT_MY_ADDRESS          S_DISABLE
#define MY_ADDRESS                  0x00
#define EN_FILT_MULTICAST_ADDRESS   S_DISABLE
#define MULTICAST_ADDRESS           0xEE
#define EN_FILT_BROADCAST_ADDRESS   S_DISABLE
#define BROADCAST_ADDRESS           0xFF
#define DESTINATION_ADDRESS         0x01

/**
* @brief Packet Basic structure fitting
*/
PktBasicInit xBasicInit={
        PREAMBLE_LENGTH,
        SYNC_LENGTH,
        SYNC_WORD,
        LENGTH_TYPE,
        LENGTH_WIDTH,
        CRC_MODE,
        CONTROL_LENGTH,
        EN_ADDRESS,
        EN_FEC,
        EN_WHITENING
};


/**
* @brief Address structure fitting
*/
PktBasicAddressesInit xAddressInit={
        EN_FILT_MY_ADDRESS,
        MY_ADDRESS,
        EN_FILT_MULTICAST_ADDRESS,
        MULTICAST_ADDRESS,
        EN_FILT_BROADCAST_ADDRESS,
        BROADCAST_ADDRESS
};


//++++++++++++++++++++++++++++++++++++++

void SpiritBaseConfiguration(void);
void SpiritVcoCalibration(void);

StatusBytes RadioSpiWriteRegisters(uint8_t address, uint8_t n_regs, uint8_t *buffer) {
    static uint16_t tmpstatus;

    StatusBytes *status = (StatusBytes *) &tmpstatus;

    spirit1.lock();
    spirit1ChipSelect = 0;

    tmpstatus = (uint16_t) (spirit1.write(WRITE_HEADER) << 8 | spirit1.write(address));

//    printf("WRTE %04x=%x (%d)\r\n", address, status, n_regs);
    uint8_t response[n_regs];
    for (int i = 0; i < n_regs; i++) response[i] = (uint8_t) spirit1.write(buffer[i]);
    dbg_dump("WRTE", response, n_regs);

    spirit1ChipSelect = 1;
    spirit1.unlock();

    return *status;
}

StatusBytes RadioSpiReadRegisters(uint8_t address, uint8_t n_regs, uint8_t *buffer) {
    static uint16_t tmpstatus;

    StatusBytes *status = (StatusBytes *) &tmpstatus;
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

    StatusBytes *status = (StatusBytes *) &tmpstatus;

    spirit1.lock();
    spirit1ChipSelect = 0;

    tmpstatus = (uint16_t) (spirit1.write(COMMAND_HEADER) << 8 | spirit1.write(cmd_code));

    spirit1ChipSelect = 1;
    spirit1.unlock();

    return *status;
}

StatusBytes RadioSpiWriteFifo(uint8_t n_regs, uint8_t *buffer) {

    static uint16_t tmpstatus;

    StatusBytes *status = (StatusBytes *) &tmpstatus;

    spirit1.lock();
    spirit1ChipSelect = 0;

    tmpstatus = (uint16_t) (spirit1.write(WRITE_HEADER) << 8 | spirit1.write(LINEAR_FIFO_ADDRESS));

//    printf("WRTE %04x=%x (%d)\r\n", address, status, n_regs);
    uint8_t response[n_regs];
    for (int i = 0; i < n_regs; i++) response[i] = (uint8_t) spirit1.write(buffer[i]);
    dbg_dump("WRITE", response, n_regs);

    spirit1ChipSelect = 1;
    spirit1.unlock();

    return *status;
}

StatusBytes RadioSpiReadFifo(uint8_t n_regs, uint8_t *buffer) {
    static uint16_t tmpstatus;

    StatusBytes *status = (StatusBytes *) &tmpstatus;

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
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/* ******************************************************************** */

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/*  Radio configuration parameters  */
#define XTAL_OFFSET_PPM             0

#ifdef USE_VERY_LOW_BAND
#define BASE_FREQUENCY              169.0e6
#endif

#ifdef USE_LOW_BAND
#define BASE_FREQUENCY              315.0e6
#endif

#ifdef USE_MIDDLE_BAND
#define BASE_FREQUENCY              433.0e6
#endif

//#ifdef USE_HIGH_BAND
#define BASE_FREQUENCY              868.0e6
//#endif

#define CHANNEL_SPACE               20e3
#define CHANNEL_NUMBER              0
#define MODULATION_SELECT           FSK
#define DATARATE                    38400
#define FREQ_DEVIATION              20e3
#define BANDWIDTH                   100E3

#define POWER_DBM                   -5 /* max is 11.6 */


SRadioInit xRadioInit = {
        XTAL_OFFSET_PPM,
        BASE_FREQUENCY,
        CHANNEL_SPACE,
        CHANNEL_NUMBER,
        MODULATION_SELECT,
        DATARATE,
        FREQ_DEVIATION,
        BANDWIDTH
};

void led_thread(void const *args) {
    while (true) {
        led1 = !led1;
        Thread::wait(1000);
    }
}

osThreadDef(led_thread, osPriorityNormal, DEFAULT_STACK_SIZE);


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
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    wait(1);

    SpiritRadioSetXtalFrequency(50000000);
    SpiritRadioInit(&xRadioInit);//    SpiritManagementWaVcoCalibration();

//    /* Spirit Radio config */
//    SpiritRadioInit(&xRadioInit);
//
//    /* Spirit Radio set power */
    SpiritRadioSetPALeveldBm(7,POWER_DBM);
    SpiritRadioSetPALevelMaxIndex(7);
//
//    /* Spirit Packet config */
    SpiritPktBasicInit(&xBasicInit);
    SpiritPktBasicAddressesInit(&xAddressInit);
//
//    /* Spirit IRQs enable */
//    SpiritIrqDeInit(NULL);
//    SpiritIrq(TX_DATA_SENT , S_ENABLE);
//
//    /* payload length config */
//    SpiritPktBasicSetPayloadLength(20);
//
//    /* destination address.
//    By default it is not used because address is disabled, see struct xAddressInit*/
    SpiritPktBasicSetDestinationAddress(0x01);
//
//    /* IRQ registers blanking */
//    SpiritIrqClearStatus();

//    * fit the TX FIFO */
//                 SpiritCmdStrobeFlushTxFifo();
//    SpiritSpiWriteLinearFifo(20, vectcTxBuff);
//
//    /* send the TX command */
//    SpiritCmdStrobeTx();

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    /* Spirit Radio set power */
//    SpiritRadioSetPALeveldBm(7,POWER_DBM);
//    SpiritRadioSetPALevelMaxIndex(7);

//    SpiritTimerSetRxTimeoutMs();

    SRadioInit xradio;
    SpiritRadioGetInfo(&xradio);
    printf("the radio config %d, %4x, %4x, %4x\r\n", xradio.cChannelNumber,
           xradio.lFreqDev, xradio.lBandwidth, xradio.nChannelSpace);


//    SET_INFINITE_RX_TIMEOUT();
//    SpiritTimerSetRxTimeoutStopCondition(SQI_ABOVE_THRESHOLD);
//    SpiritTimerSetRxTimeoutStopCondition(SQI_ABOVE_THRESHOLD);

//    SpiritPktBasicSetDestinationAddress(0x01);

    while (1) {

        uint8_t num = 7;
        uint8_t tx_buff[num] = "HELLO";
        SpiritPktBasicSetPayloadLength(num);

        SpiritCmdStrobeFlushTxFifo();
        SpiritSpiWriteLinearFifo(num, tx_buff);

        SpiritCmdStrobeTx();
/* wait for the Tx Done IRQ. Here it just sets the
tx_data_sent_flag to 1 */
//        while(!tx_data_sent_flag);
//        tx_data_sent_flag= 0;


        // SpiritCmdStrobeRx();
//        while(!rx_data_received_flag);
//        rx_data_received_flag= 0;
        // uint8_t N = SpiritLinearFifoReadNumElementsRxFifo();
        // if (N) {
        //     uint8_t *buffer = (uint8_t *) malloc(N);
        //     SpiritSpiReadLinearFifo(N, buffer);
        // }

        led1 = !led1;
        Thread::wait(2000);
    }
//    uint32_t runBootloaderAddress = **(uint32_t **) (0x1c00001c);
//    ((void (*)(void *arg)) runBootloaderAddress)(NULL);
}
}

//TODO put the device into ready state and red in blocking / non blocking mode
//


