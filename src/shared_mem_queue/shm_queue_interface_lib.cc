
#include "shared_mem_queue/shm_queue_interface_lib.h"
#include "shared_mem_queue/shmmap.h"

#define COP_QUEUE_SIZE 256
#define CMD_QUEUE_SIZE 256


//Global Queue Types
typedef SPSCQueue<compressed_op, COP_QUEUE_SIZE> cop_queue;
typedef SPSCQueue<Scarab_To_Pin_Msg, CMD_QUEUE_SIZE> cmd_queue;


void pin_shm_interface::init(int cop_queue_shm_key, int cmd_queue_shm_key) {
    cop_queue_ptr = shm_map<cop_queue>(cop_queue_shm_key, &cop_queue_shm_id);
    cmd_queue_ptr = shm_map<cmd_queue>(cmd_queue_shm_key, &cmd_queue_shm_id); 
}

void pin_shm_interface::disconnect() {
    shm_del(cop_queue_shm_id);
    shm_del(cmd_queue_shm_id);
}

void pin_shm_interface::send_cop(compressed_op cop) {
    compressed_op * shm_ptr;
    
    while((shm_ptr = cop_queue_ptr->alloc()) == nullptr);
    *shm_ptr = cop;   
    cop_queue_ptr->push();
}

Scarab_To_Pin_Msg pin_shm_interface::receive_cmd() {
    Scarab_To_Pin_Msg * shm_ptr;
    Scarab_To_Pin_Msg cmd;
    
    while((shm_ptr = cmd_queue_ptr->front()) == nullptr);
    cmd = *shm_ptr;
    cmd_queue_ptr->pop();    
    return cmd;
}

void pin_shm_interface::clear_cmd_queue() {
    Scarab_To_Pin_Msg * shm_ptr;
    while(true) {
        if((shm_ptr = cmd_queue_ptr->front()) == nullptr) break;
        cmd_queue_ptr->pop();
    }
}

void scarab_shm_interface::init(int cop_queue_shm_key, int cmd_queue_shm_key) {
    cop_queue_ptr = shm_map<cop_queue>(cop_queue_shm_key, &cop_queue_shm_id);
    cmd_queue_ptr = shm_map<cmd_queue>(cmd_queue_shm_key, &cmd_queue_shm_id); 
}

void scarab_shm_interface::disconnect() {
    shm_del(cop_queue_shm_id);
    shm_del(cmd_queue_shm_id);
}

compressed_op scarab_shm_interface::receive_cop() {
    compressed_op * shm_ptr;
    compressed_op cop;
    
    while((shm_ptr = cop_queue_ptr->front()) == nullptr);
    cop = *shm_ptr;
    cop_queue_ptr->pop();    
    return cop;
}

void scarab_shm_interface::send_cmd(Scarab_To_Pin_Msg cmd) {
    Scarab_To_Pin_Msg * shm_ptr;
    
    while((shm_ptr = cmd_queue_ptr->alloc()) == nullptr);
    *shm_ptr = cmd;   
    cmd_queue_ptr->push();
}

void scarab_shm_interface::clear_cop_queue() {
    compressed_op * shm_ptr;
    while(true)
    {
        if((shm_ptr = cop_queue_ptr->front()) == nullptr) break;
        cop_queue_ptr->pop();
    }
}
