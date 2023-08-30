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

    for (;;) {
        if (ioctl(vcpu->fd, KVM_RUN, 0) < 0) {
            print_regs(vcpu);
            error("KVM_RUN");
        }

        struct kvm_run *run = vcpu->kvm_run;

        switch (run->exit_reason) {
        case KVM_EXIT_IO:
            emulate_io(vcpu);
            break;
        case KVM_EXIT_MMIO:
            emulate_mmio(vcpu);
            break;
        case KVM_EXIT_IRQ_WINDOW_OPEN:
            emulate_interrupt(vcpu);
            break;
        default:
            printf("exit reason: %d\n", vcpu->kvm_run->exit_reason);
            break;
        }
    }
    return 1;   
}