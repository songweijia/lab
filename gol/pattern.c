#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <string.h>

int main(int argc,char **argv) {
  int *p;
  if(posix_memalign((void**)&p,4096,8<<20)!=0) {
    fprintf(stderr,"posix_memalign error:%d\n",errno);
    return -1;
  }
  memset(p,0,8<<20);

  register int * p1 asm("r15") = p;
  register int * p2 asm("r14") = p + (1<<10);

  for(register int i asm("r13") = 0; i< 100; i++) {
    for(register int y asm("r12") = 1; y< (1<<10) - 1; y++) 
    for(register int x asm("r11") = 1; x< (1<<10) - 1; x++) 
    {
      p1 = p + (x%2)*(1<<10);
      p2 = p + ((x+1)%2)*(1<<10);
      register int idx asm("r10") = (y<<10) + x;
      p2[idx] = p1[idx-((1<<10)+1)] +
                p1[idx-(1<<10)    ] +
                p1[idx-((1<<10)-1)] +
                p1[idx-1          ] +
                p1[idx            ] +
                p1[idx+1          ] +
                p1[idx+((1<<10)-1)] +
                p1[idx+(1<<10)    ] +
                p1[idx+((1<<10)+1)] +
                1;
    }
  }

  printf("result=%d\n",p2[(1<<20)-(1<<10)-2]);

  free(p);
}
