/* Copyright 2020 HPS/SAFARI Research Groups
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/***************************************************************************************
 * File         : optimizer2.c
 * Author       : HPS Research Group
 * Date         : 2/10/2009
 * Description  : Optimizer2 support (choosing best performing params at
 *run-time for limit studies).
 *
 *                The optimizer2 spawns off N slaves (one for each possible
 *configuration) and retains the original process as the master. N fifo pipes
 *from the master feed comparison barrier decisions to the
 *slaves. A single feedback fifo pipe feeds performance data
 *from the slaves to the master.
 ***************************************************************************************/

#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "debug/debug.param.h"
#include "debug/debug_macros.h"
#include "general.param.h"
#include "globals/assert.h"
#include "globals/global_defs.h"
#include "globals/global_types.h"
#include "globals/utils.h"
#include "optimizer2.h"
#include "statistics.h"

#define DEBUG(proc_id, args...) _DEBUGU(proc_id, DEBUG_OPTIMIZER2, ##args)

#define MESSAGE_TYPE_LIST                       \
  MESSAGE_TYPE_LIST_ITEM(OPT_NEW_SLAVE_REQ)     \
  MESSAGE_TYPE_LIST_ITEM(OPT_NEW_SLAVE_ACK)     \
  MESSAGE_TYPE_LIST_ITEM(OPT_REPORT_METRIC)     \
  MESSAGE_TYPE_LIST_ITEM(OPT_REPORT_METRIC_ACK) \
  MESSAGE_TYPE_LIST_ITEM(OPT_DIE)               \
  MESSAGE_TYPE_LIST_ITEM(OPT_DIE_ACK)           \
  MESSAGE_TYPE_LIST_ITEM(OPT_SIM_COMPLETE)      \
  MESSAGE_TYPE_LIST_ITEM(OPT_ANY_TYPE)          \
  MESSAGE_TYPE_LIST_ITEM(OPT_NUM_MESSAGE_TYPES) \
  /* end of list */

#define MESSAGE_TYPE_LIST_ITEM(x) x,

typedef enum { MESSAGE_TYPE_LIST } Message_Type;

#undef MESSAGE_TYPE_LIST_ITEM
#define MESSAGE_TYPE_LIST_ITEM(x) #x,

static const char* const message_type_names[] = {MESSAGE_TYPE_LIST};

#undef MESSAGE_TYPE_LIST

typedef struct Message_struct {
  pid_t        sender_pid;
  Message_Type type;
  uns          config;
  Counter      data;
} Message;

typedef struct Slave_Result_struct {
  int    pid;
  uns    config;
  double metric;
} Slave_Result;

static Flag in_use    = FALSE;
static Flag is_leader = TRUE;  // if we have not initialized, there's only one
                               // process which is the leader
static uns   num_configs;
static uns   master_pid;
static uns   my_config_num;
static FILE* read_stream;
static FILE* feedback_read_stream;
static FILE* feedback_write_stream;
void (*setup_param_fn)(int) = NULL;

static void    init_slave(void);
static void    send_msg(FILE* stream, Message_Type type, Counter data);
static void    receive_msg(FILE* stream, Message_Type type, Message* msg,
                           uns sender_pid);
static Counter dbl2ctr(double x);
static double  ctr2dbl(Counter x);
static void    spawn_children(void);
static FILE*   open_fifo(uns pid, const char* mode);
static void    slave_clean_up(void);
static void    master_clean_up(void);
static void    run_master(void);
static void    decouple_open_files(void);

void init_slave(void) {
  char buf[MAX_STR_LENGTH + 1];
  in_use = TRUE;
  decouple_open_files();
  sprintf(buf, "/tmp/scarab_opt2_feedback_fifo_%d", master_pid);
  feedback_write_stream = fopen(buf, "w");
  if(!feedback_write_stream)
    FATAL_ERROR(
      0, "Slave %d (config %d) feedback write stream fopen FAILED. errno: %s\n",
      getpid(), my_config_num, strerror(errno));
  sprintf(buf, "/tmp/scarab_opt2_fifo_%d", getpid());
  if(mkfifo(buf, S_IWUSR | S_IRUSR))
    FATAL_ERROR(0, "Creation of pipe %s FAILED\n", buf);
  read_stream = fopen(buf, "r+");
  if(!read_stream)
    FATAL_ERROR(0, "%s fopen FAILED. errno: %s\n", buf, strerror(errno));
  DEBUG(0, "Slave %d (config %d) inited\n", getpid(), my_config_num);
}

