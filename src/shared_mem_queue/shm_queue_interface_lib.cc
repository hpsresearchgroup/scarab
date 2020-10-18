
#include "shared_mem_queue/shm_queue_interface_lib.h"
#include "shared_mem_queue/shmmap.h"

ScarabOpBuffer_type get_ScarabOpBuffer_type(ScarabOpBuffer_type_fixed_alloc * src)
{
    ScarabOpBuffer_type retval;
    retval.resize(src->size);
    for(int i=0; i<src->size; i++)
    {
        retval[i] = src->cop_array[i];
    }
    return retval;
}

void pin_shm_interface::init(int cop_queue_shm_key, int cmd_queue_shm_key, int core_id) {

    //Key for core number x is base_key + x
    cop_queue_ptr = shm_map<cop_queue>(cop_queue_shm_key + core_id, &cop_queue_shm_id);
    cmd_queue_ptr = shm_map<cmd_queue>(cmd_queue_shm_key + core_id, &cmd_queue_shm_id); 

}

void pin_shm_interface::disconnect() {
    shm_del(cop_queue_shm_id);
    shm_del(cmd_queue_shm_id);
}

void pin_shm_interface::send_op_buffer(ScarabOpBuffer_type op_buffer) {
    ScarabOpBuffer_type_fixed_alloc * shm_ptr;
    
    while((shm_ptr = cop_queue_ptr->alloc()) == nullptr);
    *shm_ptr = op_buffer;   
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

void scarab_shm_interface::init(int cop_queue_shm_key, int cmd_queue_shm_key, int num_cores) {

    scarab_shm_interface::num_cores = num_cores;
    cop_queue_ptr.resize(num_cores);
    cmd_queue_ptr.resize(num_cores);
    cop_queue_shm_id.resize(num_cores);
    cmd_queue_shm_id.resize(num_cores);
    
    for(int i=0; i<num_cores; i++)
    {
        cop_queue_ptr[i] = shm_map<cop_queue>(cop_queue_shm_key, &cop_queue_shm_id[i]);
        cmd_queue_ptr[i] = shm_map<cmd_queue>(cmd_queue_shm_key, &cmd_queue_shm_id[i]); 
    }
}

void scarab_shm_interface::disconnect() {
    for(int i=0; i<num_cores; i++)
    {
        shm_del(cop_queue_shm_id[i]);
        shm_del(cmd_queue_shm_id[i]);
    }
}

ScarabOpBuffer_type scarab_shm_interface::receive_op_buffer(int core_id) {
    
    ScarabOpBuffer_type_fixed_alloc * shm_ptr;
    ScarabOpBuffer_type op_buffer;
    
    while((shm_ptr = cop_queue_ptr[core_id]->front()) == nullptr);
    op_buffer = get_ScarabOpBuffer_type(shm_ptr);
    cop_queue_ptr[core_id]->pop();    
    return op_buffer;
}

void scarab_shm_interface::send_cmd(Scarab_To_Pin_Msg cmd, int core_id) {
    Scarab_To_Pin_Msg * shm_ptr;
    
    while((shm_ptr = cmd_queue_ptr[core_id]->alloc()) == nullptr);
    *shm_ptr = cmd;   
    cmd_queue_ptr[core_id]->push();
}

void scarab_shm_interface::clear_cop_queue(int core_id) {
    ScarabOpBuffer_type_fixed_alloc * shm_ptr;
    while(true)
    {
        if((shm_ptr = cop_queue_ptr[core_id]->front()) == nullptr) break;
        cop_queue_ptr[core_id]->pop();
    }
}
