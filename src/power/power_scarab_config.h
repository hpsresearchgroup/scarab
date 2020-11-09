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
 * File         : power_scarab_config.h
 * Author       : HPS Research Group
 * Date         : 02/07/2019
 * Description  : Print the XML file that is input into McPat
 ***************************************************************************************/

#ifndef __POWER_REGISTER_UNIT_H__
#define __POWER_REGISTER_UNIT_H__

#ifdef __cplusplus
extern "C" {
#endif

#define DEBUG(proc_id, args...) _DEBUGU(proc_id, DEBUG_POWER_UTILS, ##args)

/**************************************************************************************
 * Global Types
 **************************************************************************************/

/**************************************************************************************
 * Global Variables
 **************************************************************************************/

/**************************************************************************************
 * Global Prototypes
 **************************************************************************************/

void power_print_mcpat_xml_infile();
void power_print_cacti_cfg_infile();

#ifdef __cplusplus
}
#endif

#endif
