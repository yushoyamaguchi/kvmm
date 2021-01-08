#include "uart.h"

extern int outfd;

struct uart *uart;

void create_uart() {
    uart = malloc(sizeof(struct uart));
    uart->data_reg = 0;
    uart->irr_enable_reg = 0;
    uart->irr_id_reg = 0;
    uart->line_control_reg = 0;
    uart->modem_control_reg = 0;
    uart->line_status_reg = 0;
    uart->modem_status_reg = 0;
    uart->scratch_reg = 0;
    uart->lock = 0;
    uart->buff_index = 0;
    uart->buff = malloc(10);
}

void set_uart_data_reg() {
    uart->data_reg = uart->buff[0];
}

void clear_uart_data_reg() {
    uart->data_reg = 0;
    for (int i = 0; i < uart->buff_index; i++)
        uart->buff[i] = uart->buff[i+1];

    if (uart->buff_index > 0) 
        uart->buff_index--;
}

void set_uart_lock() {
    uart->lock = 1;
}

void set_uart_unlock() {
    uart->lock = 0;
}

void set_uart_buff(char value) {
    uart->buff[uart->buff_index] = value;
    uart->buff_index++;
}

void emulate_uart_portw(struct vcpu *vcpu, int port, int count, int size) {
    switch (port) {
    case 0x3f8:
        for (int i = 0; i < count; i++) {
            char *v = (char*)((unsigned char*)vcpu->kvm_run + vcpu->kvm_run->io.data_offset);
            printf("%c", *v);
            write(outfd, v, 1);
            //uart->data_reg = *v;
            vcpu->kvm_run->io.data_offset += size;
        }
        break;
    case 0x3f9:
        uart->irr_enable_reg = *(u8*)((u8*)vcpu->kvm_run
         + vcpu->kvm_run->io.data_offset);
    case 0x3fd:
        uart->line_status_reg = *(u8*)((u8*)vcpu->kvm_run
         + vcpu->kvm_run->io.data_offset);
        break;
    default:
        break;
    }
}

void emulate_uart_portr(struct vcpu *vcpu, int port) {
    switch (port) {
    case 0x3f8:
        set_uart_data_reg();

        *(unsigned char*)((unsigned char*)vcpu->kvm_run
         + vcpu->kvm_run->io.data_offset) = (unsigned char)uart->data_reg;
        
        // clear data register
        clear_uart_data_reg();
        break;
    case 0x3fd:
        *(unsigned char*)((unsigned char*)vcpu->kvm_run
         + vcpu->kvm_run->io.data_offset) = uart->line_status_reg; 
        break;
    default:
        break;
    }
}