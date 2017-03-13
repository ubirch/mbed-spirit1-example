#include <stdint.h>
#include <Inc/SPIRIT_Radio.h>
#include <Inc/SPIRIT_Config.h>
#include "mbed.h"

#include "spirit1Driver.h"

#define ENABLETX 0  // Puts the device in TX mode
#define ENABLERX 1  // Puts the device in RX mode

// hardware ssel (where applicable)
SPI spirit1(PTB22, PTB23, PTB21); // mosi, miso, sclk, ssel
DigitalOut spirit1ChipSelect(PTB20);
DigitalOut spirit1Shutdown(PTA18);
InterruptIn spiritInterrupt(PTC11);

DigitalOut    led1(LED1);

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
}

void led_thread(void const *args) {
    while (true) {
        led1 = !led1;
        Thread::wait(1000);
    }
}

osThreadDef(led_thread, osPriorityNormal, DEFAULT_STACK_SIZE);

uint8_t vectcRxBuff[96], cRxData;

void STxIRQH() {
    /* Get the IRQ status */
    SpiritIrqGetStatus(&xIrqStatus);
    printf("In IRQ Handler: %d\r\n", xIrqStatus.IRQ_RX_DATA_READY);

#ifdef ENABLETX
    /* Check the SPIRIT TX_DATA_SENT IRQ flag */
    if (xIrqStatus.IRQ_TX_DATA_SENT) {
        /* set the tx_done_flag to manage the event in the main() */
        xTxDoneFlag = 1;
        printf("xTxDoneFlag \r\n");
    }
#endif

#ifdef ENABLERX
    if (xIrqStatus.IRQ_RX_DATA_DISC) {
        printf("discard\r\n");

        SpiritSpiReadLinearFifo(cRxData, vectcRxBuff);

        /* Flush the RX FIFO */
        SpiritCmdStrobeFlushRxFifo();

        /* RX command - to ensure the device will be ready for the next reception */
        SpiritCmdStrobeRx();
    }

    /* Check the SPIRIT RX_DATA_READY IRQ flag */
    if (xIrqStatus.IRQ_RX_DATA_READY) {
        /* Get the RX FIFO size */
        cRxData = SpiritLinearFifoReadNumElementsRxFifo();

        printf("read from fifo\r\n");
        /* Read the RX FIFO */
        SpiritSpiReadLinearFifo(cRxData, vectcRxBuff);

        /* Flush the RX FIFO */
        SpiritCmdStrobeFlushRxFifo();

        /* RX command - to ensure the device will be ready for the next reception */
        SpiritCmdStrobeRx();
    }
#endif
    SpiritIrqClearStatus();
}

int main() {
    osThreadCreate(osThread(led_thread), NULL);

    SpiritSpiCommandStrobes(COMMAND_SRES);
    printf("SPIRIT1 read from trackle\r\n");

    // set the SPI format
    spirit1.format(8);

    SpiritManagementWaExtraCurrent();

    /* Manually set the XTAL_FREQUENCY */
    SpiritRadioSetXtalFrequency(52000000);

    /*Init GUI generated configuration*/
    SpiritBaseConfiguration();

    //Set the Board IRQ
    spiritInterrupt.fall(&STxIRQH);

    //SPIRIT! IRQ
    SpiritGpioInit(&xGpioIRQ);

#if ENABLETX
    /* Spirit Radio set power */
    SpiritRadioSetPALeveldBm(7, POWER_DBM);
    SpiritRadioSetPALevelMaxIndex(7);
#endif

    /* Spirit IRQs enable */
    SpiritIrqDeInit(NULL);
#if ENABLETX
    SpiritIrq(TX_DATA_SENT, S_ENABLE);
#endif //ENABLETX

#ifdef ENABLERX
    SpiritIrq(RX_DATA_DISC,S_ENABLE);
    SpiritIrq(RX_DATA_READY,S_ENABLE);

    /* enable SQI check */
    SpiritQiSetSqiThreshold(SQI_TH_0);
    SpiritQiSqiCheck(S_ENABLE);

    /* RX timeout config */
    SpiritTimerSetRxTimeoutMs(1000.0);
    SpiritTimerSetRxTimeoutStopCondition(SQI_ABOVE_THRESHOLD);
#endif //ENABLERX

    /* IRQ registers blanking */
    SpiritIrqClearStatus();

    SRadioInit xradio;
    SpiritRadioGetInfo(&xradio);

    printf("radio config:\r\n"
                   "xradio.nXtalOffsetPpm;  :  %d\r\n"
                   "xradio.lFrequencyBase;  :  %d\r\n"
                   "xradio.nChannelSpace;   :  %d\r\n"
                   "xradio.cChannelNumber;  :  %d\r\n"
                   "xradio.xModulationSelect:  %d\r\n"
                   "xradio.lDatarate;       :  %d\r\n"
                   "xradio.lFreqDev;        :  %d\r\n"
                   "xradio.lBandwidth;      :  %d\r\n",
           xradio.nXtalOffsetPpm,
           xradio.lFrequencyBase,
           xradio.nChannelSpace,
           xradio.cChannelNumber,
           xradio.xModulationSelect,
           xradio.lDatarate,
           xradio.lFreqDev,
           xradio.lBandwidth);

    PktBasicInit pxPktBasicInit;
    SpiritPktBasicGetInfo(&pxPktBasicInit);
    printf("Basic Packet Cong:\r\n"
                   "xPreambleLength   %d\r\n"
                   "xSyncLength       %d\r\n"
                   "lSyncWords        %d\r\n"
                   "xFixVarLength     %d\r\n"
                   "cPktLengthWidth   %d\r\n"
                   "xCrcMode          %d\r\n"
                   "xControlLength    %d\r\n"
                   "xAddressField     %d\r\n"
                   "xFec              %d\r\n"
                   "xDataWhitening    %d\r\n",
           pxPktBasicInit.xPreambleLength, pxPktBasicInit.xSyncLength, pxPktBasicInit.lSyncWords,
           pxPktBasicInit.xFixVarLength, pxPktBasicInit.cPktLengthWidth, pxPktBasicInit.xCrcMode,
           pxPktBasicInit.xControlLength, pxPktBasicInit.xAddressField,
           pxPktBasicInit.xFec, pxPktBasicInit.xDataWhitening);

#ifdef ENABLERX
    /* RX command */
    SpiritCmdStrobeRx();
#endif

    while (1) {

#if ENABLETX
        printf("new loop \r\n");
        /* fit the TX FIFO */
        SpiritCmdStrobeFlushTxFifo();
//        SpiritSpiWriteLinearFifo(20, vectcTxBuff);
        SpiritSpiWriteLinearFifo(7, (uint8_t *)"HELLO");

        /* send the TX command */
        SpiritCmdStrobeTx();

        /* wait for TX done */
//        while (!xTxDoneFlag);
        xTxDoneFlag = 0;
        wait(2);
//        Thread::wait(2000);
#endif //ENABLETX
    }
}


//TODO put the device into ready state and red in blocking / non blocking mode