FILE* open_fifo(uns pid, const char* mode) {
  char buf[MAX_STR_LENGTH + 1];
  sprintf(buf, "/tmp/scarab_opt2_fifo_%d", pid);
  FILE* stream = fopen(buf, mode);
  if(!stream)
    FATAL_ERROR(0, "%s fopen FAILED. errno: %s\n", buf, strerror(errno));
  return stream;
}

void run_master(void) {
  Message      msg;
  Flag         new_slave_req_outstanding = FALSE;
  uns          num_slaves                = 1;
  uns          num_slaves_to_report      = num_slaves;
  uns          num_slaves_reported       = 0;
  Slave_Result best_result               = {0};
  Slave_Result survivor_result           = {-1, 0, 0};
  uns          prev_best_config_num      = 0;
  FILE*        master_trace              = fopen("master.trace", "w");
  ASSERTM(0, master_trace, "Could not open master trace\n");
  while(1) {
    DEBUG(0,
          "Master "
          "state:\n\tnum_slaves\t\t%d\n\tnum_slaves_to_report\t%d\n\tnum_"
          "slaves_reported\t%d\n",
          num_slaves, num_slaves_to_report, num_slaves_reported);
    receive_msg(feedback_read_stream, OPT_ANY_TYPE, &msg, 0);
    switch(msg.type) {
      case OPT_NEW_SLAVE_REQ:
        ASSERT(0, !new_slave_req_outstanding);
        if(num_slaves < OPTIMIZER2_MAX_NUM_SLAVES) {
          FILE* slave_fifo = open_fifo(msg.sender_pid, "w");
          send_msg(slave_fifo, OPT_NEW_SLAVE_ACK, 0);
          fclose(slave_fifo);
          num_slaves++;
          num_slaves_to_report++;
        } else {
          new_slave_req_outstanding = TRUE;
        }
        break;
      case OPT_DIE_ACK:
        // waitpid() here?
        if(new_slave_req_outstanding) {
          uns   parent_pid  = msg.data;
          FILE* parent_fifo = open_fifo(parent_pid, "w");
          send_msg(parent_fifo, OPT_NEW_SLAVE_ACK, 0);
          fclose(parent_fifo);
          new_slave_req_outstanding = FALSE;
          num_slaves_to_report++;
        } else {
          num_slaves--;
        }
        break;
      case OPT_REPORT_METRIC: {
        ASSERT(0, num_slaves_reported < num_slaves_to_report);
        Slave_Result result;
        result.pid    = msg.sender_pid;
        result.metric = ctr2dbl(msg.data);
        result.config = msg.config;
        Flag new_best = num_slaves_reported == 0 ||
                        result.metric <
                          best_result.metric;  // potentially non-deterministic
        Flag kill_process;
        uns  kill_pid;
        if(OPTIMIZER2_PERFECT_MEMORYLESS) {
          kill_process = (result.config != prev_best_config_num);
          kill_pid     = result.pid;
        } else {
          kill_process = num_slaves_reported > 0;
          // we want to kill previous best pid, if any
          kill_pid = new_best ? best_result.pid : result.pid;
        }
        if(new_best) {
          best_result = result;
        }
        if(kill_process) {
          FILE* slave_fifo = open_fifo(kill_pid, "w");
          send_msg(slave_fifo, OPT_DIE, kill_pid);
          fclose(slave_fifo);
          ASSERT(0, survivor_result.pid != result.pid);
          if(kill_pid == survivor_result.pid) {
            survivor_result = result;
          } else {
            ASSERT(0, kill_pid == result.pid);
          }
        } else {
          ASSERT(0, survivor_result.pid == -1);
          survivor_result = result;
        }
        ++num_slaves_reported;
      } break;
      case OPT_SIM_COMPLETE: {
        ASSERT(0, num_slaves == 1);
        // waitpid() here?
        DEBUG(0, "Master finished\n");
        fclose(master_trace);
        master_clean_up();
        exit(EXIT_SUCCESS);
      } break;
      default:
        FATAL_ERROR(0, "Unhandled message type %s\n",
                    message_type_names[msg.type]);
        break;
    }
    if(num_slaves_reported == num_slaves_to_report && num_slaves == 1) {
      DEBUG(0,
            "All slaves reported, best slave: %d (config %d), survivor slave: "
            "%d (config %d)\n",
            best_result.pid, best_result.config, survivor_result.pid,
            survivor_result.config);
      fprintf(master_trace, "%d\n", survivor_result.config);
      fflush(master_trace);
      ASSERT(0, survivor_result.pid != -1);  // we must have a survivor
      ASSERT(0, (best_result.pid == survivor_result.pid) ||
                  OPTIMIZER2_PERFECT_MEMORYLESS);
      FILE* slave_fifo = open_fifo(survivor_result.pid, "w");
      send_msg(slave_fifo, OPT_REPORT_METRIC_ACK, 0);
      fclose(slave_fifo);
      num_slaves_reported  = 0;
      num_slaves_to_report = num_slaves;
      prev_best_config_num = best_result.config;
      survivor_result.pid  = -1;
    }
  }
}

