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
 * File         : map_stage.h
 * Author       : HPS Research Group
 * Date         : 2/4/1999
 * Description  :
 ***************************************************************************************/

#ifndef __MAP_STAGE_H__
#define __MAP_STAGE_H__

#include "stage_data.h"


/**************************************************************************************/
/* Types */

typedef struct Map_Stage_struct {
  uns         proc_id;
  Stage_Data* sds;     /* stage interface data (dynamically
                        * allocated number of pipe stages) */
  Stage_Data* last_sd; /* pointer to last decode pipeline stage
                        * (for passing ops to map) */
} Map_Stage;


/**************************************************************************************/
/* External Variables */

extern Map_Stage* map;


/**************************************************************************************/
/* prototypes */

/* vanilla hps model */
void set_map_stage(Map_Stage*);
void init_map_stage(uns8, const char*);
void reset_map_stage(void);
void recover_map_stage(void);
void debug_map_stage(void);
void update_map_stage(Stage_Data*);


/**************************************************************************************/

#endif /* #ifndef __MAP_STAGE_H__ */
