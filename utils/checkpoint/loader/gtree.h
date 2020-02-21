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

#ifndef GTREE_H
#define GTREE_H

/** \file

    An implementation of a "growing tree," a simple general tree data
    structure which allows only one kind of modification: adding
    children to a tree node. */

/** A type representing both a specific node of a tree, and the tree
    for which this node is the root node.*/
struct gtree_node;

/** Add a child containing \p data to the (sub)tree rooted in \p
    parent. If \p parent is NULL, a root node for a new tree is
    created.

    @returns Pointer to the newly created tree node. */
struct gtree_node* gtree_add_child(struct gtree_node* parent, void* data);

/** @returns An array of pointers to children of \p parent. The array
    size is given by gtree_num_children(). */
struct gtree_node** gtree_children(struct gtree_node* parent);

/** @returns Number of children \p parent has. */
unsigned int gtree_num_children(struct gtree_node* parent);

/** @returns Pointer to data associated with \p node. */
void* gtree_data(struct gtree_node* node);

#endif
