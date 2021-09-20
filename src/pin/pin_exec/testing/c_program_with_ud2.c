#include <setjmp.h>
#include <signal.h>

jmp_buf buffer;

void handle_sig(int sig);
void handle_sig(int sig) {
  longjmp(buffer, 1);
}

int main() {
  signal(SIGILL, handle_sig);
  int x = setjmp(buffer);
  if(x == 0) {
    __asm__("ud2\n");
  }
  return 0;
}
