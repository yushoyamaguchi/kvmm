#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>
#include <stdint.h>
#include <linux/kvm.h>
#include <pthread.h>
#include "util.h"
#include "type.h"
#include "vcpu.h"
#include "blk.h"
#include "io.h"
#include "lapic.h"
#include "ioapic.h"
#include "mmio.h"
#include "uart.h"
#include "vm.h"

// #define GUEST_PATH "../xv6/xv6.img"
// #define START_ADDRESS 0x7c00
// #define GUEST_MEMORY_SIZE 0x80000000
// #define ALIGNMENT_SIZE 0x1000
// #define DEFAULT_FLAGS 0x0000000000000002ULL
// #define GUEST_BINARY_SIZE (4096 * 128)
// #define IMGE_SIZE 5120000
//#define FS_IMAGE_SIZE (500 * 1024)
// #define LAPIC_BASE 0xfee00000
// #define IOAPIC_BASE 0xfec00000
// #define IOAPIC_REDRTB_BASE (IOAPIC_BASE + 0x10)
// #define IRQ_BASE 32

// struct vm {
//     int vm_fd;
//     int fd;
//     void *mem;
// };

// struct vcpu {
//     int fd;
//     struct kvm_run *kvm_run;
//     struct kvm_sregs sregs;
//     struct kvm_regs regs;
// };

// struct blk {
//     u8 *data;
//     u16 data_reg;
//     u8 sec_count_reg;
//     u8 lba_low_reg;
//     u8 lba_middle_reg;
//     u8 lba_high_reg;
//     u8 drive_head_reg;
//     u8 status_command_reg;
//     u32 index;
//     u8 dev_conotrl_regs;
// };

// struct io {
//     __u8 direction;
//     __u8 size;
//     __u16 port;
//     __u32 count;
//     __u64 data_offset; 
// };

// struct mmio {
//     __u64 phys_addr;
// 	__u8  data[8];
// 	__u32 len;
// 	__u8  is_write;
// };

// struct uart {
//     u8 data_reg;
//     u8 irr_enable_reg;
//     u8 irr_id_reg;
//     u8 line_control_reg;
//     u8 modem_control_reg;
//     u8 line_status_reg;
//     u8 modem_status_reg;
//     u8 scratch_reg;
// };

// union redirtb_entry {
//     struct {
//         u64 vec : 8;
//         u64 deliverly_mode : 3;
//         u64 dest_mode : 1;
//         u64 deliverly_status : 1;
//         u64 pin_polarity : 1;
//         u64 remote_irr : 1;
//         u64 trigger_mode : 1;
//         u64 mask : 1;
//         u64 : 39;
//         u64 destination : 8;
//     } fields;
//     struct {
//         u32 lower;
//         u32 upper;
//     } regs;
// };

// struct ioapic {
//     u32 ioregsel;
//     u32 iowin;
//     u32 id;
//     u32 vec;
//     union redirtb_entry redirtb[24];
// };

int outfd = 0;

struct vm *vm;
struct vcpu *vcpu;
struct lapic *lapic;
struct blk *blk;
struct uart *uart;
int irr_count = 0;

// void enq_irr(struct irr_queue *irr, int value) {
//     irr->buff[irr->last] = value;
//     irr->last++;
//     irr_count++;
// }

// int deq_irr(struct irr_queue *irr) {
//    int out = irr->buff[0];
//    for (int i = 0; i <= irr->last; i++) {
//        irr->buff[i] = irr->buff[i+1];
//    }
//    irr->last--;

//     if (irr_count > 0) {
//         irr_count--;
//     }
// }   

// void error(char *message) {
//     perror(message);
//     exit(1);
// }

// void init_vcpu(struct vm *vm, struct vcpu *vcpu) {
//     int mmap_size;

//     vcpu->fd = ioctl(vm->fd, KVM_CREATE_VCPU, 0);
    
//     if (vcpu->fd < 0) {
//         error("KVM_CREATE_VCPU");
//     }

//     mmap_size = ioctl(vm->vm_fd, KVM_GET_VCPU_MMAP_SIZE, 0);

//     if (mmap_size <= 0) {
//         error("KVM_GET_VCPU_MMAP_SIZE");
//     }

