#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <spdlog/spdlog.h>
#include "Persistent.hpp"
#include "HLC.hpp"

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
  cout << "\tgetbytime <timestamp>" << endl;
  cout << "\tset value" << endl;
  cout << "\tlist" << endl;
  cout << "\tvolatile" << endl;
  cout << "\thlc"<< endl;
}

Persistent<X> px1;
//Persistent<X,ST_MEM> px2; 
Volatile<X> px2; 
Persistent<X> pxarr[3]; //unused

template <typename OT, StorageType st=ST_FILE>
void listvar(Persistent<OT,st> &var){
  int64_t nv = var.getNumOfVersions();
  cout<<"Number of Versions:\t"<<nv<<endl;
  while(nv-- > 0){
    // by lambda
    var.get(nv,
      [&](OT& x) {
        cout<<"["<<nv<<"]\t"<<x.to_string()<<"\t//by lambda"<<endl;
      });
    // by copy
    cout<<"["<<nv<<"]\t"<<var.get(nv)->to_string()<<"\t//by copy"<<endl;
  }
}

static void test_hlc();
 
int main(int argc,char ** argv){
  spdlog::set_level(spdlog::level::trace);
  //DEFINE_PERSIST_VAR(X,tstx,ST_FILE);

  if(argc <2){
    printhelp();
    return 0;
  }

  try{
    if (strcmp(argv[1],"list") == 0) {
      cout<<"Persistent<X> px1:"<<endl;
      listvar<X>(px1);
      cout<<"Persistent<X,ST_MEM> px2:"<<endl;
      listvar<X,ST_MEM>(px2);
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
    else if (strcmp(argv[1],"getbytime") == 0){
      HLC hlc;
      hlc.m_rtc_us = atol(argv[2]);
      hlc.m_logic = 0;
      px1.get(hlc,
        [&](X& x) {
          cout<<"[("<<hlc.m_rtc_us<<",0)]\t"<<x.x<<"\t//bylambda"<<endl;
        });
    }
    else if (strcmp(argv[1],"set") == 0) {
      int v = atoi(argv[2]);
      X x;
      x.x = v;
      px1.set(x);
    }
    else if (strcmp(argv[1],"volatile") == 0) {
      cout<<"loading Persistent<X,ST_MEM> px2"<<endl;
      listvar<X,ST_MEM>(px2);
      X x;
      x.x = 1;
      px2.set(x);
      cout<<"after set 1"<<endl;
      listvar<X,ST_MEM>(px2);
      x.x = 10;
      px2.set(x);
      cout<<"after set 10"<<endl;
      listvar<X,ST_MEM>(px2);
      x.x = 100;
      px2.set(x);
      cout<<"after set 100"<<endl;
      listvar<X,ST_MEM>(px2);
    }
    else if (strcmp(argv[1],"hlc") == 0) {
      test_hlc();
    }
    else {
      cout << "unknown command: " << argv[1] << endl;
      printhelp();
    }
  }catch (unsigned long long exp){
    cerr<<"Exception captured:0x"<<std::hex<<exp<<endl;
  }

  return 0;
}

static inline void print_hlc(const char * name, const HLC & hlc){
  cout<<"HLC\t"<<name<<"("<<hlc.m_rtc_us<<","<<hlc.m_logic<<")"<<endl;
}

void test_hlc(){
  cout<<"creating 2 HLC: h1 and h2."<<endl;
  HLC h1,h2;
  h1.tick(h2);
  print_hlc("h1",h1);
  print_hlc("h2",h2);

  cout<<"\nh1.tick()\t"<<endl;
  h1.tick();
  print_hlc("h1",h1);
  print_hlc("h2",h2);

  cout<<"\nh2.tick(h1)\t"<<endl;
  h2.tick(h1);
  print_hlc("h1",h1);
  print_hlc("h2",h2);

  cout<<"\ncomparison"<<endl;
  cout<<"h1>h2\t"<<(h1>h2)<<endl;
  cout<<"h1<h2\t"<<(h1<h2)<<endl;
  cout<<"h1>=h2\t"<<(h1>=h2)<<endl;
  cout<<"h1<=h2\t"<<(h1<=h2)<<endl;
  cout<<"h1==h2\t"<<(h1==h2)<<endl;

  cout<<"\nevaluation:h1=h2"<<endl;
  h1 = h2;
  print_hlc("h1",h1);
  print_hlc("h2",h2);
  cout<<"h1>h2\t"<<(h1>h2)<<endl;
  cout<<"h1<h2\t"<<(h1<h2)<<endl;
  cout<<"h1>=h2\t"<<(h1>=h2)<<endl;
  cout<<"h1<=h2\t"<<(h1<=h2)<<endl;
  cout<<"h1==h2\t"<<(h1==h2)<<endl;
}
