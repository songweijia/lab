#include <iostream>
#include "PersistVar.h"

using namespace ns_persistent;

class X {
public:
  int x;
  virtual ~X(){
    cerr<<"X's destructor"<<endl;
  };
};

DEFINE_PERSIST_VAR(X,tstx,ST_FILE);

int main(int argc,char ** argv){
  cout<<"[PersistVar library tester]"<<endl;
  return 0;
}
