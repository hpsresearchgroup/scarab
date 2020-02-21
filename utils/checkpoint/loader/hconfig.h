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

#ifndef HCONFIG_H
#define HCONFIG_H

#include <stdio.h>

/** \file

    This library provides support for loading and accessing
    hierarchical configuration files. The provided interface is a
    tree: the root node represents an entire configuration file and
    its descendants represent various subconfigurations.

    The configuration file looks like this:
    <pre>
    subconfigA
      value1 42
      value2 57
      subsubconfig1
        "letter A"
        "letter B"
    "subconfigB"
       "this is a quote: \", see?"
    </pre>

    Hopefully the format is obvious from the format above. Quotes in
    quoted strings need to be escaped.
*/

/** Type representing a configuration hierarchy */
struct hconfig_t;

/** Errors reported */
enum hconfig_error_t {
  HCONFIG_OK,             /**< No error */
  HCONFIG_NAME_NOT_FOUND, /**< No matching configuration node found */
  HCONFIG_MULTIPLE_NAMES /**< More than one matching configuration node found */
};

/** Load a configuration from \p file */
const struct hconfig_t* hconfig_load(FILE* file);

/** Return the subconfiguration of \p config labeled \p name. Returns
    NULL if either no or multiple such subconfigurations are
    found. The exact error condition is given by hconfig_error(). */
const struct hconfig_t* hconfig_descend(const struct hconfig_t* config,
                                        const char*             name);

/** Return the name of the configuration tree node \p config. */
const char* hconfig_name(const struct hconfig_t* config);

/** This is a shortcut function for the following common configuration case:
    <pre>
    subconfig
       key1  value1
       key2
         value2
       key3  value 3
    </pre>

    Given the configuration tree node for "subconfig" and the key
    string, hconfig_value() will return the value string.

    Returns NULL if there are no or multiple matching keys or if the
    number of children of the matching key node is not one. The exact
    error condition is given by hconfig_error(). */
const char* hconfig_value(const struct hconfig_t* config, const char* name);

/** How many children (subconfigurations) does the configuration tree
    node \p config has. */
unsigned int hconfig_num_children(const struct hconfig_t* config);

/** Returns the array of pointers to children (subconfigurations) of \p config.
 */
const struct hconfig_t** hconfig_children(const struct hconfig_t* config);

/** Returns the error of the last operation that can produce errors
    (not all can). */
enum hconfig_error_t hconfig_error(void);

#endif
