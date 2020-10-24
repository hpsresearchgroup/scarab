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

#ifndef __SCHEDULER_H
#define __SCHEDULER_H

#include <cassert>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <unordered_map>
#include <vector>
#include "Controller.h"
#include "DRAM.h"
#include "Request.h"

using namespace std;

namespace ramulator {

template <typename T>
class Controller;

template <typename T>
class Scheduler {
 public:
  Controller<T>* ctrl;

  enum class Policy {
    FCFS,
    FRFCFS,
    FRFCFS_Cap,
    FRFCFS_PriorHit,
    MAX
  } policy = Policy::FRFCFS_Cap;

  long cap = 16;

  Scheduler(Controller<T>* ctrl, const Config& configs) : ctrl(ctrl) {
    string policy_str = configs["scheduling_policy"];

    if(policy_str == "FCFS")
      policy = Policy::FCFS;
    else if(policy_str == "FRFCFS")
      policy = Policy::FRFCFS;
    else if(policy_str == "FRFCFS_Cap")
      policy = Policy::FRFCFS_Cap;
    else if(policy_str == "FRFCFS_PriorHit")
      policy = Policy::FRFCFS_PriorHit;
    else
      assert(false && "Unknown memory request scheduler. Please make \
sure to set RAMULATOR_SCHEDULING_POLICY to one of the \
available policies: FCFS, FRFCFS, FRFCFS_Cap, \
FRFCFS_PriorHit");
  }

  list<Request>::iterator get_head(list<Request>& q) {
    // TODO make the decision at compile time
    if(policy != Policy::FRFCFS_PriorHit) {
      if(!q.size())
        return q.end();

      auto head = q.begin();
      for(auto itr = next(q.begin(), 1); itr != q.end(); itr++)
        head = compare[int(policy)](head, itr);

      return head;
    } else {
      if(!q.size())
        return q.end();

      auto head = q.begin();
      for(auto itr = next(q.begin(), 1); itr != q.end(); itr++) {
        head = compare[int(Policy::FRFCFS_PriorHit)](head, itr);
      }

      if(this->ctrl->is_ready(head) && this->ctrl->is_row_hit(head)) {
        return head;
      }

      // prepare a list of hit request
      vector<vector<int>> hit_reqs;
      for(auto itr = q.begin(); itr != q.end(); ++itr) {
        if(this->ctrl->is_row_hit(itr)) {
          auto begin = itr->addr_vec.begin();
          // TODO Here it assumes all DRAM standards use PRE to close a row
          // It's better to make it more general.
          auto end = begin +
                     int(ctrl->channel->spec->scope[int(T::Command::PRE)]) + 1;
          vector<int> rowgroup(begin, end);  // bank or subarray
          hit_reqs.push_back(rowgroup);
        }
      }
      // if we can't find proper request, we need to return q.end(),
      // so that no command will be scheduled
      head = q.end();
      for(auto itr = q.begin(); itr != q.end(); itr++) {
        bool violate_hit = false;
        if((!this->ctrl->is_row_hit(itr)) && this->ctrl->is_row_open(itr)) {
          // so the next instruction to be scheduled is PRE, might violate hit
          auto begin = itr->addr_vec.begin();
          // TODO Here it assumes all DRAM standards use PRE to close a row
          // It's better to make it more general.
          auto end = begin +
                     int(ctrl->channel->spec->scope[int(T::Command::PRE)]) + 1;
          vector<int> rowgroup(begin, end);  // bank or subarray
          for(const auto& hit_req_rowgroup : hit_reqs) {
            if(rowgroup == hit_req_rowgroup) {
              violate_hit = true;
              break;
            }
          }
        }
        if(violate_hit) {
          continue;
        }
        // If it comes here, that means it won't violate any hit request
        if(head == q.end()) {
          head = itr;
        } else {
          head = compare[int(Policy::FRFCFS)](head, itr);
        }
      }

      return head;
    }
  }

