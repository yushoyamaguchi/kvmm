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
#include <time.h>
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
#include "input_filename.h"


struct vm *vm;
int outfd = 0;

char kernel_img_name[MAX_LINE_LENGTH];
char fs_img_name[MAX_LINE_LENGTH];
char bootblock_name[MAX_LINE_LENGTH];

void init_kvm(struct vm *vm) {
    vm->vm_fd = open("/dev/kvm", O_RDWR);
    if (vm->vm_fd < 0) { 
        error("open /dev/kvm");
    }
}

void create_output_file() {
    outfd = open("out.txt", O_RDWR | O_CREAT);
}

struct subthread_input {
    char *in;
    struct vcpu *vcpu;
};

int co = 0;

void *observe_input(void *in) {
    for (;;) {
        struct subthread_input *subin = (struct subthread_input*)in;
        int c = getchar();
        set_uart_buff((char)c);
        //printf("set: %d\n", co);
        enq_irr(subin->vcpu, IRQ_BASE+4);
        co++;
    }
}

extern struct vcpu *vcpu;

int main(int argc, char **argv) {
    if(argc!=2){
        printf("Usage: %s <input file name> \n", argv[0]);
        exit(1);
    }
    FILE *file = fopen(argv[1], "r");
    if (file == NULL) {
        printf("cannot open the input file\n");
        return 1;
    }
    char line[MAX_LINE_LENGTH];
    int input_line_count=0;
    while (fgets(line, MAX_LINE_LENGTH-1, file) != NULL) {
        line[strcspn(line, "\n")] = 0;
        printf("%s\n", line);
        if(input_line_count==0){
            strcpy(kernel_img_name, line);
        }
        else if(input_line_count==1){
            strcpy(fs_img_name, line);
        }
        else if(input_line_count==2){
            strcpy(bootblock_name, line);
        }
        else{
            break;
        }
        input_line_count++;
    }
    vm = malloc(sizeof(struct vm));

    kvm_mem *memreg = malloc(sizeof(kvm_mem));

    init_kvm(vm);
    create_vm(vm);
    create_blk();
    set_tss(vm->fd);
    set_vm_mem(vm, memreg, 0, GUEST_MEMORY_SIZE);    
    load_guest_binary(vm->mem);
    init_vcpu(vm);
    init_lapic();
    init_ioapic();
    set_regs();
    create_uart();
    create_output_file();

    pthread_t thread;
    struct subthread_input subin;
    char *usrinput = malloc(100);
    subin.in = usrinput;
    subin.vcpu = vcpu;

    if (pthread_create(&thread, NULL, observe_input, (void*)&subin) != 0) {
        error("pthread_create");
    }

    // Open file to write time data
    int file_fd = open("time.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (file_fd < 0) {
        error("open time.txt");
    }

    time_t last_write_time = time(NULL);
    struct timespec start_kvm, end_kvm, start_emulation, end_emulation;
    long kvm_time_ns = 0, emulation_time_ns = 0;
    long count_io_in = 0;
    long count_io_out = 0;
    long count_mmio = 0;
    long count_interrupt = 0;
    long count_disk_in = 0;
    long count_uart_in = 0;
    long count_132 = 0;

    for (;;) {
        clock_gettime(CLOCK_MONOTONIC, &start_kvm);
        if (ioctl(vcpu->fd, KVM_RUN, 0) < 0) {
            print_regs(vcpu);
            error("KVM_RUN");
        }
        clock_gettime(CLOCK_MONOTONIC, &end_kvm);

        struct kvm_run *run = vcpu->kvm_run;
        clock_gettime(CLOCK_MONOTONIC, &start_emulation);

        switch (run->exit_reason) {
        case KVM_EXIT_IO:
            if (run->io.direction == KVM_EXIT_IO_IN) {
                count_io_in++;
                switch (vcpu->kvm_run->io.port) {
                case 0x1F0 ... 0x1F7:
                    count_disk_in++;
                    break;
                case 0x3f8 ... 0x3fd:
                    count_uart_in++;
                    break;    
                default:
                    break;
                }
                if (run->io.port == 0x84) {
                    count_132++;
                }
            } else {
                count_io_out++;
            }
            emulate_io(vcpu);
            break;
        case KVM_EXIT_MMIO:
            count_mmio++;
            emulate_mmio(vcpu);
            break;
        case KVM_EXIT_IRQ_WINDOW_OPEN:
            count_interrupt++;
            emulate_interrupt(vcpu);
            break;
        default:
            printf("exit reason: %d\n", vcpu->kvm_run->exit_reason);
            break;
        }
        clock_gettime(CLOCK_MONOTONIC, &end_emulation);

        kvm_time_ns += (end_kvm.tv_sec - start_kvm.tv_sec) * 1e9 + (end_kvm.tv_nsec - start_kvm.tv_nsec);
        emulation_time_ns += (end_emulation.tv_sec - start_emulation.tv_sec) * 1e9 + (end_emulation.tv_nsec - start_emulation.tv_nsec);

        time_t current_time = time(NULL);
        if (difftime(current_time, last_write_time) >= 10) {
            char buffer[256];
            snprintf(buffer, sizeof(buffer), "KVM time: %ld ns, Emulation time: %ld ns\n", kvm_time_ns, emulation_time_ns);
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "IO in: %ld, IO out: %ld, MMIO count: %ld, Interrupt count: %ld\n", count_io_in, count_io_out, count_mmio, count_interrupt);
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "Disk in: %ld, UART in: %ld\n", count_disk_in, count_uart_in);
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "0x84 count: %ld\n", count_132);
            write(file_fd, buffer, strlen(buffer));
            last_write_time = current_time;
            kvm_time_ns = 0;
            emulation_time_ns = 0;
        }
    }
    close(file_fd);
    return 1;   
}