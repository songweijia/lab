#include <iostream>
#include <stdlib.h>
#include "PersistVar.h"

using namespace ns_persistent;

class X {
public:
  int x;
  static void* serialize(const X &x, uint64_t *psize){
    int *buf = (int*)malloc(4);
    *psize = 4;
    *buf = x.x;
    return (void*)buf;
  };
  static X * deserialize(const void *pdata){
    X *x = new X();
    x->x = *(int*)pdata;
    return x;
  }
};

DEFINE_PERSIST_VAR(X,tstx,ST_FILE);

int main(int argc,char ** argv){
  cout<<"[PersistVar library tester]"<<endl;
  return 0;
}
