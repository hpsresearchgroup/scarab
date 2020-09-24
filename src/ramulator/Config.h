/* Copyright 2020 HPS/SAFARI Research Groups
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#include <string>
#include <fstream>
#include <vector>
#include <map>
#include <iostream>
#include <cassert>

namespace ramulator
{

    enum class StatCallbackType : int 
    {
        DRAM_ACT,
        DRAM_PRE,
        DRAM_READ,
        DRAM_WRITE,
        MAX
    };

class Config {

private:
    std::map<std::string, std::string> options;
    int channels;
    int ranks;
    int subarrays;
    int cpu_tick;
    int mem_tick;
    int core_num = 0;
    long expected_limit_insts = 0;
    long warmup_insts = 0;

	std::map<std::string, std::string> defaults = {
        // DRAM and Memory Controller
        {"standard", "DDR4"},
        {"speed", "DDR4_3200"},
        {"org", "DDR4_8Gb_x16"},
        {"channels", "1"},
        {"ranks", "1"},

		// Request Queues
		{"readq_entries", "64"},
		{"writeq_entries", "64"},

        
        // Other
        {"record_cmd_trace", "off"},
        {"print_cmd_trace", "off"},
        {"use_rest_of_addr_as_row_addr", "on"}
    };

	template<typename T>
    T get(const std::string& param_name, T (*cast_func)(const std::string&)) const {

        std::string param = this->operator[](param_name);

        if(param == "") {
            param = defaults.at(param_name); // get the default param, if exists

            if(param == "") {
                std::cerr << "ERROR: All options should have their default values in Config.h!" << std::endl;
                std::cerr << "No default value found for: " << param_name << std::endl;
                exit(-1);
            }
        }

        try {
            return (*cast_func)(param);
        } catch (const std::invalid_argument& ia) {
            std::cerr << "Invalid argument: " << ia.what() << std::endl;
            exit(-1);
        } catch (const std::out_of_range& oor) {
            std::cerr << "Out of Range error: " << oor.what() << std::endl;
            exit(-1);
        } catch (...) {
            std::cerr << "Error! Unhandled exception." << std::endl;
            std::exit(-1);
        }

        return T();
    }

public:
    Config() {}
    Config(const std::string& fname);
    void parse(const std::string& fname);
    std::string operator [] (const std::string& name) const {
      if (options.find(name) != options.end()) {
        return (options.find(name))->second;
      } else {
        return "";
      }
    }

	int get_int(const std::string& param_name) const {

        return get<int>(param_name, [](const std::string& s){ return std::stoi(s); }); // Hasan: the lambda function trick helps ignoring the optional argument of stoi
    }

    bool contains(const std::string& name) const {
      if (options.find(name) != options.end()) {
        return true;
      } else {
        return false;
      }
    }

    void add (const std::string& name, const std::string& value) {
      if (!contains(name)) {
        options.insert(make_pair(name, value));
      } else {
        printf("ramulator::Config::add options[%s] already set.\n", name.c_str());
      }
    }

    void set_core_num(int _core_num) {core_num = _core_num;}

    int get_channels() const {return channels;}
    int get_subarrays() const {return subarrays;}
    int get_ranks() const {return ranks;}
    int get_cpu_tick() const {return cpu_tick;}
    int get_mem_tick() const {return mem_tick;}
    int get_core_num() const {return core_num;}
    long get_expected_limit_insts() const {return expected_limit_insts;}
    long get_warmup_insts() const {return warmup_insts;}

    bool has_l3_cache() const {
      if (options.find("cache") != options.end()) {
        const std::string& cache_option = (options.find("cache"))->second;
        return (cache_option == "all") || (cache_option == "L3");
      } else {
        return false;
      }
    }
    bool has_core_caches() const {
      if (options.find("cache") != options.end()) {
        const std::string& cache_option = (options.find("cache"))->second;
        return (cache_option == "all" || cache_option == "L1L2");
      } else {
        return false;
      }
    }
    bool is_early_exit() const {
      // the default value is true
      if (options.find("early_exit") != options.end()) {
        if ((options.find("early_exit"))->second == "off") {
          return false;
        }
        return true;
      }
      return true;
    }
    bool calc_weighted_speedup() const {
      return (expected_limit_insts != 0);
    }
    bool record_cmd_trace() const {
      // the default value is false
      if (options.find("record_cmd_trace") != options.end()) {
        if ((options.find("record_cmd_trace"))->second == "on") {
          return true;
        }
        return false;
      }
      return false;
    }
    bool print_cmd_trace() const {
      // the default value is false
      if (options.find("print_cmd_trace") != options.end()) {
        if ((options.find("print_cmd_trace"))->second == "on") {
          return true;
        }
        return false;
      }
      return false;
    }
    bool use_rest_of_addr_as_row_addr() const {
      if (options.find("use_rest_of_addr_as_row_addr") != options.end()) {
        if ((options.find("use_rest_of_addr_as_row_addr"))->second == "on") {
          return true;
        }
        return false;
      }
      return false;
    }
};


} /* namespace ramulator */

#endif /* _CONFIG_H */

