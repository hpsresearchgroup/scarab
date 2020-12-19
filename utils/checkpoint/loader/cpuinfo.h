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

/*cpuinfo.h
 * 2/25/20
 */

#ifndef __CPUINFO_H__
#define __CPUINFO_H__

#include <cassert>
#include <string>

inline std::string getCPUflags() {
  const int BUF_MAX = 2048;
  char      buf[2048];

  FILE* fp;
  fp = popen("cat /proc/cpuinfo | grep flags | head -n 1", "r");
  assert(NULL != fp);

  [[maybe_unused]] auto fgets_ret1 = fgets(buf, BUF_MAX, fp);
  assert(fgets_ret1 == buf);  // read should succeed

  [[maybe_unused]] auto fgets_ret2 = fgets(buf, BUF_MAX, fp);
  assert(NULL == fgets_ret2);  // should only be a single line

  [[maybe_unused]] int status = pclose(fp);
  assert(-1 != status);

  std::string full_line(buf);
  std::string separator        = std::string(": ");
  std::size_t pos_of_separator = full_line.find(separator);
  assert(std::string::npos != pos_of_separator);
  std::size_t pos_of_endline = full_line.find("\n");
  assert(std::string::npos != pos_of_endline);
  std::size_t start_pos  = (pos_of_separator + separator.size());
  std::string just_flags = full_line.substr(start_pos,
                                            pos_of_endline - start_pos);

  assert(!just_flags.empty());
  return just_flags;
}

#endif
