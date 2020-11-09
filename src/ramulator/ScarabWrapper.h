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

#ifndef __SCARAB_WRAPPER_H
#define __SCARAB_WRAPPER_H

#include <string>

#include "Config.h"

using namespace std;

namespace ramulator
{

class Request;
class MemoryBase;

class ScarabWrapper 
{
private:
    MemoryBase *mem;
public:
    //double tCK;
    ScarabWrapper(const Config& configs, const unsigned int cacheline, void (* stats_callback)(int, int));
    ~ScarabWrapper();
    void tick();
    bool send(Request req);
    void finish(void);

    int get_chip_width() const;
    int get_chip_size()  const;
    int get_num_chips()  const;
    int get_chip_row_buffer_size() const;
};

} /*namespace ramulator*/

#endif /*__SCARAB_WRAPPER_H*/
