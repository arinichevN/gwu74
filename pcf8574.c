
#include "main.h"
#define PCF8574_SAFE_DATA 0xff
void pcf8574_pinMode(Device *dev, int pin, int mode) {
    int bit, old;

    bit = 1 << (pin & 7);

    old = dev->old_data;
    if (mode == OUTPUT) {
        old &= (~bit); // Write bit to 0
    } else {
        old |= bit; // Write bit to 1 ground
    }
    I2CWrite(dev->fd, old);
    dev->old_data = old;
}

void pcf8574_digitalWrite(Device *dev, int pin, int value) {
    int bit, old;
    bit = 1 << (pin & 7);
    old = dev->old_data;
    if (value == LOW) {
        old &= (~bit);
    } else {
        old |= bit;
    }
    I2CWrite(dev->fd, old);
    dev->old_data = old;
}

int pcf8574_read(Device *dev) {
    #ifdef P_A20
    return I2CRead(dev->fd);
    #endif
}

int pcf8574_getpinval(int data, int pin) {
    int mask;
    mask = 1 << (pin & 7);
    if ((data & mask) == 0) {
        return LOW;
    } else {
        return HIGH;
    }
}

void pcf8574_updatenew(Device *dev, int pin, int value) {
    int bit;
    bit = 1 << (pin & 7);
    if (value == LOW) {
        dev->new_data &= (~bit);
    } else {
        dev->new_data |= bit;
    }
}

void pcf8574_write(Device *dev) {
     #ifdef P_A20
    I2CWrite(dev->fd, dev->new_data);
     #endif
    dev->old_data = dev->new_data;
    
}