 private:
  typedef list<Request>::iterator     ReqIter;
  function<ReqIter(ReqIter, ReqIter)> compare[int(Policy::MAX)] = {
    // FCFS
    [this](ReqIter req1, ReqIter req2) {
      if(req1->arrive <= req2->arrive)
        return req1;
      return req2;
    },

    // FRFCFS
    [this](ReqIter req1, ReqIter req2) {
      bool ready1 = this->ctrl->is_ready(req1);
      bool ready2 = this->ctrl->is_ready(req2);

      if(ready1 ^ ready2) {
        if(ready1)
          return req1;
        return req2;
      }

      if(req1->arrive <= req2->arrive)
        return req1;
      return req2;
    },

    // FRFCFS_CAP
    [this](ReqIter req1, ReqIter req2) {
      bool ready1 = this->ctrl->is_ready(req1);
      bool ready2 = this->ctrl->is_ready(req2);

      ready1 = ready1 &&
               (this->ctrl->rowtable->get_hits(req1->addr_vec) <= this->cap);
      ready2 = ready2 &&
               (this->ctrl->rowtable->get_hits(req2->addr_vec) <= this->cap);

      if(ready1 ^ ready2) {
        if(ready1)
          return req1;
        return req2;
      }

      if(req1->arrive <= req2->arrive)
        return req1;
      return req2;
    },
    // FRFCFS_PriorHit
    [this](ReqIter req1, ReqIter req2) {
      bool ready1 = this->ctrl->is_ready(req1) && this->ctrl->is_row_hit(req1);
      bool ready2 = this->ctrl->is_ready(req2) && this->ctrl->is_row_hit(req2);

      if(ready1 ^ ready2) {
        if(ready1)
          return req1;
        return req2;
      }

      if(req1->arrive <= req2->arrive)
        return req1;
      return req2;
    }};
};


template <typename T>
class RowPolicy {
 public:
  Controller<T>* ctrl;

  enum class Type {
    Closed,
    ClosedAP,
    Opened,
    Timeout,
    MAX
  } type = Type::Opened;

  int timeout = 50;

  RowPolicy(Controller<T>* ctrl) : ctrl(ctrl) {}

  vector<int> get_victim(typename T::Command cmd) {
    return policy[int(type)](cmd);
  }

 private:
  function<vector<int>(typename T::Command)> policy[int(Type::MAX)] = {
    // Closed
    [this](typename T::Command cmd) -> vector<int> {
      for(auto& kv : this->ctrl->rowtable->table) {
        if(!this->ctrl->is_ready(cmd, kv.first))
          continue;
        return kv.first;
      }
      return vector<int>();
    },

    // ClosedAP
    [this](typename T::Command cmd) -> vector<int> {
      for(auto& kv : this->ctrl->rowtable->table) {
        if(!this->ctrl->is_ready(cmd, kv.first))
          continue;
        return kv.first;
      }
      return vector<int>();
    },

    // Opened
    [this](typename T::Command cmd) { return vector<int>(); },

    // Timeout
    [this](typename T::Command cmd) -> vector<int> {
      for(auto& kv : this->ctrl->rowtable->table) {
        auto& entry = kv.second;
        if(this->ctrl->clk - entry.timestamp < timeout)
          continue;
        if(!this->ctrl->is_ready(cmd, kv.first))
          continue;
        return kv.first;
      }
      return vector<int>();
    }};
};

struct ReuseDistance {
  bool valid;
  int  column_reuse_distance;
  int  row_reuse_distance;
};

template <typename T>
class RowTable {
 public:
  Controller<T>* ctrl;

  struct Entry {
    int  row;
    int  hits;
    long timestamp;
  };

  map<vector<int>, Entry> table;

  RowTable(Controller<T>* ctrl, bool track_col_reuse_distance,
           bool track_row_reuse_distance) :
      ctrl(ctrl),
      track_col_reuse_distance(track_col_reuse_distance),
      track_row_reuse_distance(track_row_reuse_distance),
      row_to_timestamp_to_col(0, vector_hash),
      row_to_col_to_timestamp(0, vector_hash),
      bank_to_timestamp_to_row(0, vector_hash),
      bank_to_row_to_timestamp(0, vector_hash) {}

  ReuseDistance update(typename T::Command cmd, const vector<int>& addr_vec,
                       long clk, bool demand_req, bool is_first_command) {
    auto        begin = addr_vec.begin();
    auto        end   = begin + int(T::Level::Row);
    vector<int> rowgroup(begin, end);  // bank or subarray
    int         row = *end;

    T*            spec = ctrl->channel->spec;
    ReuseDistance rd   = {false, -1, -1};

    if(spec->is_opening(cmd))
      table.insert({rowgroup, {row, 0, clk}});

    if(spec->is_accessing(cmd)) {
      // we are accessing a row -- update its entry
      auto match = table.find(rowgroup);
      assert(match != table.end());
      assert(match->second.row == row);
      match->second.hits++;
      match->second.timestamp = clk;
      if(track_col_reuse_distance || track_row_reuse_distance) {
        rd = update_reuse_distance(addr_vec, clk);
      }
    } /* accessing */

    if(spec->is_closing(cmd)) {
      // we are closing one or more rows -- remove their entries
      int n_rm = 0;
      int scope;
      if(spec->is_accessing(cmd))
        scope = int(T::Level::Row) - 1;  // special condition for RDA and WRA
      else
        scope = int(spec->scope[int(cmd)]);

      for(auto it = table.begin(); it != table.end();) {
        if(equal(begin, begin + scope + 1, it->first.begin())) {
          n_rm++;
          it = table.erase(it);
        } else
          it++;
      }

      assert(n_rm > 0);
    } /* closing */

    assert((spec->is_accessing(cmd) && (track_col_reuse_distance ||
                                        track_row_reuse_distance)) == rd.valid);
    return rd;
  }

