//
// Created by nirao on 09.03.17.
//
#include <stdint.h>
#include <Inc/SPIRIT_Radio.h>
#include "mbed.h"

#include "MCU_Interface.h"
#include "spirit1Driver.h"


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