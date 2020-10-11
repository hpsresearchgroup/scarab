
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
    cop_queue* cop_queue_ptr;
    cmd_queue* cmd_queue_ptr;
    int cop_queue_shm_id;
    int cmd_queue_shm_id;

public:
    void init(int cop_queue_shm_key, int cmd_queue_shm_key, int core_id);
    void disconnect();
    void send_cop(compressed_op cop);
    Scarab_To_Pin_Msg receive_cmd();
    void clear_cmd_queue();
};

class scarab_shm_interface {   
private:
    std::vector<cop_queue*> cop_queue_ptr;
    std::vector<cmd_queue*> cmd_queue_ptr;
    std::vector<int> cop_queue_shm_id;
    std::vector<int> cmd_queue_shm_id;
    int num_cores;

public:
    void init(int cop_queue_shm_key, int cmd_queue_shm_key, int num_cores);
    void disconnect();
    compressed_op receive_cop(int core_id);
    void send_cmd(Scarab_To_Pin_Msg msg, int core_id);    
    void clear_cop_queue(int core_id);
};

#endif /* #ifndef __SHM_QUEUE_INTERFACE_H__ */