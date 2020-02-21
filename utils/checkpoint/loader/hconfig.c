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

#include "hconfig.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gtree.h"

static enum hconfig_error_t error = HCONFIG_OK;

typedef struct gtree_node hconfig_t;

char* buf          = 0;
int   buf_capacity = 0;
int   buf_size     = 0;

static int  datum_read(FILE* file, char** datum, int* offset);
static void datum_append(char** datum, char c);

static int datum_read(FILE* file, char** datum, int* offset) {
  int c = 0;
  enum { BEFORE_DATUM, IN_DATUM, ESCAPE_IN_DATUM } state;
  *datum   = buf;
  buf_size = 0;
  state    = BEFORE_DATUM;
  do {
    c = fgetc(file);
    if(c == EOF) {        /* stream error */
      if(buf_size == 0) { /* no datum to return */
        return EOF;
      } else {
        if(state == ESCAPE_IN_DATUM)
          datum_append(datum, '\\');
        datum_append(datum, 0);
        if(buf[0] == '"')
          (*datum)++; /* point after the quote */
        return buf[0] == '"' ?
                 buf_size - 2 :
                 buf_size - 1; /* no end quote, not counting null-terminator */
      }
    }
    switch(state) {
      case BEFORE_DATUM:
        if(c == '\n') {
          (*offset) = 0;
        } else if(c == ' ') {
          (*offset)++;
        } else {
          state = IN_DATUM;
          datum_append(datum, c);
        }
        break;
      case ESCAPE_IN_DATUM:
        state = IN_DATUM;
        if(c == '"') {
          datum_append(datum, '"');
          break;
        } else {
          datum_append(datum, '\\');
          /* WARNING: falling through on purpose!!! no break!!! */
        }
      case IN_DATUM:
        if(c == '\\') {
          state = ESCAPE_IN_DATUM;
        } else if((c == '"' && buf[0] == '"') ||
                  ((c == ' ' || c == '\n') && buf[0] != '"')) {
          /* done */
          datum_append(datum, 0);
          if(buf[0] == '"')
            (*datum)++; /* point after the quote */
          else
            ungetc(c, file); /* put back the space */
          return buf[0] == '"' ?
                   buf_size - 2 :
                   buf_size -
                     1; /* no end quote, ignore null-terminator in count */
        } else {
          datum_append(datum, c);
        }
        break;
    }
  } while(1);
}

static void datum_append(char** datum, char c) {
  if(!buf) {
    buf_capacity = 4;
    buf          = malloc(buf_capacity);
    *datum       = buf;
  }
  if(buf_size >= buf_capacity) {
    buf_capacity = 2 * buf_size;
    buf          = realloc(buf, buf_capacity);
    *datum       = buf;
  }
  buf[buf_size] = c;
  buf_size++;
}

static void hconfig_load_r(FILE* file, char** datum, int* offset,
                           struct gtree_node* parent, int* end,
                           int parent_offset) {
  struct gtree_node* node = 0;
  int                rc;
  *datum = 0;
  rc     = datum_read(file, datum, offset);
  if(rc != EOF) {
    while(!(*end) && *offset > parent_offset) {
      node = gtree_add_child(parent, (void*)strdup(*datum));
      hconfig_load_r(file, datum, offset, node, end, *offset);
    }
  } else {
    *end = 1;
  }
}

const struct hconfig_t* hconfig_load(FILE* file) {
  char*              datum  = 0;
  int                offset = 0;
  int                end    = 0;
  struct gtree_node* root   = gtree_add_child(0, "");
  hconfig_load_r(file, &datum, &offset, root, &end, -1);
  return (const struct hconfig_t*)root;
}

unsigned int hconfig_num_children(const struct hconfig_t* config) {
  return gtree_num_children((struct gtree_node*)config);
}

const struct hconfig_t** hconfig_children(const struct hconfig_t* config) {
  return (const struct hconfig_t**)gtree_children((struct gtree_node*)config);
}

const struct hconfig_t* hconfig_descend(const struct hconfig_t* config,
                                        const char*             name) {
  int                     i;
  const struct hconfig_t* ret = 0;
  for(i = 0; i < hconfig_num_children(config); ++i) {
    const struct hconfig_t* child = hconfig_children(config)[i];
    if(!strcmp(hconfig_name(child), name)) {
      if(ret) {
        error = HCONFIG_MULTIPLE_NAMES;
        return 0;
      } else {
        ret = child;
      }
    }
  }
  if(!ret) {
    error = HCONFIG_NAME_NOT_FOUND;
  } else {
    error = HCONFIG_OK;
  }
  return ret;
}

const char* hconfig_name(const struct hconfig_t* config) {
  return (const char*)gtree_data((struct gtree_node*)config);
}

const char* hconfig_value(const struct hconfig_t* config, const char* name) {
  const struct hconfig_t* child = hconfig_descend(config, name);
  if(child) {
    if(hconfig_num_children(child) == 0) {
      error = HCONFIG_NAME_NOT_FOUND;
      return 0;
    }
    if(hconfig_num_children(child) > 1) {
      error = HCONFIG_MULTIPLE_NAMES;
      return 0;
    }
    error = HCONFIG_OK;
    return hconfig_name(hconfig_children(child)[0]);
  } else
    return 0; /* error already contains the error */
}

enum hconfig_error_t hconfig_error(void) {
  return error;
}
