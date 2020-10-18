
#ifndef __SHM_QUEUE_INTERFACE_LIB_H__
#define __SHM_QUEUE_INTERFACE_LIB_H__

#include "shared_mem_queue/SPSCQueue.h"
#include "shared_mem_queue/SPSCQueueOPT.h"

#include "pin/pin_lib/pin_scarab_common_lib.h"
#include "pin/pin_lib/message_queue_interface_lib.h"

#define COP_QUEUE_SIZE 256
#define CMD_QUEUE_SIZE 256

#define MAX_SCARAB_BUFFER_OPS 8

struct ScarabOpBuffer_type_fixed_alloc
{
    int size;
    compressed_op cop_array[MAX_SCARAB_BUFFER_OPS];
    
    ScarabOpBuffer_type_fixed_alloc & operator=(const ScarabOpBuffer_type src)
    {
        size = src.size();
        for(int i=0; i<size; i++)
        {
            cop_array[i] = src[i];
        }
        return *this;
    }

};

ScarabOpBuffer_type get_ScarabOpBuffer_type(ScarabOpBuffer_type_fixed_alloc * src);
    

//Global Queue Types
typedef SPSCQueue<ScarabOpBuffer_type_fixed_alloc, COP_QUEUE_SIZE> cop_queue;
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
    void send_op_buffer(ScarabOpBuffer_type op_buffer);
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
    ScarabOpBuffer_type receive_op_buffer(int core_id);
    void send_cmd(Scarab_To_Pin_Msg msg, int core_id);    
    void clear_cop_queue(int core_id);
};

#endif /* #ifndef __SHM_QUEUE_INTERFACE_H__ */