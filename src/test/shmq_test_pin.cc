
#include "shared_mem_queue/shm_queue_interface_lib.h"
#include "pin/pin_lib/pin_scarab_common_lib.h"

int main() {

    pin_shm_interface * scarab;
    scarab = new pin_shm_interface;

    scarab->init(1234, 5678, 0);

    int count = 0;
    while(1) {
        Scarab_To_Pin_Msg cmd;
        cmd = scarab->receive_cmd();
        printf("Received cmd inst uid = %lu\n", cmd.inst_uid);
        
        compressed_op cop;
        cop.instruction_addr = 0x3000 + 2*count;
        scarab->send_cop(cop);
        cop.instruction_addr = 0x3000 + 2*count+1;
        scarab->send_cop(cop);

        count++;
        if(count>9) break;
    }
    
    scarab->disconnect();
    printf("PIN terminated\n");

    return 0;
}

