#include <iostream>
#include <stdlib.h>
#include <string.h>
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


static void printhelp(){
  cout << "usage:" << endl;
  cout << "\tget <version>" << endl;
  cout << "\tset value" << endl;
  cout << "\tlist" << endl;
}


PersistVar<X> px1;
PersistVar<X> px2; //unused
PersistVar<X> pxarr[3]; //unused

int main(int argc,char ** argv){
  //DEFINE_PERSIST_VAR(X,tstx,ST_FILE);


  if(argc <2){
    printhelp();
    return 0;
  }

  if (strcmp(argv[1],"list") == 0) {
    uint64_t nv = px1.getNumOfVersions();
    cout<<"Number of Versions:\t"<<nv<<endl;
    while(nv-- > 0)
      cout<<"["<<nv<<"]\t"<<px1.get(nv)->x<<endl;
  } 
  else if (strcmp(argv[1],"get") == 0){
    int64_t nv = atol(argv[2]);
    cout<<"["<<nv<<"]\t"<<px1.get(nv)->x<<endl;
  }
  else if (strcmp(argv[1],"set") == 0){
    int v = atoi(argv[2]);
    X x;
    x.x = v;
    px1.set(x);
  }
  else {
    cout << "unknown command: " << argv[1] << endl;
    printhelp();
  }

  return 0;
}
