#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <semaphore.h>
#include <pthread.h>
#include <errno.h>
/*
  Shape of a 8x8 World: 
  The actual space in the world is 6x6
  # # # # # # # #
 #              #
 #              #
 #              #
 #              #
 #              #
 #              #
 # # # # # # # #
 */

// (x,y) are coordination start from 1
#define STATE(w,x,y,ox,shadow) \
  (w[((y)<<(ox))+(x)] & (shadow))

#define L_STATE(w,x,y,ox) \
  (w[((y)<<(ox))+(x)])

#ifdef DEBUG
void dumpWorld(int *world, int lower_slot, int ncol_with_margin, int nrow_with_margin) {
  int i,j;

  printf("START>\n");
  int ofst = (lower_slot)?0:(ncol_with_margin*nrow_with_margin);
  for(i=0;i<nrow_with_margin;i++) {
    for(j=0;j<ncol_with_margin;j++) {
      if(world[i*ncol_with_margin+ofst+j])
        printf("* ");
      else
        printf("  ");
    }
    printf("\n");
  }
  printf("<END\n");
}
#endif

#ifdef RAND_TEST
int* convertIndexToMarginWorld(int *world,int ncol,int nrow){
  for(int i=0;i<ncol*nrow;i++){
    int x = world[i]%nrow;
    int y = world[i]/nrow;
    world[i] = y*(nrow+2) + x + 1;
  }
  return world;
}

int* createRandTable(int ncol,int nrow){
  int nworld = ncol*nrow;
  int* random_map = (int*)malloc(nworld*sizeof(int));
  char* char_map = (char*)malloc(nworld);
  if(random_map==NULL || char_map==NULL){
    fprintf(stderr,"ERROR allocating memory.errno=%d\n",errno);
    return NULL;
  }

  memset(char_map,0,nworld);
  for(int i=0;i<nworld;i++) {
    int next = rand()%(nworld);
    while(char_map[next]!=0){
      next = (next+1)%nworld;
    }
    random_map[i] = next;
    char_map[next] = 1;
  }

  free(char_map);
  printf("randomize...done.\n");
// #ifdef DEBUG
  for(int i=0;i<nworld;i++) {
    fprintf(stderr,"%d-%d\n",i,random_map[i]);
  }
// #endif
  return convertIndexToMarginWorld(random_map,ncol,nrow);
}
#endif//RAND_TEST

void touchWorld(int *world, int ncol_with_margin, int nrow_with_margin){
  volatile int v;
  int space = 2*ncol_with_margin*nrow_with_margin;
  while(--space) {
    v = world[space];
  }
}

