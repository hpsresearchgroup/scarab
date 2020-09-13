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
 * File         : libs/port_lib.h
 * Author       : HPS Research Group
 * Date         : 3/10/1999
 * Description  :
 ***************************************************************************************/

#ifndef __PORT_LIB_H__
#define __PORT_LIB_H__

#include "globals/global_defs.h"
#include "globals/global_types.h"


/**************************************************************************************/
/* Types */

// typedef in globals/global_types.h
struct Ports_struct {
  char    name[MAX_STR_LENGTH + 1];
  Counter read_last_cycle;
  Counter write_last_cycle;
  uns     num_read_ports;
  uns     read_ports_in_use;
  uns     num_write_ports;
  uns     write_ports_in_use;
  Flag    writes_prevent_reads;
};


/**************************************************************************************/
/* Prototypes */

void init_ports(Ports* ports, char[], uns, uns, Flag);
Flag get_read_port(Ports* ports);
Flag get_write_port(Ports* ports);


/**************************************************************************************/


#endif /* #ifndef __PORT_LIB_H__ */
