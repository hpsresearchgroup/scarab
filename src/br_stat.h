#include "globals/assert.h"
#include "globals/global_defs.h"
#include "globals/global_types.h"
#include "globals/global_vars.h"
#include "globals/utils.h"

#include "bp/bp.h"
#include "exec_ports.h"
#include "frontend/frontend.h"
#include "memory/memory.h"
#include "node_stage.h"
#include "thread.h"

#include "bp/bp.param.h"
#include "core.param.h"
#include "debug/debug.param.h"
#include "map.h"
#include "memory/memory.param.h"
#include "sim.h"
#include "statistics.h"

#include "bp/tagescl.h"


#ifdef __cplusplus
extern "C" {
#endif
  void collect_br_stats(Op* op);
  void final_br_stat_print();
#ifdef __cplusplus
}
#endif