void opt2_init(uns n, uns n_to_keep, void (*fn)(int)) {
  ASSERT(0, sizeof(Counter) == sizeof(double));
  ASSERTM(0, !INST_LIMIT || NUM_CORES == 1 || !DUMP_STATS,
          "Optimizer2 does not work with with multiple cores that dump stats "
          "at different times.\n");
  num_configs    = n;
  setup_param_fn = fn;
  char buf[MAX_STR_LENGTH + 1];
  DEBUG(0, "Initializing optimizer2\n");
  signal(SIGCHLD, SIG_IGN); /* avoid zombie processes */
  master_pid = getpid();
  sprintf(buf, "/tmp/scarab_opt2_feedback_fifo_%d", master_pid);
  if(mkfifo(buf, S_IWUSR | S_IRUSR))
    FATAL_ERROR(0, "Creation of pipe %s FAILED\n", buf);
  feedback_read_stream = fopen(buf, "r+");
  if(!feedback_read_stream)
    FATAL_ERROR(0, "Master feedback read stream fopen FAILED. errno: %s\n",
                strerror(errno));
  fflush(stdout); /* avoid repeated messages */
  if(!fork()) {
    init_slave();
    is_leader = TRUE;
    /*
    spawn_children();
    */
    return;
  }
  run_master();
  return;
}

void slave_clean_up(void) {
  char buf[MAX_STR_LENGTH + 1];
  fclose(read_stream);
  sprintf(buf, "/tmp/scarab_opt2_fifo_%d", getpid());
  unlink(buf);
  fclose(feedback_write_stream);
}

void master_clean_up(void) {
  char buf[MAX_STR_LENGTH + 1];
  fclose(feedback_read_stream);
  sprintf(buf, "/tmp/scarab_opt2_feedback_fifo_%d", getpid());
  unlink(buf);
}

void opt2_comparison_barrier(double metric) {
  send_msg(feedback_write_stream, OPT_REPORT_METRIC, dbl2ctr(metric));
  Message msg;
  receive_msg(read_stream, OPT_ANY_TYPE, &msg, master_pid);
  switch(msg.type) {
    case OPT_DIE:
      send_msg(feedback_write_stream, OPT_DIE_ACK, getppid());
      slave_clean_up();
      exit(EXIT_SUCCESS);
      break;
    case OPT_REPORT_METRIC_ACK:
      is_leader = TRUE;
      break;
    default:
      FATAL_ERROR(0, "Unexpected message %s received!\n",
                  message_type_names[msg.type]);
      break;
  }
}

void send_msg(FILE* stream, Message_Type type, Counter data) {
  DEBUG(0, "Process %d sending msg %s\n", getpid(), message_type_names[type]);
  ASSERT(0, type != OPT_ANY_TYPE);
  Message msg;
  msg.type       = type;
  msg.config     = my_config_num;
  msg.data       = data;
  msg.sender_pid = getpid();
  uns written    = fwrite(&msg, sizeof(Message), 1, stream);
  if(written != 1)
    FATAL_ERROR(0, "Send FAILED!\n");
  fflush(stream);
}

