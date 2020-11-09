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

#include <map>

#include "Memory.h"
#include "MemoryFactory.h"
#include "Request.h"
#include "ScarabWrapper.h"

#include "DDR3.h"
#include "DDR4.h"
#include "GDDR5.h"
#include "HBM.h"
#include "LPDDR3.h"
#include "LPDDR4.h"
#include "SALP.h"
#include "WideIO.h"
#include "WideIO2.h"

using namespace ramulator;

static map<string, function<MemoryBase*(const Config&, int, void (*)(int, int))>>
  name_to_func = {
    {"DDR3", &MemoryFactory<DDR3>::create},
    {"DDR4", &MemoryFactory<DDR4>::create},
    {"LPDDR3", &MemoryFactory<LPDDR3>::create},
    {"LPDDR4", &MemoryFactory<LPDDR4>::create},
    {"GDDR5", &MemoryFactory<GDDR5>::create},
    {"WideIO", &MemoryFactory<WideIO>::create},
    {"WideIO2", &MemoryFactory<WideIO2>::create},
    {"HBM", &MemoryFactory<HBM>::create},
    {"SALP-1", &MemoryFactory<SALP>::create},
    {"SALP-2", &MemoryFactory<SALP>::create},
    {"SALP-MASA", &MemoryFactory<SALP>::create},
};


ScarabWrapper::ScarabWrapper(const Config&      configs,
                             const unsigned int cacheline,
                             void (*stats_callback)(int,int)) {
  const string& std_name = configs["standard"];
  assert(name_to_func.find(std_name) != name_to_func.end() &&
         "unrecognized standard name");
  mem = name_to_func[std_name](configs, cacheline, stats_callback);
  // tCK = mem->clk_ns();
  Stats::statlist.output(configs["output_dir"] + "/ramulator.stat.out");
}


ScarabWrapper::~ScarabWrapper() {
  delete mem;
}

void ScarabWrapper::tick() {
  mem->tick();
}

bool ScarabWrapper::send(Request req) {
  return mem->send(req);
}

void ScarabWrapper::finish(void) {
  mem->finish();
  Stats::statlist.printall();
}

int ScarabWrapper::get_chip_width() const {
  return mem->get_chip_width();
}

int ScarabWrapper::get_chip_size() const {
  return mem->get_chip_size();
}

int ScarabWrapper::get_num_chips() const {
  return mem->get_num_chips();
}

int ScarabWrapper::get_chip_row_buffer_size() const {
  return mem->get_chip_row_buffer_size();
}
