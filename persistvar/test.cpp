#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <spdlog/spdlog.h>
#include <SerializationSupport.hpp>
#include "Persistent.hpp"
#include "HLC.hpp"
#include "util.hpp"

using namespace ns_persistent;
using namespace mutils;

// A test class
class X {
public:
  int x;
  const std::string to_string(){
    return std::to_string(x);
  }
};

// A variable that can change the length of its value
class VariableBytes : public ByteRepresentable{
public:
  std::size_t data_len;
  char buf[MAX_DATA_SIZE];

  VariableBytes () {
    data_len = MAX_DATA_SIZE;
  }

  virtual std::size_t to_bytes(char *v) const {
    memcpy(v,buf,this->data_len);
    return data_len;
  };

  virtual void post_object(const std::function<void (char const * const,std::size_t)>& func) const {
    func(this->buf,this->data_len);
  };

  virtual std::size_t bytes_size() const {
    return this->data_len;
  };

  virtual void ensure_registered(DeserializationManager &dsm) {
    // do nothing, we don't need DSM.
  };

  virtual const char* to_string(){
    return buf;
  };
  
  static std::unique_ptr<VariableBytes> from_bytes(DeserializationManager *dsm, char const * const v) {
    VariableBytes vb;
    std::unique_ptr<VariableBytes> pvb = std::make_unique<VariableBytes>();
    strcpy(pvb->buf,v);
    pvb->data_len = strlen(v)+1;
    return pvb;
  };
};

static void printhelp(){
  cout << "usage:" << endl;
  cout << "\tgetbyidx <index>" << endl;
  cout << "\tgetbyver <version>" << endl;
  cout << "\tgetbytime <timestamp>" << endl;
  cout << "\tset value version" << endl;
  cout << "\ttrimbyidx <index>" << endl;
  cout << "\ttrimbyver <version>" << endl;
  cout << "\ttrimbytime <time>" << endl;
  cout << "\tlist" << endl;
  cout << "\tvolatile" << endl;
  cout << "\thlc" << endl;
  cout << "\teval <file|mem> <datasize> <num>" << endl;
  cout << "NOTICE: test can crash if <datasize> is too large(>8MB).\n"
       << "This is probably due to the stack size is limited. Try \n"
       << "  \"ulimit -s unlimited\"\n"
       << "to remove this limitation."<<endl;
}

Persistent<X> px1;
Persistent<VariableBytes> npx;
//Persistent<X,ST_MEM> px2; 
Volatile<X> px2; 
Persistent<X> pxarr[3]; //unused

template <typename OT, StorageType st=ST_FILE>
void listvar(Persistent<OT,st> &var){
  int64_t nv = var.getNumOfVersions();
  int64_t idx = var.getEarliestIndex();
  cout<<"Number of Versions:\t"<<nv<<endl;
  while (nv--) {
/*
    // by lambda
    var.getByIndex(nv,
      [&](OT& x) {
        cout<<"["<<nv<<"]\t"<<x.to_string()<<"\t//by lambda"<<endl;
      });
*/
    // by copy
    cout<<"["<<idx<<"]\t"<<var.getByIndex(idx)->to_string()<<"\t//by copy"<<endl;
    idx++;
  }
}

static void test_hlc();
template <StorageType st=ST_FILE>
static void eval_write (std::size_t osize, int nops) {
  VariableBytes writeMe;
  Persistent<VariableBytes,st> pvar;
  writeMe.data_len = osize;
  struct timespec ts,te;
  int cnt = nops;
  __int128 ver = (__int128)0l;
  clock_gettime(CLOCK_REALTIME,&ts);
  while(cnt -- > 0) {
    pvar.set(writeMe,ver++);
  }
  pvar.persist();
  clock_gettime(CLOCK_REALTIME,&te);
  long sec = (te.tv_sec - ts.tv_sec);
  long nsec = sec*1000000000 + te.tv_nsec - ts.tv_nsec;
  dbg_warn("nanosecond={}\n",nsec);
  double thp_MBPS = (double)osize*nops/(double)nsec * 1000;
  double lat_us = (double)nsec/nops / 1000;
  cout << "WRITE TEST(st=" << st << ", size=" << osize << " byte, ops=" << nops << ")" << endl;
  cout << "throughput:\t" << thp_MBPS << " MB/s" << endl;
  cout << "latency:\t" << lat_us << " microseconds" << endl;
}