void receive_msg(FILE* stream, Message_Type type, Message* external_msg,
                 uns sender_pid) {
  Message  local_msg;
  Message* msg  = external_msg ? external_msg : &local_msg;
  uns      read = fread(msg, sizeof(Message), 1, stream);
  if(read != 1)
    FATAL_ERROR(0, "Receive FAILED!\n");
  DEBUG(0, "Process %d received msg %s from pid %d\n", getpid(),
        message_type_names[msg->type], msg->sender_pid);
  ASSERT(0, msg->type != OPT_ANY_TYPE);
  if((type != OPT_ANY_TYPE) && (type != msg->type))
    FATAL_ERROR(0, "Unexpected message type %s received (expected %s)!\n",
                message_type_names[msg->type], message_type_names[type]);
  if(sender_pid && (sender_pid != msg->sender_pid))
    FATAL_ERROR(0,
                "Message received from unexpected sender %d (expected %d)!\n",
                msg->sender_pid, sender_pid);
  return;
}

void spawn_children() {
  fflush(stdout);    /* prevent duplicate output */
  is_leader = FALSE; /* spawned children should know they are not leaders */
  uns config_num = 0;
  while(config_num < num_configs) {
    if(config_num != my_config_num) { /* don't init myself */
      send_msg(feedback_write_stream, OPT_NEW_SLAVE_REQ, config_num);
      receive_msg(read_stream, OPT_NEW_SLAVE_ACK, NULL, master_pid);
      if(!fork()) {
        fclose(read_stream);
        fclose(feedback_write_stream);
        setup_param_fn(config_num);
        my_config_num = config_num;
        init_slave();
        return;
      }
    }
    config_num++;
  }
  is_leader = TRUE;
}

void opt2_decision_point(void) {
  spawn_children();
}

void opt2_sim_complete(void) {
  send_msg(feedback_write_stream, OPT_SIM_COMPLETE, 0);
  slave_clean_up();
}

Flag opt2_in_use(void) {
  return in_use;
}

Flag opt2_is_leader(void) {
  return is_leader;
}

void decouple_open_files(void) {
  const uns MAX_FDS = 1024;
  uns       fds[MAX_FDS];
  uns       num_fds = 0;

  char fdinfo_path[MAX_STR_LENGTH + 1];
  uns  len = snprintf(fdinfo_path, MAX_STR_LENGTH, "/proc/%d/fdinfo", getpid());
  ASSERT(0, len < MAX_STR_LENGTH);
  DIR* dir = opendir(fdinfo_path);

  while(dir) {
    struct dirent* entry = readdir(dir);
    if(entry) {
      if(entry->d_name[0] != '.') {  // not a "." or ".."
        ASSERT(0, num_fds < MAX_FDS);
        fds[num_fds] = atoi(entry->d_name);
        ++num_fds;
      }
    } else {
      break;
    }
  }

  // by closing the directory, we invalidate the FDs opened by opendir/readdir
  closedir(dir);

  for(int i = 0; i < num_fds; ++i) {
    if(fcntl(fds[i], F_GETFL, 0) !=
       -1) {  // valid FD (not related to /proc/pid/fdinfo traversal)
      int fd = fds[i];
      if(fd <= 2)
        continue;  // do not decouple standard input/output/error
      char fd_path[MAX_STR_LENGTH + 1];
      uns  len = snprintf(fd_path, MAX_STR_LENGTH, "/proc/%d/fd/%d", getpid(),
                         fd);
      ASSERT(0, len < MAX_STR_LENGTH);
      char path[MAX_STR_LENGTH + 1];
      uns  path_len = readlink(fd_path, path, MAX_STR_LENGTH);
      ASSERT(0, path_len < MAX_STR_LENGTH);
      path[path_len] = 0;  // null terminator
      int64 offset   = lseek(fd, 0, SEEK_CUR);
      int   flags    = fcntl(fd, F_GETFL, 0) & ~O_CREAT & ~O_EXCL & ~O_NOCTTY &
                  ~O_TRUNC;  // clear file creation flags for safety
      int close_rc = close(fd);
      ASSERT(0, close_rc == 0);
      int open_fd = open(path, flags);
      ASSERT(0, open_fd != -1);
      if(open_fd != fd) {
        int new_fd = dup2(open_fd, fd);
        ASSERT(0, new_fd == fd);
        int close_rc = close(open_fd);
        ASSERT(0, close_rc == 0);
      }
      int ret_offset = lseek(fd, offset, SEEK_SET);
      ASSERT(0, ret_offset == offset);
    } else {
      ASSERT(0, errno == EBADF);
      errno = 0;
    }
  }
}

Counter dbl2ctr(double x) {
  Counter result;
  memcpy(&result, &x, sizeof(Counter));
  return result;
}

double ctr2dbl(Counter x) {
  double result;
  memcpy(&result, &x, sizeof(double));
  return result;
}
