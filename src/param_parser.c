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
* File         : param_parser.c
* Author       : HPS Research Group
* Date         : 1/30/1998
* Description  :

    This file contains functions to interpret parameters defined in .param
files.  For every parameter declaration in a .param file, a variable of the
given type is created and given the default value specified.  This variable can
be accessed anywhere as an extern by including '*_def.h'.  'get_params' is
called from the simulator initialization function to parse command line
parameter definitions.  Default values for the parameter variables are
overwritten by parameters specified on the command line. 'get_params' also reads
a file called 'PARAMS.in' to get pseduo command-line arguments. Lines in this
file should appear exactly as they would be typed on the command-line.  It is
still possible to pass command-line arguments; they override anything in the
file. If DUMP_USED_PARAMS is TRUE, a file called 'PARAMS.out' will be created
with all of the command-line and file arguments that were actually used to run
the program.  This way, an exact duplicate run can be performed.
***************************************************************************************/
#include <ctype.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include "globals/global_defs.h"
#include "globals/global_types.h"
#include "globals/global_vars.h"
#include "globals/utils.h"

#include "bp/bp.h"
#include "frontend/frontend_intf.h"
#include "model.h"
#include "param_parser.h"
#include "sim.h"
#include "stat_trace.h"

/**************************************************************************************/
/* Macros */

#define ARG_FILE_IN                                        \
  "PARAMS.in" /* the name of the parameter file to be used \
               */
/* (anything in it is overridden by the command-line args) */

#define ARG_FILE_OUT "PARAMS" /* the name of the parameter dump file */

#define ASSERTM(proc_id, cond, ...) \
  if(!(cond)) {                     \
    printf(__VA_ARGS__);            \
    fflush(stdout);                 \
    exit(0);                        \
  }


/**************************************************************************************/
/* Global Variables */

const char* help_options[]   = {"-help", "-h", "--help",
                              "--h"}; /* cmd-line help options strings */
const char* sim_mode_names[] = {"uop", "full"};

/**************************************************************************************/
/* include all header files with enum declarations used as parameters */

#include "globals/param_enum_headers.h"

/**************************************************************************************/
/* need to declare the variables that will be externed the .param.h files */

#define DEF_PARAM(name, variable, type, func, def, const) \
  const type variable = def;
#include "param_files.def"
#undef DEF_PARAM

/*The values of these parameters are computed from other parameters*/
uns NUM_FUS;
uns NUM_RS;


/**************************************************************************************/
/* set up an ENUM to give each parameter a unique number */

#define DEF_PARAM(name, variable, type, func, def, const) PARAM_ENUM_##name,
typedef enum param_enum {
#include "param_files.def"
  PARAM_ENUM_help,
  NUM_PARAMS,
} Param_Enum;
#undef DEF_PARAM


/**************************************************************************************/
/* define the options array for the call to getopt_long -- all parameters take a
 * value */

static int param_idx;

