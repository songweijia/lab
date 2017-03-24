#include <iostream>
#include <stdlib.h>
#include <string.h>
#include "Persistent.h"

using namespace ns_persistent;

class X {
public:
  int x;
  const std::string to_string(){
    return std::to_string(x);
  }
};


static void printhelp(){
  cout << "usage:" << endl;
  cout << "\tget <version>" << endl;
  cout << "\tset value" << endl;
  cout << "\tlist" << endl;
  cout << "\tvolatile" << endl;
}

Persistent<X> px1;
Persistent<X,ST_MEM> px2; //unused
Persistent<X> pxarr[3]; //unused

int main(int argc,char ** argv){
  //DEFINE_PERSIST_VAR(X,tstx,ST_FILE);

  if(argc <2){
    printhelp();
    return 0;
  }

  try{
    if (strcmp(argv[1],"list") == 0) {
      int64_t nv = px1.getNumOfVersions();
      cout<<"Number of Versions:\t"<<nv<<endl;
      while(nv-- > 0){
        // by lambda
        px1.get(nv,
          [&](X& x) {
            cout<<"["<<nv<<"]\t"<<x.x<<"\t//by lambda"<<endl;
          });
        // by copy
        cout<<"["<<nv<<"]\t"<<px1.get(nv)->x<<"\t//by copy"<<endl;
      }
    } 
    else if (strcmp(argv[1],"get") == 0){
      int64_t nv = atol(argv[2]);
      // by lambda
      px1.get(nv,
        [&](X& x) { 
          cout<<"["<<nv<<"]\t"<<x.x<<"\t//by lambda"<<endl;
        });
      // by copy
      cout<<"["<<nv<<"]\t"<<px1.get(nv)->x<<"\t//by copy"<<endl;
    }
    else if (strcmp(argv[1],"set") == 0) {
      int v = atoi(argv[2]);
      X x;
      x.x = v;
      px1.set(x);
    }
    else if (strcmp(argv[1],"volatile") == 0) {
  /*
      int64_t nv = px2.getNumOfVersions();
      cout<<"Number of Versions:\t"<<nv<<endl;
      while(nv-- > 0){
        // by lambda
        px1.get(nv,
          [&](X& x) {
            cout<<"["<<nv<<"]\t"<<x.x<<"\t//by lambda"<<endl;
          });
        // by copy
        cout<<"["<<nv<<"]\t"<<px1.get(nv)->x<<"\t//by copy"<<endl;
      }
  */
    }
    else {
      cout << "unknown command: " << argv[1] << endl;
      printhelp();
    }
  }catch (uint64_t exp){
    cerr<<"Exception captured:0x"<<std::hex<<exp<<endl;
  }

  return 0;
}