//     vcpu->kvm_run = mmap(NULL, mmap_size, 
//                          PROT_READ | PROT_WRITE,
//                          MAP_SHARED, vcpu->fd, 0);

//     if (vcpu->kvm_run == MAP_FAILED) {
//         error("kvm_run: failed\n");
//     }
// }

// void set_sregs(struct kvm_sregs *sregs) {
//     sregs->cs.selector = 0;
// 	sregs->cs.base = 0;
// }

// void set_regs(struct vcpu *vcpu) {
//     if (ioctl(vcpu->fd, KVM_GET_SREGS, &(vcpu->sregs)) < 0) {
//         error("KVM_GET_SREGS");
//     }

//     set_sregs(&vcpu->sregs);

//     if (ioctl(vcpu->fd, KVM_SET_SREGS, &(vcpu->sregs)) < 0) {
//         error("KVM_SET_SREGS");
//     }

//     vcpu->regs.rflags = DEFAULT_FLAGS;
//     vcpu->regs.rip = START_ADDRESS;

//     if (ioctl(vcpu->fd, KVM_SET_REGS, &(vcpu->regs)) < 0) {
//         error("KVM_SET_REGS");
//     }
// }

// void load_guest_binary(void *dst) {
//     int fd = open("../xv6/bootblock", O_RDONLY);
//     if (fd < 0) 
//         error("open fail");
    
//     void *tmp = dst;
//     for(;;) {
//         int size = read(fd, tmp + START_ADDRESS, 
//                         GUEST_BINARY_SIZE);
//         if (size <= 0) 
//             break;
//         tmp += size;
//     }
// }


void print_regs(struct vcpu *vcpu) {
    ioctl(vcpu->fd, KVM_GET_REGS, &(vcpu->regs));
    printf("rip: 0x%llx\n", vcpu->regs.rip);
}

void init_kvm(struct vm *vm) {
    vm->vm_fd = open("/dev/kvm", O_RDWR);
    if (vm->vm_fd < 0) { 
        error("open /dev/kvm");
    }
}

void create_uart(struct uart *uart) {
    uart->data_reg = 0;
    uart->irr_enable_reg = 0;
    uart->irr_id_reg = 0;
    uart->line_control_reg = 0;
    uart->modem_control_reg = 0;
    uart->line_status_reg = 0;
    uart->modem_status_reg = 0;
    uart->scratch_reg = 0;
}

// void set_tss(int fd) {
//     if (ioctl(fd, KVM_SET_TSS_ADDR, 0xfffbd000) < 0) {
//         error("KVM_SET_TSS_ADDR");
//     }
// }

void create_output_file() {
    outfd = open("out.txt", O_RDWR | O_CREAT);
}

// void emulate_diskr(struct vcpu *vcpu, struct blk *blk) {
//     u32 data = 0;
//     for (int i = 0; i < vcpu->kvm_run->io.count; ++i) {
//         for (int j = 0; j < 4; j++) {
//             data |= blk->data[blk->index] << (8 * j);
//             blk->index += 1;
//         }
//         *(u32*)((unsigned char*)vcpu->kvm_run + vcpu->kvm_run->io.data_offset) = data;
//         vcpu->kvm_run->io.data_offset += vcpu->kvm_run->io.size;
//         data = 0;
//     }
// }

// void update_blk_index(struct blk *blk) {
//     u32 index = 0 | blk->lba_low_reg | (blk->lba_middle_reg << 8) | (blk->lba_high_reg << 16);
//     blk->index = index * 512;
//     if (blk->drive_head_reg == 0xf0) {
//         blk->index += IMGE_SIZE;
//     }
// }

// void emulate_disk_portw(struct vcpu *vcpu, 
//                         struct blk *blk, struct io io) {
//     u8 val1;
//     u16 val2;

//     val1 = *(u8*)((u8*)vcpu->kvm_run + io.data_offset);
    
//     switch (io.port) {
//     case 0x1F0:
//         emulate_diskw(vcpu, blk, io);
//         break;
//     case 0x1F2:
//         blk->sec_count_reg = val1;
//         break;
//     case 0x1F3:
//         blk->lba_low_reg = val1;
//         break;            
//     case 0x1F4:
//         blk->lba_middle_reg = val1;
//         break;
//     case 0x1F5:
//         blk->lba_high_reg = val1;
//     case 0x1F6:
//         blk->drive_head_reg = val1;
//         break;
//     case 0x3F6:
//         blk->dev_conotrl_regs = val1;
//     default:
//         break;
//     }

