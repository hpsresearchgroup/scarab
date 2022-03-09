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
 * File         : param_parser.h
 * Author       : HPS Research Group
 * Date         : 1/30/1998
 * Description  : Header for parameters.c
 ***************************************************************************************/

#ifndef __PARAM_PARSER_H__
#define __PARAM_PARSER_H__


/**************************************************************************************/
/* Global Variables */

extern const char* sim_mode_names[];


/**************************************************************************************/
/* Prototypes */

char** get_params(int, char* []);
void   get_bp_mech_param(const char*, uns*);
void   get_btb_mech_param(const char*, uns*);
void   get_ibtb_mech_param(const char*, uns*);
void   get_conf_mech_param(const char*, uns*);
void   get_sim_mode_param(const char*, Generic_Enum*);
void   get_exit_cond_param(const char*, Generic_Enum*);
void   get_sim_model_param(const char*, uns*);
void   get_frontend_param(const char*, uns*);
// void get_dram_sched_param(const char *, uns *); // Ramulator_remove
void get_float_param(const char*, float*);
void get_int_param(const char*, int*);
void get_uns_param(const char*, uns*);
void get_uns8_param(const char*, uns8*);
void get_Flag_param(const char*, uns8*);
void get_string_param(const char*, char**);
void get_strlist_param(const char*, char***);
void get_uns64_param(const char*, uns64*);


/**************************************************************************************/

#endif /* #ifndef __PARAM_PARSER_H__ */
