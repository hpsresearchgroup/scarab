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

#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

int*    global = 0;
int     temp   = 1;
jmp_buf bufferA, bufferB;

void handle_sig(int sig) {
  printf("Exception\n");
  longjmp(bufferA, 10001);
}

int main() {
  signal(SIGSEGV, handle_sig);
  // signal(SIGILL, handle_sig);

  setbuf(stdout, NULL);
  printf("Here 1\n");
  printf("Here 2\n");
  int k = 0;
  int j = 0;
  k     = setjmp(bufferA);
  printf("%d\n", k);
  __asm__("ud2;");
  // int a = *global;
  printf("Here 3\n");

  return 1;
}