//     update_blk_index(blk);

//     if ((io.port == 0x1F0 || io.port == 0x1F7) && blk->dev_conotrl_regs == 0) {
//         enq_irr(lapic->irr,IRQ_BASE+14);
//         vcpu->kvm_run->request_interrupt_window = 1;
//     }
// }

// void emulate_disk_portr(struct vcpu *vcpu,
//                         struct blk *blk) {
//     u32 data = 0;

//     switch (vcpu->kvm_run->io.port) {
//     case 0x1F0:
//         emulate_diskr(vcpu, blk);
//         break;
//     case 0x1F7:
//          *(unsigned char*)((unsigned char*)vcpu->kvm_run + vcpu->kvm_run->io.data_offset) = blk->status_command_reg; 
//         break;
//     default:
//         break;
//     }
// }

int write_f = 0;

// void emulate_uart_portw(struct vcpu *vcpu, struct io io, struct uart *uart) {
//     switch (io.port) {
//     case 0x3f8:
//         for (int i = 0; i < io.count; i++) {
//             char *v = (char*)((unsigned char*)vcpu->kvm_run + io.data_offset);
//             printf("%c", *v);
//             write(outfd, v, 1);
//             uart->data_reg = *v;
//             vcpu->kvm_run->io.data_offset += io.size;
//         }
//         break;
//     case 0x3f9:
//         uart->irr_enable_reg = *(u8*)((u8*)vcpu->kvm_run
//          + vcpu->kvm_run->io.data_offset);
//     case 0x3fd:
//         uart->line_status_reg = *(u8*)((u8*)vcpu->kvm_run
//          + vcpu->kvm_run->io.data_offset);
//         break;
//     default:
//         break;
//     }
// }

// void emulate_uart_portr(struct vcpu *vcpu, struct io io, struct uart *uart) {
//     switch (io.port)
//     {
//     case 0x3f8:
//         *(unsigned char*)((unsigned char*)vcpu->kvm_run
//          + vcpu->kvm_run->io.data_offset) = uart->data_reg; 
//         break;
//     case 0x3fd:
//         *(unsigned char*)((unsigned char*)vcpu->kvm_run
//          + vcpu->kvm_run->io.data_offset) = uart->line_status_reg; 
//         break;
//     default:
//         break;
//     }
// }

int ck = -1;
char vk = 0;

void emulate_kbd_portr(struct io io) {
    switch (io.port)
    {
    case 0x64:
        *(unsigned char*)((unsigned char*)vcpu->kvm_run
         + vcpu->kvm_run->io.data_offset) = 1;
        break;
    case 0x60:
        *(unsigned char*)((unsigned char*)vcpu->kvm_run
         + vcpu->kvm_run->io.data_offset) = vk;
        break;
    default:
        break;
    }
}

void emulate_io_out(struct vcpu *vcpu, struct blk *blk, 
                    struct io io, struct uart *uart) {
    switch (io.port) {
    case 0x1F0 ... 0x1F7:
        emulate_disk_portw(vcpu, blk, io);
        break;
    case 0x3F6:
        emulate_disk_portw(vcpu, blk, io);
        break;
    case 0x3F8 ... 0x3FD:
        emulate_uart_portw(vcpu, io, uart);
        break;
    default:
        break;
    }
}

void emulate_io_in(struct vcpu *vcpu, struct blk *blk, 
                   struct io io, struct uart *uart) {
    switch (io.port) {
    case 0x1F0 ... 0x1F7:
        emulate_disk_portr(vcpu, blk);
        break;
    case 0x3f8 ... 0x3fd:
        emulate_uart_portr(vcpu, io, uart);
        break;
    case 0x60 ... 0x64:
        emulate_kbd_portr(io);
        break;
    default:
        break;
    }
}