  int get_hits(const vector<int>& addr_vec, const bool to_opened_row = false) {
    auto begin = addr_vec.begin();
    auto end   = begin + int(T::Level::Row);

    vector<int> rowgroup(begin, end);
    int         row = *end;

    auto itr = table.find(rowgroup);
    if(itr == table.end())
      return 0;

    if(!to_opened_row && (itr->second.row != row))
      return 0;

    return itr->second.hits;
  }

  int get_open_row(const vector<int>& addr_vec) {
    auto begin = addr_vec.begin();
    auto end   = begin + int(T::Level::Row);

    vector<int> rowgroup(begin, end);

    auto itr = table.find(rowgroup);
    if(itr == table.end())
      return -1;

    return itr->second.row;
  }

 private:
  std::function<size_t(const vector<int>& v)> vector_hash =
    [this](const vector<int>& v) -> size_t {
    int*   sz   = ctrl->channel->spec->org_entry.count;
    size_t hash = 0;

    unsigned int lev;
    for(lev = 1; lev < v.size(); lev++) {
      hash += v[lev - 1];
      hash <<= ((sizeof(sz[lev]) * __CHAR_BIT__) - __builtin_clz(sz[lev] - 1));
    }
    hash += v[lev - 1];

    return hash;
  };

  bool track_col_reuse_distance = false;
  bool track_row_reuse_distance = false;
  unordered_map<vector<int>, map<long, int>, decltype(vector_hash)>
    row_to_timestamp_to_col;
  unordered_map<vector<int>, unordered_map<int, long>, decltype(vector_hash)>
    row_to_col_to_timestamp;
  unordered_map<vector<int>, map<long, int>, decltype(vector_hash)>
    bank_to_timestamp_to_row;
  unordered_map<vector<int>, unordered_map<int, long>, decltype(vector_hash)>
    bank_to_row_to_timestamp;

  ReuseDistance update_reuse_distance(const vector<int>& addr_vec, long clk) {
    auto        begin   = addr_vec.begin();
    auto        row_itr = begin + int(T::Level::Row);
    vector<int> bank_vec(begin, row_itr);
    int         row     = *row_itr;
    auto        col_itr = begin + int(T::Level::Column);
    vector<int> row_vec(begin, col_itr);
    int         col = *col_itr;

    ReuseDistance rd = {true, -1, -1};

    if(track_col_reuse_distance) {
      if(0 == row_to_timestamp_to_col.count(row_vec)) {
        row_to_timestamp_to_col[row_vec] = map<long, int>();
        row_to_col_to_timestamp[row_vec] = unordered_map<int, long>();
      }

      size_t count = row_to_col_to_timestamp.at(row_vec).count(col);
      if(count) {
        long last_clk = row_to_col_to_timestamp.at(row_vec).at(col);
        std::map<long int, int>::const_iterator itr =
          row_to_timestamp_to_col.at(row_vec).find(last_clk);
        assert(itr != row_to_timestamp_to_col.at(row_vec).cend());
        int reuse_distance = std::distance(
          itr, row_to_timestamp_to_col.at(row_vec).cend());
        assert(reuse_distance >= 1);
        reuse_distance -= 1;
        rd.column_reuse_distance = reuse_distance;
        row_to_timestamp_to_col.at(row_vec).erase(itr);
      }
      row_to_timestamp_to_col[row_vec][clk] = col;
      row_to_col_to_timestamp[row_vec][col] = clk;
    }

    if(track_row_reuse_distance) {
      if(0 == bank_to_timestamp_to_row.count(bank_vec)) {
        bank_to_timestamp_to_row[bank_vec] = map<long, int>();
        bank_to_row_to_timestamp[bank_vec] = unordered_map<int, long>();
      }

      size_t count = bank_to_row_to_timestamp.at(bank_vec).count(row);
      if(count) {
        long last_clk = bank_to_row_to_timestamp.at(bank_vec).at(row);
        std::map<long int, int>::const_iterator itr =
          bank_to_timestamp_to_row.at(bank_vec).find(last_clk);
        assert(itr != bank_to_timestamp_to_row.at(bank_vec).cend());
        int reuse_distance = std::distance(
          itr, bank_to_timestamp_to_row.at(bank_vec).cend());
        assert(reuse_distance >= 1);
        reuse_distance -= 1;
        rd.row_reuse_distance = reuse_distance;
        bank_to_timestamp_to_row.at(bank_vec).erase(itr);
      }
      bank_to_timestamp_to_row[bank_vec][clk] = row;
      bank_to_row_to_timestamp[bank_vec][row] = clk;
    }

    return rd;
  }
};

} /*namespace ramulator*/

#endif /*__SCHEDULER_H*/
