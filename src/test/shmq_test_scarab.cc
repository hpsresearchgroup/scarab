
#include "shared_mem_queue/shm_queue_interface_lib.h"
#include "pin/pin_lib/pin_scarab_common_lib.h"

int main() {

    scarab_shm_interface * pin;
    pin = new scarab_shm_interface;

    pin->init(1234, 5678, 1);

    int count = 10;
    while(1) {
        Scarab_To_Pin_Msg msg;
        msg.inst_uid = 10-count;
        pin->send_cmd(msg, 0);

        ScarabOpBuffer_type buf;
        buf = pin->receive_op_buffer(0);
        printf("Op Buf received: ");
        for(int i=0; i<buf.size(); i++)
        {
          printf("%lx, ", buf[i].instruction_addr);
        }
        printf("\n");
        fflush(stdout);

        count--;
        if(count<1) break;
    }

    pin->disconnect();
    printf("Scarab terminated\n");   

    return 0;
}