int main(int argc,char ** argv){
  spdlog::set_level(spdlog::level::trace);

  if(argc <2){
    printhelp();
    return 0;
  }

  try{
    if (strcmp(argv[1],"list") == 0) {
      cout<<"Persistent<VariableBytes> npx:"<<endl;
      listvar<VariableBytes>(npx);
      //cout<<"Persistent<X,ST_MEM> px2:"<<endl;
      //listvar<X,ST_MEM>(px2);
    } 
    else if (strcmp(argv[1],"getbyidx") == 0){
      int64_t nv = atol(argv[2]);
      // by lambda
/*
      npx.getByIndex(nv,
        [&](VariableBytes& x) { 
          cout<<"["<<nv<<"]\t"<<x.to_string()<<"\t//by lambda"<<endl;
        });
*/
      // by copy
      cout<<"["<<nv<<"]\t"<<npx.getByIndex(nv)->to_string()<<"\t//by copy"<<endl;
    }
    else if (strcmp(argv[1],"getbyver") == 0){
      __int128 ver = atoi(argv[2]);
/*
      // by lambda
      npx.get(ver,
        [&](VariableBytes& x) { 
          cout<<"["<<(uint64_t)(ver>>64)<<"."<<(uint64_t)ver<<"]\t"<<x.to_string()<<"\t//by lambda"<<endl;
        });
*/
      // by copy
      cout<<"["<<(uint64_t)(ver>>64)<<"."<<(uint64_t)ver<<"]\t"<<npx.get(ver)->to_string()<<"\t//by copy"<<endl;
    }
    else if (strcmp(argv[1],"getbytime") == 0){
      HLC hlc;
      hlc.m_rtc_us = atol(argv[2]);
      hlc.m_logic = 0;
/*
      npx.get(hlc,
        [&](VariableBytes& x) {
          cout<<"[("<<hlc.m_rtc_us<<",0)]\t"<<x.to_string()<<"\t//bylambda"<<endl;
        });
*/
      cout<<"["<<"[("<<hlc.m_rtc_us<<",0)]\t"<<npx.get(hlc)->to_string()<<"\t//by copy"<<endl;
    }
    else if(strcmp(argv[1],"trimbyidx") == 0){
      int64_t nv = atol(argv[2]);
      npx.trim(nv);
      npx.persist();
      cout<<"trim till index "<<nv<<" successfully"<<endl;
    }
    else if(strcmp(argv[1],"trimbyver") == 0){
      __int128 ver = atol(argv[2]);
      npx.trim(ver);
      npx.persist();
      cout<<"trim till ver "<<(int64_t)(ver>>64)<<"."<<(int64_t)ver<<" successfully"<<endl;
    }
    else if(strcmp(argv[1],"trimbytime") == 0){
      HLC hlc;
      hlc.m_rtc_us = atol(argv[2]);
      hlc.m_logic = 0;
      npx.trim(hlc);
      npx.persist();
      cout<<"trim till time "<<hlc.m_rtc_us<<" successfully"<<endl;
    }
    else if (strcmp(argv[1],"set") == 0) {
      char* v = argv[2];
      __int128 ver = (__int128)atoi(argv[3]);
      sprintf((*npx).buf,"%s",v);
      (*npx).data_len = strlen(v)+1;
      npx.version(ver);
      npx.persist();
    }
    else if (strcmp(argv[1],"volatile") == 0) {
      cout<<"loading Persistent<X,ST_MEM> px2"<<endl;
      listvar<X,ST_MEM>(px2);
      __int128 ver = (__int128)0L;
      X x;
      x.x = 1;
      px2.set(x,ver++);
      px2.persist();
      cout<<"after set 1"<<endl;
      listvar<X,ST_MEM>(px2);
      x.x = 10;
      px2.set(x,ver++);
      px2.persist();
      cout<<"after set 10"<<endl;
      listvar<X,ST_MEM>(px2);
      x.x = 100;
      px2.set(x,ver++);
      px2.persist();
      cout<<"after set 100"<<endl;
      listvar<X,ST_MEM>(px2);
    }
    else if (strcmp(argv[1],"hlc") == 0) {
      test_hlc();
    }
    else if (strcmp(argv[1],"eval") == 0) {
      // eval file|mem osize nops
      int osize = atoi(argv[3]);
      int nops = atoi(argv[4]);

      if(strcmp(argv[2],"file") == 0) {
        eval_write<ST_FILE>(osize,nops);
      } else if(strcmp(argv[2],"mem") == 0) {
        eval_write<ST_MEM>(osize,nops);
      } else {
        cout << "unknown storage type:" << argv[2] << endl;
      }
    }
    else {
      cout << "unknown command: " << argv[1] << endl;
      printhelp();
    }
  }catch (unsigned long long exp){
    cerr<<"Exception captured:0x"<<std::hex<<exp<<endl;
    return -1;
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
