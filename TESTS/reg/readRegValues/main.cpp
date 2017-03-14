//
// Created by nirao on 09.03.17.
//

#include "mbed.h"
#include "greentea-client/test_env.h"
#include "unity.h"
#include "utest.h"

#include <stdint.h>
#include <Inc/SPIRIT_Radio.h>
#include "mbed.h"

using namespace utest::v1;

// hardware ssel (where applicable)
SPI        spirit1(PTB22, PTB23, PTB21); // mosi, miso, sclk, ssel
DigitalOut spirit1ChipSelect(PTB20);
DigitalOut spirit1Shutdown(PTA18);

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

#define POWER_DBM -5

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
//    dbg_dump("WRTE", response, n_regs);

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

void test_write_reg(){
    spirit1.format(8);

    // reset the module
    SpiritSpiCommandStrobes(COMMAND_SRES);
    // read all registers and dump them
    uint8_t regs[0xf2];
    SpiritSpiReadRegisters(0x00, 0xf2, regs);
    dbg_dump("RESET", regs, 0xf2);

    //write the reg values
    SpiritBaseConfiguration();

    // read all registers again to check they are correct
    SpiritSpiReadRegisters(0x00, 0xf2, regs);
    dbg_dump("CONFI", regs, 0xf2);
}

void get_reg_value(uint8_t address, uint8_t n_regs, uint8_t *buffer){
    uint8_t regBuffer[n_regs];

    SpiritSpiReadRegisters(address, n_regs, regBuffer);

    int ret = memcmp(buffer, regBuffer, n_regs);

    dbg_dump("GETV", regBuffer, n_regs);
    char msg[35];
    sprintf(msg, "Values from reg %d did not match", address);
    TEST_ASSERT_MESSAGE(!ret, "Failed reg read");
}

#define TEST_READ_REG(name, address, n_regs, buffer)         \
void name(){                                                 \
    get_reg_value(address, n_regs, buffer);                  \
}

uint8_t tmp0[1] = {0xCA};
TEST_READ_REG(reg_B2, 0xB2, 1, tmp0)

uint8_t tmp1[1] = {0x00};
TEST_READ_REG(reg_A8, 0xA8, 1, tmp1)

uint8_t tmp2[1] = {0xA3};
TEST_READ_REG(reg_02, 0x02, 1, tmp2)

uint8_t tmp3[7] = {0x36, 0x06, 0x82, 0x54, 0x61, 0x01, 0xAC};
TEST_READ_REG(reg_07, 0x07, 7, tmp3)

uint8_t tmp4[1] = {0x17};
TEST_READ_REG(reg_10, 0x10, 1, tmp4)

uint8_t tmp5[5] = {0x93, 0x09, 0x41, 0x13, 0xC8};
TEST_READ_REG(reg_1A, 0x1A, 5, tmp5)

uint8_t tmp6[1] = {0x64};
TEST_READ_REG(reg_22, 0x22, 1, tmp6)

uint8_t tmp7[1] = {0x62};
TEST_READ_REG(reg_25, 0x25, 1, tmp7)

uint8_t tmp8[1] = {0x15};
TEST_READ_REG(reg_27, 0x27, 1, tmp8)

uint8_t tmp9[2] = {0x1F, 0x40};
TEST_READ_REG(reg_32, 0x32, 2, tmp9)

uint8_t tmp10[5] = {0x12, 0x91, 0xD3, 0x91, 0xD3};
TEST_READ_REG(reg_35, 0x35, 5, tmp10)

uint8_t tmp11[3] = {0x41, 0x40, 0x01};
TEST_READ_REG(reg_4F, 0x4F, 3, tmp11)

uint8_t tmp12[1] = {0x09};
TEST_READ_REG(reg_53, 0x53, 1, tmp12)

uint8_t tmp13[2] = {0x3E, 0x3F};
TEST_READ_REG(reg_6E, 0x6E, 2, tmp13)

uint8_t tmp14[1] = {0x02};
TEST_READ_REG(reg_92, 0x92, 1, tmp14)

uint8_t tmp15[1] = {0xA0};
TEST_READ_REG(reg_9F, 0x9F, 1, tmp15)

uint8_t tmp16[1] = {0x25};
TEST_READ_REG(reg_A1, 0xA1, 1, tmp16)

uint8_t tmp17[1] = {0x35};
TEST_READ_REG(reg_A3, 0xA3, 1, tmp17)

uint8_t tmp18[1] = {0x98};
TEST_READ_REG(reg_A5, 0xA5, 1, tmp18)

uint8_t tmp19[1] = {0x22};
TEST_READ_REG(reg_BC, 0xBC, 1, tmp19)


// Test setup
utest::v1::status_t test_setup(const size_t number_of_cases) {
    GREENTEA_SETUP(10, "default_auto");
    return verbose_test_setup_handler(number_of_cases);
}

Case cases[] = {
        Case("initialize Reg Test", test_write_reg),
        Case("First Reg Test", reg_B2),
        Case("second Reg Test", reg_A8),
        Case("GPIO", reg_02),
        Case("Channel", reg_07),
        Case("PA Power", reg_10),
        Case("Radio", reg_1A),
        Case("RSSI", reg_22),
        Case("AGC Ctrl", reg_25),
        Case("ANT Sel Conf", reg_27),
        Case("PCKT Cntrl", reg_32),
        Case("PCKT n SYNC", reg_35),
        Case("PCKT FLT Protocol", reg_4F),
        Case("Timers", reg_53),
        Case("RCO VCO Caliber", reg_6E),
        Case("IRQ Mask", reg_92),
        Case("SYNTH Config", reg_9F),
        Case("VCO Config", reg_A1),
        Case("DEM Config", reg_A3),
        Case("PM Config", reg_A5),
        Case("VCO Caliber", reg_BC),
};

Specification specification(test_setup, cases);

int main() {
    return !Harness::run(specification);
}
