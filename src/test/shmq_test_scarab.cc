
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

        for(int i=0; i<2; i++)
        {
          ScarabOpBuffer_type buf;
          printf("Before Op Buf received: ");
          fflush(stdout);
          buf = pin->receive_op_buffer(0);
          printf("Op Buf received: ");
          fflush(stdout);
          for(int i=0; i<buf.size(); i++)
          {
            printf("%lx, ", buf[i].instruction_addr);
            printf("\n");
          }
        }

        count--;
        if(count<1) break;
    }

    pin->disconnect();
    printf("Scarab terminated\n");   

    return 0;
}


