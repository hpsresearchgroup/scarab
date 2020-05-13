
#ifndef __SHM_QUEUE_INTERFACE_LIB_H__
#define __SHM_QUEUE_INTERFACE_LIB_H__

#include "shared_mem_queue/SPSCQueue.h"
#include "shared_mem_queue/SPSCQueueOPT.h"

#include "pin/pin_lib/pin_scarab_common_lib.h"

#define COP_QUEUE_SIZE 256
#define CMD_QUEUE_SIZE 256

//Global Queue Types
typedef SPSCQueue<compressed_op, COP_QUEUE_SIZE> cop_queue;
typedef SPSCQueue<Scarab_To_Pin_Msg, CMD_QUEUE_SIZE> cmd_queue;

class pin_shm_interface {
private:
    cop_queue * cop_queue_ptr;
    cmd_queue * cmd_queue_ptr;
    int cop_queue_shm_id;
    int cmd_queue_shm_id;

public:
    void init(int cop_queue_shm_key, int cmd_queue_shm_key);
    void disconnect();
    void send_cop(compressed_op cop);
    Scarab_To_Pin_Msg receive_cmd();
    void clear_cmd_queue();
};

class scarab_shm_interface {   
private:
    cop_queue * cop_queue_ptr;
    cmd_queue * cmd_queue_ptr;
    int cop_queue_shm_id;
    int cmd_queue_shm_id;

public:
    void init(int cop_queue_shm_key, int cmd_queue_shm_key);
    void disconnect();
    compressed_op receive_cop();
    void send_cmd(Scarab_To_Pin_Msg);    
    void clear_cop_queue();
};

#endif /* #ifndef __SHM_QUEUE_INTERFACE_H__ */