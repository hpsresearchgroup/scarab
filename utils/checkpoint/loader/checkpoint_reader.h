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

#ifndef __CHECKPOINT_READER_H__
#define __CHECKPOINT_READER_H__

#include <cinttypes>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sys/mman.h>
#include <unordered_map>
#include <vector>

extern "C" {
#include "hconfig.h"
}

#include "utils.h"

static const int FPSTATE_SIZE = 2688;
extern char      fpstate_buffer[FPSTATE_SIZE];

void read_checkpoint(const char* checkpoint_dir);

void set_child_pid(pid_t pid);

void open_file_descriptors();

void change_working_directory();

uint64_t get_checkpoint_start_rip();

const char* get_checkpoint_exe_path();

void allocate_new_regions(pid_t child_pid);

void write_data_to_regions(pid_t child_pid);

void update_region_protections(pid_t child_pid);

void load_registers(pid_t child_id);

std::vector<char*> get_checkpoint_envp_vector();

std::vector<char*> get_checkpoint_argv_vector();

void get_checkpoint_os_info(std::string& kernel_release, std::string& os_version);
#endif
