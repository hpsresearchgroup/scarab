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
 * File         : libs/port_lib.c
 * Author       : HPS Research Group
 * Date         : 3/10/1999
 * Description  :
 ***************************************************************************************/

#include "debug/debug_macros.h"
#include "globals/assert.h"
#include "globals/global_defs.h"
#include "globals/global_types.h"
#include "globals/global_vars.h"

#include "libs/port_lib.h"

#include "debug/debug.param.h"


/**************************************************************************************/
/* Macros */

#define DEBUG(proc_id, args...) _DEBUG(proc_id, DEBUG_PORT_LIB, ##args)


/**************************************************************************************/
/* init_ports: */

void init_ports(Ports* ports, char name[], uns read, uns write,
                Flag writes_prevent_reads) {
  DEBUG(0, "Initializing ports called '%s'.\n", name);
  strncpy(ports->name, name, MAX_STR_LENGTH);
  ports->read_last_cycle      = 0;
  ports->write_last_cycle     = 0;
  ports->num_read_ports       = read;
  ports->read_ports_in_use    = 0;
  ports->num_write_ports      = write;
  ports->write_ports_in_use   = 0;
  ports->writes_prevent_reads = writes_prevent_reads;
}


/**************************************************************************************/
/* get_read_port: */

Flag get_read_port(Ports* ports) {
  if(ports->read_last_cycle != cycle_count) {
    ASSERT(0, ports->num_read_ports > 0);
    ports->read_last_cycle   = cycle_count;
    ports->read_ports_in_use = 1;
    DEBUG(0, "get_read_port successful\n");
    return SUCCESS;
  }

  if(ports->read_ports_in_use == ports->num_read_ports) {
    DEBUG(0, "get_read_port failed (%d ports in use)\n",
          ports->read_ports_in_use);
    return FAILURE;
  }
  if(ports->write_ports_in_use && ports->writes_prevent_reads) {
    DEBUG(0, "get_read_port failed (%d writes preventing reads)\n",
          ports->write_ports_in_use);
    return FAILURE;
  }

  DEBUG(0, "get_read_port successful\n");
  ports->read_ports_in_use++;
  return SUCCESS;
}


/**************************************************************************************/
/* get_write_port: */

Flag get_write_port(Ports* ports) {
  if(ports->write_last_cycle != cycle_count) {
    ASSERT(0, ports->num_write_ports > 0);
    ports->write_last_cycle   = cycle_count;
    ports->write_ports_in_use = 1;
    DEBUG(0, "get_write_port successful\n");
    return SUCCESS;
  }

  if(ports->write_ports_in_use == ports->num_write_ports) {
    DEBUG(0, "get_write_port failed (%d ports in use)\n",
          ports->write_ports_in_use);
    return FAILURE;
  }
  if(ports->writes_prevent_reads)
    ASSERTM(0, ports->read_ports_in_use == 0,
            "Must request write ports before reads.\n");

  DEBUG(0, "get_write_port successful\n");
  ports->write_ports_in_use++;
  return SUCCESS;
}
