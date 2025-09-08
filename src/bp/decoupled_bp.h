#ifndef __DECOUPLED_BP_H__
#define __DECOUPLED_BP_H__


#ifdef __cplusplus
extern "C" {
#endif



typedef enum Bp_State_enum {
  BP_NORMAL,
  BP_WAIT_TIMER,
  BP_WAIT_EMPTY_ROB,
  BP_WAIT_REDIRECT
} Bp_State;

typedef enum Bp_Break_enum {
  BP_NO_BREAK,
  BP_BREAK_ON_EARLY_LATE_DISAGREE,
  BP_BREAK_ON_BTB_MISS,
  BP_BREAK_ON_N_TAKEN,
  BP_BREAK_ON_BARRIER,
  BP_BREAK_ON_NUM_OP,
  BP_BREAK_ON_MISPRED,
  BP_BREAK_ON_FULL_FETCH_QUEUE
} Bp_Break;

typedef struct Decoupled_BP_struct {
  uns8       proc_id;

  Bp_State state ; /* state that the BP is in */
  Bp_State
    next_state; /* state that the BP is going to be in next cycle */

  Counter inst_count; /* instruction counter used to number ops (global counter
                         is for retired ops) */
  Addr        curr_addr;      /* address fetched */
  Addr        next_addr; /* address to fetch */
  Flag        off_path;        /* is the icache fetching on the correct path? */
  Flag        off_path_btb_miss; /* is the icache off path from a btb miss */
  Counter     oldest_btb_miss_op_num; /* uid of the oldest btb miss*/
  Flag back_on_path; /* did a recovery happen to put the machine back on path?
                      */

  Counter timer_cycle; /* cycle that the icache stall timer will have elapsed
                          and the icache can fetch again */
  
  //data needed to maintain the fetch queue
  Counter num_branches_in_fetch_queue;
  Counter num_taken_branches_in_fetch_queue;
} Decoupled_BP;

typedef struct fetch_queue_entry {
  Flag valid;
  Op* op;
} fetch_queue_entry;

extern Decoupled_BP* dbp;

void set_dbp_stage(Decoupled_BP* new_dbp);
void reset_dbp_stage();
void init_dbp_stage(uns8 proc_id);
void update_decoupled_bp();
void redirect_decoupled_bp();
Bp_State cycle_decoupled_bp(uns proc_id);
void recover_fetch_queue();
Op * read_fetch_queue(uns proc_id);
Flag pop_fetch_queue(uns proc_id);

#ifdef __cplusplus
}
#endif


#endif  //__DECOUPLED_BP_H__