void emulate_io(struct vcpu *vcpu, struct blk *blk, struct uart *uart) {
    struct io io = {
        .direction = vcpu->kvm_run->io.direction,
        .size = vcpu->kvm_run->io.size,
        .port = vcpu->kvm_run->io.port,
        .count = vcpu->kvm_run->io.count,
        .data_offset = vcpu->kvm_run->io.data_offset
    };

    switch (io.direction) {
    case KVM_EXIT_IO_OUT:
        emulate_io_out(vcpu, blk, io, uart);
        break;
    case KVM_EXIT_IO_IN:
        emulate_io_in(vcpu, blk, io, uart);
        break;
    default:
        printf("exit reason: %d\n", vcpu->kvm_run->exit_reason);
        print_regs(vcpu);
        break;
    }
}

// void emulate_lapicw(struct vcpu *vcpu, struct lapic *lapic) {
//     int index = vcpu->kvm_run->mmio.phys_addr - LAPIC_BASE;
//     u32 data = 0;

//     for (int i = 0; i < 4; i++) {
//         data |= vcpu->kvm_run->mmio.data[i] << i*8;
//     }

//     if (vcpu->kvm_run->mmio.is_write)
//         lapic->regs[index/4] = data;
// }

void emulate_mmio(struct vcpu *vcpu, struct vm *vm,
                  struct lapic *lapic,
                  struct ioapic *ioapic) {
    struct mmio mmio = {
        .data = vcpu->kvm_run->mmio.data,
        .is_write = vcpu->kvm_run->mmio.is_write,
        .len = vcpu->kvm_run->mmio.len,
        .phys_addr = vcpu->kvm_run->mmio.phys_addr
    };

    if (mmio.phys_addr >= LAPIC_BASE)
        emulate_lapicw(vcpu, lapic);
    if (mmio.phys_addr >= IOAPIC_BASE) {
        emulate_ioapicw(vcpu, ioapic);
    }
}

int flag = 0;

void *observe_input(void *in) {
    for (;;) {
        int size = read(STDIN_FILENO, in, 1);
        uart->data_reg = *(char*)in;
        enq_irr(lapic->irr, IRQ_BASE+4);
        vcpu->kvm_run->request_interrupt_window = 1;
        flag = 1;
    }
}

int main(int argc, char **argv) {
    vm = malloc(sizeof(struct vm));
    vcpu = malloc(sizeof(struct vcpu));
    lapic = malloc(sizeof(struct lapic));
    lapic->irr = malloc(4096);
    blk = malloc(sizeof(struct blk));
    uart = malloc(sizeof(struct uart));

    kvm_mem *memreg = malloc(sizeof(kvm_mem));
    struct kvm_vcpu_events *events = malloc(sizeof(struct kvm_vcpu_events));
    struct kvm_pit_state2 *pit = malloc(sizeof(struct kvm_pit_state2));
    struct kvm_msrs *msrs = malloc(sizeof(struct kvm_msrs));
    struct ioapic *ioapic = malloc(sizeof(struct ioapic));
    

    init_kvm(vm);
    create_vm(vm);
    create_blk(blk);
    set_tss(vm->fd);
    set_vm_mem(vm, memreg, 0, GUEST_MEMORY_SIZE);    
    load_guest_binary(vm->mem);
    init_vcpu(vm, vcpu);
    set_regs(vcpu);
    create_uart(uart);
    create_output_file();

    pthread_t thread;
    char input[100];

    if (pthread_create(&thread, NULL, observe_input, input) != 0) {
        error("pthread_create");
    }

    for (;;) {
        if (ioctl(vcpu->fd, KVM_RUN, 0) < 0) {
            print_regs(vcpu);
            error("KVM_RUN");
        }

        struct kvm_run *run = vcpu->kvm_run;

        switch (run->exit_reason) {
        case KVM_EXIT_IO:
            emulate_io(vcpu, blk, uart);
            break;
        case KVM_EXIT_MMIO:
            emulate_mmio(vcpu, vm, lapic, ioapic);
            break;
        case KVM_EXIT_IRQ_WINDOW_OPEN:
            if (lapic->irr->buff[0] >= 32) {
                inject_interrupt(vcpu->fd, lapic->irr->buff[0]);
                deq_irr(lapic->irr);
                vcpu->kvm_run->request_interrupt_window = 0;
            }
            break;
        default:
            printf("exit reason: %d\n", vcpu->kvm_run->exit_reason);
            break;
        }
    }
    return 1;   
}