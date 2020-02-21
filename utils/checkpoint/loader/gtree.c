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

#include "gtree.h"
#include <stdlib.h>

struct gtree_node {
  void*               data;
  struct gtree_node** children; /* an array of pointers */
  unsigned int        capacity;
  unsigned int        size;
};

struct gtree_node* gtree_add_child(struct gtree_node* parent, void* data) {
  struct gtree_node* node = (struct gtree_node*)calloc(
    1, sizeof(struct gtree_node));
  node->data = data;
  if(parent) {
    if(!parent->children) {
      const int MIN_CAPACITY = 4;
      parent->children       = (struct gtree_node**)calloc(
        1, MIN_CAPACITY * sizeof(struct gtree_node*));
      parent->capacity = MIN_CAPACITY;
    }
    if(parent->size == parent->capacity) {
      parent->capacity = 2 * parent->capacity;
      parent->children = (struct gtree_node**)realloc(
        parent->children, parent->capacity * sizeof(struct gtree_node*));
    }
    parent->children[parent->size] = node;
    parent->size++;
  }
  return node;
}

struct gtree_node** gtree_children(struct gtree_node* parent) {
  return parent->children;
}

unsigned int gtree_num_children(struct gtree_node* parent) {
  return parent->size;
}

void* gtree_data(struct gtree_node* node) {
  return node->data;
}