#define DEF_PARAM(name, variable, type, func, def, const) \
  {#name, TRUE, &param_idx, PARAM_ENUM_##name},
struct option long_options[] = {
#include "param_files.def"
  {"help", 0, &param_idx, PARAM_ENUM_help},
  {0, 0, 0, 0},
};
#undef DEF_PARAM

#define DEF_PARAM(name, variable, type, func, def, const) #const,
char* const_options[] = {
#include "param_files.def"
  "", "", ""};
#undef DEF_PARAM

#define DEF_PARAM(name, variable, type, func, def, const) \
  {#variable, #def, #const},
char* compiled_param_dump_array[][3] = {
#include "param_files.def"
  {0, 0, 0}};
#undef DEF_PARAM

/**************************************************************************************/

typedef struct Param_Record_struct {
  Flag used;
  char optarg[MAX_STR_LENGTH + 1];
} Param_Record;

void dump_params(char** arg_list, Param_Record used_params[], Flag exe_found);

/**************************************************************************************/
/* Local prototypes */

static void print_help(void);
void        mark_all_params_as_unused(Param_Record* used_params);
Flag        contains_help_options(int argc, char* argv[]);
Flag        param_file_exists(FILE* f);
char**      allocate_and_initialize_arg_list(int param_file_arg_count, int argc,
                                             char* argv[]);
int         get_next_parameter(FILE* f, char* buffer);
int         get_rest_of_line(FILE* f, char* buffer);
int         count_parameters_in_file(FILE* f);
Flag        param_is_exe_option(char* param);
Flag        contains_exe_option_in_file(FILE* param_file_fp);
Flag        contains_exe_option_in_args(char* argv[]);
Flag        param_is_comment(char* param);
uns add_arg_to_arg_list_at_index(char** arg_list, const char* arg, uns index);
int find_index_of_first_nonspace(char* str);
int remove_trailing_whitespace(char* str);
uns fill_arg_list_with_param_file_args(FILE*     param_file_fp,
                                       const int param_file_arg_count,
                                       char** arg_list, int argc);
uns add_all_param_file_args_to_arg_list(FILE*     param_file_fp,
                                        const uns param_file_arg_count,
                                        char** arg_list, int argc);
uns add_all_command_line_args_to_end_of_arg_list(char** arg_list,
                                                 uns    arg_list_index,
                                                 char*  argv[]);
uns get_param_file_args_and_command_line_args(char*** arg_list, int argc,
                                              char* argv[]);


/**************************************************************************************/
/**************************************************************************************/
/* get_bp_mech: Converts the optarg into a number by looking it up in the
 * bp_table. */

void get_bp_mech_param(const char* name, uns* variable) {
  if(optarg) {
    uns ii;

    for(ii = 0; bp_table[ii].name; ii++)
      if(strncmp(optarg, bp_table[ii].name, MAX_STR_LENGTH) == 0) {
        *variable = ii;
        return;
      }
    FATAL_ERROR(0, "Invalid value ('%s') for parameter '%s' --- Ignored.\n",
                optarg, name);
  } else
    FATAL_ERROR(0, "Parameter '%s' missing value --- Ignored.\n", name);
}


/**************************************************************************************/
/* get_btb_mech: Converts the optarg into a number by looking it up in the
 * bp_btb_table. */

void get_btb_mech_param(const char* name, uns* variable) {
  if(optarg) {
    uns ii;

    for(ii = 0; bp_ibtb_table[ii].name; ii++)
      if(strncmp(optarg, bp_ibtb_table[ii].name, MAX_STR_LENGTH) == 0) {
        *variable = ii;
        return;
      }
    FATAL_ERROR(0, "Invalid value ('%s') for parameter '%s' --- Ignored.\n",
                optarg, name);
  } else
    FATAL_ERROR(0, "Parameter '%s' missing value --- Ignored.\n", name);
}


/**************************************************************************************/
/* get_ibtb_mech: Converts the optarg into a number by looking it up in the
 * bp_ibtb_table. */

void get_ibtb_mech_param(const char* name, uns* variable) {
  if(optarg) {
    uns ii;

    for(ii = 0; bp_ibtb_table[ii].name; ii++)
      if(strncmp(optarg, bp_ibtb_table[ii].name, MAX_STR_LENGTH) == 0) {
        *variable = ii;
        return;
      }
    FATAL_ERROR(0, "Invalid value ('%s') for parameter '%s' --- Ignored.\n",
                optarg, name);
  } else
    FATAL_ERROR(0, "Parameter '%s' missing value --- Ignored.\n", name);
}

/**************************************************************************************/
/* get_conf_mech: Converts the optarg into a number by looking it up in the
 * bp_conf_table. */

void get_conf_mech_param(const char* name, uns* variable) {
  if(optarg) {
    uns ii;

    for(ii = 0; br_conf_table[ii].name; ii++)
      if(strncmp(optarg, br_conf_table[ii].name, MAX_STR_LENGTH) == 0) {
        *variable = ii;
        return;
      }
    FATAL_ERROR(0, "Invalid value ('%s') for parameter '%s' --- Ignored.\n",
                optarg, name);
  } else
    FATAL_ERROR(0, "Parameter '%s' missing value --- Ignored.\n", name);
}

/**************************************************************************************/
/* get_sim_mode: Converts the optarg string to a number by looking it up in the
   sim_mode_names array.  The index corresponds to a Sim_Mode enum entry, which
   determines the sim mode that is run from the 'main' function. */

void get_sim_mode_param(const char* name, Generic_Enum* variable) {
  if(optarg) {
    uns ii;

    for(ii = 0; ii < NUM_SIM_MODES; ii++)
      if(strncmp(optarg, sim_mode_names[ii], MAX_STR_LENGTH) == 0) {
        *variable = ii;
        return;
      }
    FATAL_ERROR(0, "Invalid value ('%s') for parameter '%s' --- Ignored.\n",
                optarg, name);
  } else
    FATAL_ERROR(0, "Parameter '%s' missing value --- Ignored.\n", name);
}


/**************************************************************************************/
/* get_sim_model: Converts the optarg string to a number by looking it up in the
   model_table array.  The index corresponds to the entry index, which
   determines the type of simulator model that will be used. */

void get_sim_model_param(const char* name, uns* variable) {
  if(optarg) {
    uns ii;

    for(ii = 0; model_table[ii].name; ii++)
      if(strncmp(optarg, model_table[ii].name, MAX_STR_LENGTH) == 0) {
        *variable = ii;
        return;
      }
    FATAL_ERROR(0, "Invalid value ('%s') for parameter '%s' --- Ignored.\n",
                optarg, name);
  } else
    FATAL_ERROR(0, "Parameter '%s' missing value --- Ignored.\n", name);
}

/**************************************************************************************/
/* get_frontend: Converts the optarg string to a number by looking it up in the
   frontend_table array.  The index corresponds to the entry index, which
   determines the type of simulator model that will be used. */

void get_frontend_param(const char* name, uns* variable) {
  if(optarg) {
    uns ii;

    for(ii = 0; frontend_table[ii].name; ii++)
      if(strncmp(optarg, frontend_table[ii].name, MAX_STR_LENGTH) == 0) {
        *variable = ii;
        return;
      }
    FATAL_ERROR(0, "Invalid value ('%s') for parameter '%s' --- Ignored.\n",
                optarg, name);
  } else
    FATAL_ERROR(0, "Parameter '%s' missing value --- Ignored.\n", name);
}


/**************************************************************************************/
/* get_dram_sched: Converts the optarg string to a number by looking it up in
   the dram_sched_table array.  The index corresponds to the entry index, which
   determines the type of simulator model that will be used. */

// Ramulator_remove
// void get_dram_sched_param (const char *name, uns *variable)
// {
//     if (optarg) {
//         uns ii;
//
//         for (ii = 0 ; dram_sched_table[ii].name ; ii++)
//             if (strncmp(optarg, dram_sched_table[ii].name, MAX_STR_LENGTH) ==
//             0) {
//                 *variable = ii;
//                 return;
//             }
//         FATAL_ERROR(0,"Invalid value ('%s') for parameter '%s' ---
//         Ignored.\n", optarg, name);
//     } else
//         FATAL_ERROR(0,"Parameter '%s' missing value --- Ignored.\n", name);
// }


/**************************************************************************************/
/* get_float_param: Converts the optarg string to an float and assigns it to the
   parameter variable. */

void get_float_param(const char* name, float* variable) {
  if(optarg)
    *variable = strtod(optarg, NULL);
  else
    FATAL_ERROR(0, "Parameter '%s' missing value --- Ignored.\n", name);
}


/**************************************************************************************/
/* get_int_param: Converts the optarg string to an integer and assigns it to the
   parameter variable. */

void get_int_param(const char* name, int* variable) {
  if(optarg)
    *variable = strtol(optarg, NULL, 0);
  else
    FATAL_ERROR(0, "Parameter '%s' missing value --- Ignored.\n", name);
}


/**************************************************************************************/
/* get_uns_param: Converts the optarg string to an unseger and assigns it to the
   parameter variable. */

void get_uns_param(const char* name, uns* variable) {
  if(optarg)
    *variable = strtoul(optarg, NULL, 0);
  else
    FATAL_ERROR(0, "Parameter '%s' missing value --- Ignored.\n", name);
}


/**************************************************************************************/
/* get_uns8_param: Converts the optarg string to a flag and assigns it to the
   parameter variable. */

void get_uns8_param(const char* name, uns8* variable) {
  if(optarg)
    *variable = strtoul(optarg, NULL, 0);
  else
    FATAL_ERROR(0, "Parameter '%s' missing value --- Ignored.\n", name);
}


/**************************************************************************************/
/* get_Flag_param: Converts the optarg string to a flag and assigns it to the
   parameter variable. */

void get_Flag_param(const char* name, Flag* variable) {
  if(optarg)
    *variable = strtoul(optarg, NULL, 0) ? 1 : 0;
  else
    FATAL_ERROR(0, "Parameter '%s' missing value --- Ignored.\n", name);
}


/**************************************************************************************/
/* get_string_param: copies the optarg string to a parameter variable. */

void get_string_param(const char* name, char** variable) {
  if(optarg)
    /* this may waste some memory...screw it */
    *variable = strdup(optarg);
  else
    FATAL_ERROR(0, "Parameter '%s' missing value --- Ignored.\n", name);
}


/**************************************************************************************/
/* get_strlist_param: gets a string and adds it to the parameter value, which is
   an array of all strings specified for this param. */

void get_strlist_param(const char* name, char*** variable) {
  static int count = 0;

  if(optarg) {
    *variable            = realloc(count == 0 ? NULL : *variable,
                        sizeof(char*) * (count + 2));
    (*variable)[count]   = strdup(optarg);
    (*variable)[++count] = NULL;
  } else
    FATAL_ERROR(0, "Parameter '%s' missing value --- Ignored.\n", name);
}


/**************************************************************************************/
/* get_uns64_param: */

void get_uns64_param(const char* name, uns64* variable) {
  if(optarg)
    *variable = strtoull(optarg, NULL, 0);
  else
    FATAL_ERROR(0, "Parameter '%s' missing value --- Ignored.\n", name);
}

/**************************************************************************************/
/* dump_params: */

void dump_params(char** arg_list, Param_Record used_params[], Flag exe_found) {
  int   ii;
  FILE* arg_stream_out = file_tag_fopen(NULL, ARG_FILE_OUT, "w");
  if(!arg_stream_out) {
    WARNINGU(
      0, "Couldn't open parameter output file %s.out --- Dumping to stderr.\n",
      ARG_FILE_OUT);
    arg_stream_out = stderr;
  }
  for(ii = 0; ii < NUM_PARAMS; ii++)
    if(used_params[ii].used)
      fprintf(arg_stream_out, "--%s %s\n", long_options[ii].name,
              used_params[ii].optarg);
  if(exe_found)
    fprintf(arg_stream_out, "--exe ");
  for(ii = optind; arg_list[ii]; ii++)
    fprintf(arg_stream_out, "%s ", arg_list[ii]);

  fprintf(
    arg_stream_out,
    "\n\n--- Cut out everything below to use this file as PARAMS.in ---\n\n");

  fprintf(arg_stream_out, "Parameter status at compile time and values "
                          "supplied on the command line:\n\n");
  for(ii = 0; compiled_param_dump_array[ii][0]; ii++) {
    fprintf(arg_stream_out, "%-40s %-20s %10s ",
            compiled_param_dump_array[ii][0], compiled_param_dump_array[ii][1],
            compiled_param_dump_array[ii][2]);
    if(used_params[ii].used)
      fprintf(arg_stream_out, "%s", used_params[ii].optarg);
    fprintf(arg_stream_out, "\n");
  }


  if(arg_stream_out != stderr)
    fclose(arg_stream_out);
}


  /**************************************************************************************/
  /* get_params: Parses argv and a default file for any long options and calls
     the appropriate function when it finds one.  It also returns a pointer to
     the start of the simulated command's argv string.  */

#define DEF_PARAM(name, variable, type, func, def, const)                   \
  case PARAM_ENUM_##name:                                                   \
    get_##func##_param(#name, (type*)&variable);                            \
    used_params[PARAM_ENUM_##name].used = TRUE;                             \
    strncpy(used_params[PARAM_ENUM_##name].optarg, optarg, MAX_STR_LENGTH); \
    break;

void mark_all_params_as_unused(Param_Record* used_params) {
  uns ii;

  for(ii = 0; ii < NUM_PARAMS; ii++)
    used_params[ii].used = FALSE;
}

Flag contains_help_options(int argc, char* argv[]) {
  return (argc > 1 && strin(argv[1], help_options,
                            sizeof(help_options) / sizeof(char*)) != -1);
}

Flag param_file_exists(FILE* f) {
  return f != NULL;
}

char** allocate_and_initialize_arg_list(int param_file_arg_count, int argc,
                                        char* argv[]) {
  char** arg_list;
  arg_list = (char**)malloc(sizeof(char*) * (param_file_arg_count + argc + 1));
  arg_list[0] = (char*)strdup(argv[0]);        /* copy exec name over */
  arg_list[param_file_arg_count + argc] = 0x0; /* Sentinal. Put here initially
                                                  so we can make sure it is not
                                                  overwritten later.*/
  return arg_list;
}

int get_next_parameter(FILE* f, char* buffer) {
  return fscanf(f, "%s", buffer) != EOF;
}

int get_rest_of_line(FILE* f, char* buffer) {
  return (fscanf(f, "%[^\n\r]", buffer) != EOF);
}

/* Counts parameters and values. Values always count as 1, no matter
 * how many per line there are.
 *
 * Example File format:
 *    --parameter1 value
 *    --parameter2 value1 value2 value3
 *
 * Example Return:
 *    4    //(2 parameters + 2 values)
 */
int count_parameters_in_file(FILE* f) {
  char param_name[MAX_STR_LENGTH + 1];
  char param_val[MAX_STR_LENGTH + 1];
  int  param_file_arg_count = 0;

  while(get_next_parameter(f, param_name)) {
    if(!param_is_comment(param_name)) {
      param_file_arg_count++;
      if(get_rest_of_line(f, param_val))
        param_file_arg_count++;
    } else {
      get_rest_of_line(f, param_val);
    }
  }

  rewind(f);
  return param_file_arg_count;
}

Flag param_is_exe_option(char* param) {
  return (strncmp(param, "--exe", MAX_STR_LENGTH) == 0);
}

Flag contains_exe_option_in_file(FILE* param_file_fp) {
  char param_name[MAX_STR_LENGTH + 1];
  char param_val[MAX_STR_LENGTH + 1];

  while(get_next_parameter(param_file_fp, param_name)) {
    if(param_is_exe_option(param_name)) {
      return TRUE;
    }
    get_rest_of_line(param_file_fp, param_val);
  }
  return FALSE;
}

Flag contains_exe_option_in_args(char* argv[]) {
  uns i;
  for(i = 1; argv[i]; i++) {
    if(param_is_exe_option(argv[i]))
      return TRUE;
  }
  return FALSE;
}

Flag param_is_comment(char* param) {
  int i = find_index_of_first_nonspace(param);
  return (param[i] == '#');
}

uns add_arg_to_arg_list_at_index(char** arg_list, const char* arg, uns index) {
  arg_list[index] = (char*)strdup(arg);
  return index;
}

int find_index_of_first_nonspace(char* str) {
  int b = 0;
  while(isspace(str[b]))
    ++b;
  return b;
}

// returns index of last non-whitespace character
int remove_trailing_whitespace(char* str) {
  int e = strlen(str) - 1;  // point last char in str
  while(e >= 0 && isspace(str[e]))
    --e;
  if(e >= 0 && (e + 1 < strlen(str)))
    str[e + 1] = 0;  // remove whitespace from end of line
  return e;
}

uns fill_arg_list_with_param_file_args(FILE*     param_file_fp,
                                       const int param_file_arg_count,
                                       char** arg_list, int argc) {
  char param_name[MAX_STR_LENGTH + 1];
  char param_val[MAX_STR_LENGTH + 1];
  uns  exe_option_found     = 0;
  uns  arg_list_index       = 1;
  uns  validation_arg_count = 0; /*used to verify number of args counted earlier
                                    matches number read from file*/

  while(get_next_parameter(param_file_fp, param_name)) {
    if(!param_is_comment(param_name)) { /* comment, ignore argument */

      if(param_is_exe_option(param_name)) {
        /* Want to put --exe at the end of arg_list. The --exe option indicates
         * that there are no more Scarab params in this file. Therefore, skip
         * enough locations for the command line arguments, then continue.*/
        exe_option_found = arg_list_index;
        arg_list_index += argc - 1;
      }

      add_arg_to_arg_list_at_index(arg_list, param_name, arg_list_index);
      arg_list_index++;
      validation_arg_count++;

      int num_chars = get_rest_of_line(param_file_fp, param_val);
      ASSERTM(0, num_chars < MAX_STR_LENGTH, "Arg %s exceedes max length",
              param_name);  // will read at most MAX_STR_LENGTH of char's in
      if(num_chars != EOF) {
        int index_of_first_char = find_index_of_first_nonspace(param_val);
        remove_trailing_whitespace(param_val);
        add_arg_to_arg_list_at_index(arg_list, &param_val[index_of_first_char],
                                     arg_list_index);
        arg_list_index++;
        validation_arg_count++;
      }
    } else {
      // ignore rest of comment
      get_rest_of_line(param_file_fp, param_val);
    }
  }
  ASSERTM(0, param_file_arg_count == validation_arg_count,
          "First count of args (%d) differs from Second count of args (%d)\n",
          param_file_arg_count, validation_arg_count);

  if(exe_option_found > 0) {
    /*--exe option was found, and space was allocated for command line
     * arguemnts. We want the arg_list_index to point to the top of that
     * space.*/
    arg_list_index = exe_option_found;
  }

  return arg_list_index; /*returns the index for command line args to be
                            placed*/
}

uns add_all_param_file_args_to_arg_list(FILE*     param_file_fp,
                                        const uns param_file_arg_count,
                                        char** arg_list, int argc) {
  uns arg_list_index = 1; /*Points to the next empty slot in arg_list*/

  if(param_file_arg_count > 0) {
    arg_list_index = fill_arg_list_with_param_file_args(
      param_file_fp, param_file_arg_count, arg_list, argc);
  }
  ASSERTM(0, arg_list_index == param_file_arg_count + 1,
          "Parsed too many options from PARAM.in\n");
  return arg_list_index; /*returns index for command line args to be placed*/
}

uns add_all_command_line_args_to_end_of_arg_list(char** arg_list,
                                                 uns    arg_list_index,
                                                 char*  argv[]) {
  uns i;
  uns exe_option_found = 0;
  for(i = 1; argv[i]; i++, arg_list_index++) {
    if(param_is_exe_option(argv[i]))
      exe_option_found = arg_list_index;

    add_arg_to_arg_list_at_index(arg_list, argv[i], arg_list_index);
  }
  if(exe_option_found) {
    /*Ensure this is the last arg by writing a sentinal here. Command line args
     * are added after param file args, so in general this should be the last
     * arg by default. However, if the param file also contains a --exe options
     * then it will already be at the end of the arg list. By writting a 0 we
     * are ignoring the exe specified by the param file.
     */
    arg_list[arg_list_index] = 0x0;
  }
  return arg_list_index;
}

/* Since getopt is does _not_ like to be called more than once, we need to
 * create a single array to pass.  Command line options need to be parsed after
 * what is in the file, so they are put at the end of the concatenated list.
 * However, the -exe option in the file needs to go at the end of the
 * concatenated list.  We've also got to be sure that a null word (all 0's)
 * follows the end of the first -exe option.
 *
 * parses PARAMS.in file and command line arguments. Returns them as single list
 * (arg_list).
 *
 * Guarentees that --exe option is at the end of the arg_list. If --exe option
 * is in both PARAMS.in and the command line, then PARAMS.in is ignored
 */
uns get_param_file_args_and_command_line_args(char*** arg_list_ptr, int argc,
                                              char* argv[]) {
  FILE* param_file_fp        = fopen(ARG_FILE_IN, "r"); /*The PARAMS.in file*/
  uns   param_file_arg_count = 0; /*Count arguments in param file (like argc for
                                     the command line)*/
  char** arg_list = NULL;

  uns command_line_arg_index = 1;
  if(!param_file_exists(param_file_fp)) {
    WARNINGU(0,
             "Parameter file '%s' not found --- Using hard-coded defaults and "
             "command-line arguments only.\n",
             ARG_FILE_IN);
    arg_list = allocate_and_initialize_arg_list(param_file_arg_count, argc,
                                                argv);
  } else {
    param_file_arg_count = count_parameters_in_file(param_file_fp);
    arg_list = allocate_and_initialize_arg_list(param_file_arg_count, argc,
                                                argv);
    command_line_arg_index = add_all_param_file_args_to_arg_list(
      param_file_fp, param_file_arg_count, arg_list, argc);
  }
  add_all_command_line_args_to_end_of_arg_list(arg_list, command_line_arg_index,
                                               argv);

  fclose(param_file_fp);
  ASSERTM(0, arg_list[param_file_arg_count + argc] == 0x0,
          "2: Reading in parameters overflowed the space allocated for the "
          "args_list\n");

  *arg_list_ptr = arg_list;
  return param_file_arg_count +
         argc; /*Return the total number of args in the arg_list*/
}

char** get_params(int argc, char* argv[]) {
  uns arg_list_count; /*Count of all args and values in the arg_list (like argc
                         for the command line)*/
  char** arg_list = NULL; /*Merged list of all args and values from PARAMS.in
                             and the command line (like argv for the command
                             line)*/
  Param_Record used_params[NUM_PARAMS]; /*Keeps track of the values that are
                                           actually used by the simulator. */

  if(contains_help_options(argc, argv)) {
    print_help();
    exit(0);
  }

  arg_list_count = get_param_file_args_and_command_line_args(&arg_list, argc,
                                                             argv);

  int temp_index = 0;
  param_idx      = -1;
  opterr         = 0;  // Suppress getopt_long's error message (we have our own)
  mark_all_params_as_unused(used_params);
  while(getopt_long(arg_list_count, arg_list, "", long_options, &temp_index) !=
        -1) {
    int index = param_idx;
    param_idx = -1;
    if(index == -1) {
      FATAL_ERROR(0, "Unknown parameter '%s'\n", arg_list[optind - 1]);
    }
    if(strncmp(const_options[index], "const", MAX_STR_LENGTH) == 0) {
      FATAL_ERROR(0, "Cannot set parameter '%s' compiled as a constant.\n",
                  long_options[index].name);
      continue;
    }
    switch(index) {
#include "param_files.def"
      case PARAM_ENUM_help:
        if(system("cat ../doc/cmd-line_options") != 0)
          if(system("cat $SIMDIR/doc/cmd-line_options") != 0)
            ERROR(0, "File 'cmd-line_options' could not be found.\n");
        break;
      default:
        FATAL_ERROR(0, "Unknown command-line option found (index:%u).\n",
                    index);
    }
  }

  // Set global size variables.
  NUM_RS   = num_tokens(RS_SIZES, DELIMITERS);
  uns temp = num_tokens(RS_CONNECTIONS, DELIMITERS);
  NUM_FUS  = num_tokens(FU_TYPES, DELIMITERS);
  ASSERTM(0, NUM_RS == temp,
          "Number of elements in RS_SIZES(%d) must match number of elements in "
          "RS_CONNECTIONS(%d)",
          NUM_RS, temp);

  if(FRONTEND == FE_TRACE && !CBP_TRACE_R0) {
    if(SIM_MODEL != DUMB_MODEL) {
      FATAL_ERROR(0, "Trace frontend specified, but no trace file specified "
                     "(use --cbp_trace_r0).\n");
    }
  }

  ASSERTM(0, arg_list[arg_list_count] == 0x0,
          "3: Reading in parameters overflowed the space allocated for the "
          "args_list\n");
  dump_params(arg_list, used_params, FALSE);
  return &arg_list[optind]; /* return pointer to simulated argv */
}

static void print_help(void) {
  const char* help =
    "Scarab command-line option summary:\n"
    "\n"
    "    --help\n"
    "        Displays usage information (this message).\n"
    "\n"
    "    --sim_limit=<trigger spec>\n"
    "        When should Scarab stop? Examples of <trigger spec>:\n"
    "            none          When the application finishes (Default)\n"
    "            i[1]:10000    After 10000 instructions retire in core 1\n"
    "            c[0]:20000    After 20000 core 0 cycles (may differ from\n"
    "                          core 1 cycles if core frequencies differ)\n"
    "            t:40000000    After 40000000 simulated femtoseconds\n"
    "            <stat>[2]:50  After specified <stat> reaches 50\n"
    "\n"
    "    --exe <cmd-line>\n"
    "        Signals the beginning of the simulated command to execute.\n"
    "        Everything after '--exe' is assumed to be part of the\n"
    "        simulated command and is not parsed for Scarab options.  This\n"
    "        must be the last option given in the PARAMS.in file or on the\n"
    "        command line.  (No Default)\n"
    "\n"
    "Other options are listed in *.param.def files in the src directory.\n";

  printf("%s", help);
}

#undef DEF_PARAM
