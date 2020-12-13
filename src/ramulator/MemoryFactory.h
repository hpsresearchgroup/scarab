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

#ifndef __MEMORY_FACTORY_H
#define __MEMORY_FACTORY_H

#include <cassert>
#include <map>
#include <string>

#include "Config.h"
#include "Memory.h"

#include "SALP.h"
#include "WideIO2.h"

using namespace std;

namespace ramulator {

template <typename T>
class MemoryFactory {
 public:
  static void extend_channel_width(T* spec, int cacheline) {
    int channel_unit = spec->prefetch_size * spec->channel_width / 8;
    int gang_number  = cacheline / channel_unit;

    assert(gang_number == 1 &&
           "cacheline size must match the channel width");  // Hasan: preventing
                                                            // automatically
                                                            // extending the
                                                            // channel width

    assert(gang_number >= 1 &&
           "cacheline size must be greater or equal to minimum channel width");

    assert(cacheline == gang_number * channel_unit &&
           "cacheline size must be a multiple of minimum channel width");

    spec->channel_width *= gang_number;
  }

  static Memory<T>* populate_memory(const Config& configs, T* spec,
                                    int channels, int ranks,
                                    void (*stats_callback)(int, int)) {
    // int& default_ranks = spec->org_entry.count[int(T::Level::Rank)];
    // int& default_channels = spec->org_entry.count[int(T::Level::Channel)];

    // if (default_channels == 0) default_channels = channels;
    // if (default_ranks == 0) default_ranks = ranks;

    vector<Controller<T>*> ctrls;
    for(int c = 0; c < channels; c++) {
      DRAM<T>* channel = new DRAM<T>(spec, T::Level::Channel);
      channel->id      = c;
      channel->regStats("");
      ctrls.push_back(new Controller<T>(configs, channel, stats_callback));
    }
    return new Memory<T>(configs, ctrls);
  }

  static void validate(int channels, int ranks, const Config& configs) {
    assert(channels > 0 && ranks > 0);
  }

  static MemoryBase* create(const Config& configs, int cacheline,
                            void (*stats_callback)(int, int)) {
    int channels = stoi(configs["channels"], NULL, 0);
    int ranks    = stoi(configs["ranks"], NULL, 0);

    validate(channels, ranks, configs);

    //const string& org_name = configs["org"];
    //const string& speed_name = configs["speed"];

    // T *spec = new T(org_name, speed_name);
    T* spec = new T(configs);

    extend_channel_width(spec, cacheline);

    return (MemoryBase*)populate_memory(configs, spec, channels, ranks,
                                        stats_callback);
  }
};

template <>
MemoryBase* MemoryFactory<WideIO2>::create(const Config& configs, int cacheline,
                                           void (*stats_callback)(int, int));
template <>
MemoryBase* MemoryFactory<SALP>::create(const Config& configs, int cacheline,
                                        void (*stats_callback)(int, int));

} /*namespace ramulator*/

#endif /*__MEMORY_FACTORY_H*/