#ifdef RAND_TEST
int step(int *w1,int *w2, int order_of_ncol,int nrow,int * rand_map) {
#else
int step(int *w1,int *w2, int order_of_ncol,int nrow) {
#endif
    // initialize l/c/r cols.
    //
    //  1 2 3
    //  4 5 6
    //  7 8 9
    //  | | \
    //  | \  rcol
    //  |  ccol
    //  lcol
    #define VFF(x,offset) _world_from[(x)+(offset)]
    #define VF1(x) VFF(x,-_ncol-3)
    #define VF2(x) VFF(x,-_ncol-2)
    #define VF3(x) VFF(x,-_ncol-1)
    #define VF4(x) VFF(x,-1)
    #define VF5(x) VFF(x,0)
    #define VF6(x) VFF(x,+1)
    #define VF7(x) VFF(x,_ncol+1)
    #define VF8(x) VFF(x,_ncol+2)
    #define VF9(x) VFF(x,_ncol+3)
    #define VT(x)  _world_to[x]

#ifdef RAND_TEST
  register int * _world_from asm("r15") = w1;
  register int * _world_to asm("r14") = w2;
  register int _oncol asm("r13") = order_of_ncol;
  register int _ncol asm("r12") = (1<<_oncol) - 2;
  register int _nrow asm("r10") = nrow;
  register int _count asm("r8") = _nrow*_ncol;
  register int* _rand_map asm("r7") = rand_map;
  register int _ofst asm("r6") = nrow + 3;

  while(_count --) {
    _ofst = _rand_map[_count];
    register int _sum asm("r5") = 
      VF1(_ofst) + VF2(_ofst) + VF3(_ofst) +
      VF4(_ofst) + VF5(_ofst) + VF6(_ofst) +
      VF7(_ofst) + VF8(_ofst) + VF9(_ofst);
    VT(_ofst) = 0;
    if ((_sum == 3) || ((_sum-VF5(_ofst)) == 3)) {
      VT(_ofst) = 1;
    }
  }

#else//RAND_TEST

#ifdef DEBUG
  int * _world_from = w1;
  int * _world_to = w2;
  int _oncol  = order_of_ncol;
  int _ncol  = (1<<_oncol) - 2;
  int _nrow  = nrow;
  for (int y = 1; y <= _nrow; y++) {
#else
  register int * _world_from asm("r15") = w1;
  register int * _world_to asm("r14") = w2;
  register int _oncol asm("r13") = order_of_ncol;
  register int _ncol asm("r12") = (1<<_oncol) - 2;
  register int _nrow asm("r10") = nrow;
  for (register int y asm("r9") = 1; y <= _nrow; y++) {
#endif

#ifdef DEBUG
    int tmp = (y<<_oncol)+1;
    int lcol = VF1(tmp) + VF4(tmp) + VF7(tmp);
    int ccol = VF2(tmp) + VF5(tmp) + VF8(tmp);
    int rcol = VF3(tmp) + VF6(tmp) + VF9(tmp);
    int sum = lcol + ccol + rcol;
#else
    register int tmp asm("r1") = (y<<_oncol)+1;
    register int lcol asm("r8") = VF1(tmp) + VF4(tmp) + VF7(tmp);
    register int ccol asm("r7") = VF2(tmp) + VF5(tmp) + VF8(tmp);
    register int rcol asm("r6") = VF3(tmp) + VF6(tmp) + VF9(tmp);
    register int sum asm("r5") = lcol + ccol + rcol;
#endif
    VT(tmp) = 0;
    if ((sum == 3) || ((sum-VF5(tmp)) == 3)) {
      VT(tmp) = 1;
    }
#ifdef DEBUG
    for(int x = 2; x <= _ncol; x++) { // x
#else    
    for(register int x asm("r4") = 2; x <= _ncol; x++) { // x
#endif
      tmp ++;
      sum -= lcol;
      lcol = ccol;
      ccol = rcol;
      rcol = VF3(tmp) + VF6(tmp) + VF9(tmp);
      sum += rcol;

      VT(tmp) = 0;
      if ((sum == 3) || ((sum-VF5(tmp)) == 3)) {
        VT(tmp) = 1;
      }
    }
  }
#endif//RAND_TEST
}

int initialize(int **world, int order_of_ncol, int nrow) {
  long ncol_with_margin = 1<<order_of_ncol;
  long world_space_with_margin = ncol_with_margin * (nrow + 2);
//  *world = (int*)malloc(2*world_space_with_margin*sizeof(int));
  if (posix_memalign((void **)world, 4096, 2*world_space_with_margin*sizeof(int))!=0) {
    fprintf(stderr, "cannot allocate space for the world:error=%d.\n",errno);
    return errno;
  }

  time_t t;
  time(&t);
  srand(t);

  //initialize
  memset(*world,0,(world_space_with_margin<<1)*sizeof(int));
/*
#ifdef DEBUG
  (*world)[ncol_with_margin + 1] = 1;
  (*world)[ncol_with_margin + 2] = 1;
  // (*world)[2*ncol_with_margin + 3] = 1;
  (*world + world_space_with_margin)[ncol_with_margin + 1] = 1;
  (*world + world_space_with_margin)[ncol_with_margin + 2] = 1;
  //(*world + world_space_with_margin)[2*ncol_with_margin + 3] = 1;
#else
*/
  for(int i=1;i<=(nrow);i++)
  for(int j=1;j<(ncol_with_margin-1);j++) {
    (*world)[(i<<order_of_ncol) + j] = (rand()%3+1)%2;
    (*world + world_space_with_margin)[(i<<order_of_ncol) + j] = (rand()%3+1)%2;
  }
  //warm up CPU
  volatile int tmp[4];
  register long counter = 30000000l;
  while(counter--!=0) {
    tmp[0] = (*world)[counter%(world_space_with_margin+0)];
    tmp[1] = (*world)[counter%(world_space_with_margin+1)];
    tmp[2] = (*world)[counter%(world_space_with_margin+2)];
    tmp[3] = (*world)[counter%(world_space_with_margin+3)];
  }
/*
  #endif
*/
  return 0;
}

int destroy(int **world){
  if(*world != NULL){
    free(*world);
    *world = NULL;
  }
  return 0;
}

//Thread parameters
typedef struct {
  int *w1, *w2;
  int order_of_ncol;
  int nrow;
  int num_of_generations;
  sem_t s1; 
  sem_t *ps2;
} ThreadParams;

//Thread
void * thread_routine(void * param) {
  ThreadParams* tp = (ThreadParams*)param;

#ifdef RAND_TEST
  int *rand_map = createRandTable((1<<tp->order_of_ncol)-2,tp->nrow);
#endif

  while(tp->num_of_generations -- ){
    int direction = (tp->num_of_generations%2);
    // 1 - waiting on semaphore
#ifdef DEBUG
      printf("Thread waiting for sem@%p\n",&tp->s1);
#endif
#ifndef SKIP_SYNC
    if(sem_wait(&tp->s1)){
      printf("error wait for sempahore.\n");
      return NULL;
    }
#endif
    // 2 - do the work:
#ifdef RAND_TEST
    step(direction?tp->w1:tp->w2,direction?tp->w2:tp->w1,tp->order_of_ncol,tp->nrow,rand_map);
#else
    step(direction?tp->w1:tp->w2,direction?tp->w2:tp->w1,tp->order_of_ncol,tp->nrow);
#endif
    // 3 - post the direction
#ifdef DEBUG
      printf("Thread posting to sem@%p\n",tp->ps2);
#endif
#ifndef SKIP_SYNC
    if(sem_post(tp->ps2)){
      printf("error post to semaphore.\n");
      return NULL;
    }
#endif
  };

#ifdef RAND_TEST
  free(rand_map);
#endif

  return NULL;
}

//semaphores.
sem_t sem2;

int main(int argc, char **argv) {
  if(argc < 4) {
    printf("USAGE: %s <order of number of cols> <number of rows> <number of thread> [Generation]\n",argv[0]);
    return -1;
  }

  int order_of_ncol = atoi(argv[1]);
  int nrow = atoi(argv[2]);
  int num_of_threads = atoi(argv[3]);
  int nrow_thread = (nrow+num_of_threads-1)/num_of_threads;
  int nrow_last_thread = (nrow%nrow_thread)?(nrow%nrow_thread):nrow_thread;
  int num_of_generations;
  if (argc >=5)
    num_of_generations = atoi(argv[4]);
  else
    num_of_generations = (1L<<30) / ((1L<<order_of_ncol)*(nrow+2)) * num_of_threads; // do ~1GB cells by default.
  int gcount = num_of_generations;
  int *world = NULL;

  if(sem_init(&sem2,0,0)) {
    printf("failed to initialize semaphore,errno=%d\n",errno);
    return -1;
  }

  // initialize world
  if(initialize(&world,order_of_ncol,nrow)!=0) {
    return -1;
  }

  // start threads.
  ThreadParams *tps = (ThreadParams*)malloc(sizeof(ThreadParams)*num_of_threads);
  if(tps==NULL){
    fprintf(stderr,"Fail to allocate memory for ThreadParameters.\n");
    return -1;
  }
  pthread_t *ths = (pthread_t*)malloc(sizeof(pthread_t)*num_of_threads);
  if(ths==NULL){
    fprintf(stderr,"Fail to allocate memory for thread identifiers.\n");
    return -1;
  }

  for(int i=0;i<num_of_threads;i++){
    tps[i].w1 = world + (1<<order_of_ncol)*(nrow_thread)*i;
    tps[i].w2 = tps[i].w1 + (1<<order_of_ncol)*(nrow+2);
    tps[i].order_of_ncol = order_of_ncol;
    tps[i].nrow = nrow_thread;
    tps[i].num_of_generations = num_of_generations;
    if(sem_init(&tps[i].s1,0,0)){
      printf("failed to initialize semaphore,errno=%d\n",errno);
      return -1;
    }
    tps[i].ps2 = &sem2;
#ifdef DEBUG
    printf("tps[%d].w1=%p.\n",i,tps[i].w1);
    printf("tps[%d].w2=%p.\n",i,tps[i].w2);
    printf("tps[%d].order_of_ncol=%d.\n",i,tps[i].order_of_ncol);
    printf("tps[%d].nrow=%d.\n",i,tps[i].nrow);
    printf("tps[%d].num_of_generations=%d.\n",i,tps[i].num_of_generations);
#endif
  }
  tps[num_of_threads-1].nrow = nrow_last_thread;

  for(int i=0;i<num_of_threads;i++) {
    if(pthread_create(&ths[i],NULL,thread_routine,&tps[i])) {
      fprintf(stderr,"Failed to create thread, errno=%d.\n",errno);
      return -1;
    }
  }
  
  struct timespec ts,te;
  clock_gettime(CLOCK_REALTIME,&ts);

  while(gcount -- ) {
#ifdef DEBUG
    dumpWorld(world, gcount%2, 1<<order_of_ncol,nrow+2);
#endif
    // Start threads.
    int i;
    for(i=0;i<num_of_threads;i++){
#ifdef DEBUG
      printf("posting to sem@%p\n",&tps[i].s1);
#endif
#ifndef SKIP_SYNC
      if(sem_post(&tps[i].s1)) {
        fprintf(stderr,"Failed to post semaphore.errno=%d\n",errno);
        return -1;
      }
#endif
    }
    // wait for results.
    for(i=0;i<num_of_threads;i++){
#ifdef DEBUG
      printf("waiting for sem@%p\n",&sem2);
#endif
#ifndef SKIP_SYNC
      if(sem_wait(&sem2)) {
        fprintf(stderr,"Failed to wait semaphore.errno=%d\n",errno);
        return -1;
      }
#endif
    }
  }

#ifdef SKIP_SYNC
  for(int i=0;i<num_of_threads;i++){
    void *ret;
    pthread_join(ths[i],&ret);
  }
#endif

  clock_gettime(CLOCK_REALTIME,&te);
#ifndef DEBUG
  touchWorld(world,1<<order_of_ncol,nrow+2);
#endif
  ssize_t tot_ns = (te.tv_sec-ts.tv_sec)*1000000000 + (te.tv_nsec-ts.tv_nsec);
  ssize_t ncell = ((1L<<order_of_ncol)-2)*nrow;
  ssize_t nworld = (1L<<(order_of_ncol))*(nrow + 2);
  printf("%.3f second\n",(double)tot_ns/1000000000);
  printf("%ld KB working set;\t throughput = %.3fM cell generation per sec.\n", nworld>>7, (double)ncell*num_of_generations/tot_ns*1000);

  for(int i=0;i<num_of_threads;i++){
    sem_destroy(&tps[i].s1);
  }
  free(tps);
  free(ths);
  sem_destroy(&sem2);
  destroy(&world);
}
