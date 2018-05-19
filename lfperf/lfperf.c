#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <malloc.h>
#include <getopt.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <inttypes.h>
#include <netdb.h>
#include <errno.h>
#include <rdma/fabric.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_rma.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_errno.h>

///////////////////////////////////////////////////////////////////////////////
// 'lfperf' is a tool measuring the RDMA throughput and latency of libfabric //
// over different hardwares                                                  //
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// 1. default definitoins
///////////////////////////////////////////////////////////////////////////////
#define LFPF_MR_KEY (0xC0DEC0DE)
#define LFPF_CTRL_BUF_LEN (64)
#define LFPF_VERSION FI_VERSION(1,5)
#define LFPF_SERVER_PORT (10055)
#define LF_USE_VADDR ((ct->fi->domain_attr->mr_mode) & (FI_MR_VIRT_ADDR|FI_MR_BASIC))

#ifdef DEBUG
  #define dbgprintf( ... ) \
    do { \
      fprintf(stdout, "[DBG] Line %-6d: ", __LINE__ ); \
      fprintf(stdout, __VA_ARGS__); \
      fprintf(stdout, "\n"); \
    } while (0)
#else
  #define dbgprintf( ... )
#endif//DEBUG

#define CALL_FI_API(x,desc) \
  do { \
    int ret = (x); \
    if (ret) { \
      fprintf(stderr, "Fail to %s with error [%d], %s.\n", \
        desc, ret, fi_strerror(-ret)); \
    } \
  } while (0)

#define TIMESPAN_NANOSEC(s,e) (((double)(e).tv_sec - (s).tv_sec)*1e9 + \
  (e).tv_nsec - (s).tv_nsec)
#define TIMESPAN_MICROSEC(s,e) (TIMESPAN_NANOSEC(s,e)/1e3)
#define TIMESPAN_MILLSEC(s,e) (TIMESPAN_MICROSEC(s,e)/1e3)
#define THROUGHPUT_GBPS(b,s,e) ((double)(b)/TIMESPAN_NANOSEC(s,e))
#define THROUGHPUT_MBPS(b,s,e) (THROUGHPUT_GBPS(b,s,e)*1e3)
#define THROUGHPUT_KBPS(b,s,e) (THROUGHPUT_MBPS(b,s,e)*1e3)
///////////////////////////////////////////////////////////////////////////////
// 2. types and data structures
///////////////////////////////////////////////////////////////////////////////
enum LFPFWorkload {
  LFPFNONE  = 0, // nothing - show hardware
  LFPFRDMA1 = 1, // One sided RDMA
  LFPFRDMA2 = 2, // Two sided RDMA
  LFPFINFO  = 99
};

struct lfpf_ctxt {
  struct fi_info *fi, *hints, *fi_pep;
  struct fid_fabric * fabric;
  struct fid_domain * domain;
  struct fid_pep * pep;
  struct fid_ep * ep;
  struct fid_cq * txcq, * rxcq;
  struct fid_mr * mr;
  struct fid_av * av;
  struct fid_eq * eq;

  fi_addr_t remote_fi_addr;
  void * buf, * tx_buf, * rx_buf;
  uint64_t local_mr_key,remote_mr_key;

  int timeout_sec;

  struct fi_av_attr av_attr;
  struct fi_eq_attr eq_attr;
  struct fi_cq_attr cq_attr;

  int ctrl_connfd;
  char ctrl_buf[LFPF_CTRL_BUF_LEN+1];
  char rem_name[LFPF_CTRL_BUF_LEN];

  // application options
  struct {
    uint16_t cli_port;
    uint16_t svr_port;
    char *dst_addr;
    char *domain;
    char *provider;
    int use_odp;
  } opts;

  struct {
    size_t buf_size, tx_size, rx_size;
    int iterations;
    int use_buffer_offset;
    int transfer_depth;
    enum LFPFWorkload workload;
  } common_opts;
};


///////////////////////////////////////////////////////////////////////////////
// 3. test work horses
///////////////////////////////////////////////////////////////////////////////
static int default_context(struct lfpf_ctxt *ct) {
  ct->hints = fi_allocinfo();
  if(!ct->hints) {
    fprintf(stderr, "fi_allocinfo failed.\n" );
    return -1;
  }

  ct->hints->caps = FI_MSG|FI_RMA;
  ct->hints->mode = ~0; // all mode

  ct->hints->ep_attr->type = FI_EP_MSG;

  // ct->hints->domain_attr->mr_mode = FI_MR_LOCAL | FI_MR_ALLOCATED | FI_MR_PROV_KEY;
  // ct->hints->domain_attr->mr_mode = FI_MR_LOCAL | FI_MR_ALLOCATED | FI_MR_PROV_KEY;
  // ct->hints->mode = FI_CONTEXT;

  ct->hints->domain_attr->mode = ~0;
  ct->hints->domain_attr->mr_mode = ~0;

  ct->common_opts.buf_size = 65536;
  ct->common_opts.tx_size = 4096;
  ct->common_opts.rx_size = 4096;

  ct->common_opts.workload = LFPFRDMA1;
  ct->opts.svr_port = LFPF_SERVER_PORT;
  ct->common_opts.iterations = 1000;
  ct->common_opts.use_buffer_offset = 0;
  ct->common_opts.transfer_depth = 16;

  return 0;
}

/**
 * print available endpoints
 * @PARAM ct - context
 * @RETURN VALUE 0 - succ, other - fail
 */
static int show_endpoints(struct lfpf_ctxt *ct) {
  CALL_FI_API(fi_getinfo(LFPF_VERSION,NULL,NULL,0,ct->hints,&ct->fi),"fi_getinfo()");
  // CALL_FI_API(fi_getinfo(LFPF_VERSION,NULL,NULL,0,NULL,&ct->fi),"fi_getinfo()");

  //print endpoints
  struct fi_info *cur;
  int cnt = 0;
  for (cur = ct->fi; cur; cur = cur->next,cnt++) {
    printf("Endpoint %d)\n",cnt);
    printf("provider: %s\n", cur->fabric_attr->prov_name);
    printf("    fabric: %s\n", cur->fabric_attr->name),
    printf("    domain: %s\n", cur->domain_attr->name),
    printf("    version: %d.%d\n", FI_MAJOR(cur->fabric_attr->prov_version),
      FI_MINOR(cur->fabric_attr->prov_version));
    printf("    type: %s\n", fi_tostr(&cur->ep_attr->type, FI_TYPE_EP_TYPE));
    printf("    protocol: %s\n", fi_tostr(&cur->ep_attr->protocol, FI_TYPE_PROTOCOL));
  }

  fi_freeinfo(ct->fi);
  return -1;
}

/**
 * The control plane: making connection to the client.
 */
static int ctrl_init_client(struct lfpf_ctxt *ct) {
  struct sockaddr_in in_addr = {0};
  struct addrinfo *results;
  struct addrinfo *rp;
  int errno_save;
  int ret;
  int optval = 1;

  // get addr info
  {
    const char *err_msg;
    char port_s[6];

    struct addrinfo hints = {
      .ai_family = AF_INET,
      .ai_socktype = SOCK_STREAM,
      .ai_protocol = IPPROTO_TCP,
      .ai_flags = AI_NUMERICSERV
    };
    snprintf(port_s, 6, "%" PRIu16, ct->opts.svr_port);
    ret = getaddrinfo(ct->opts.dst_addr, port_s, &hints, &results);
    if (ret !=0) {
      fprintf(stderr,"getaddrinfo failed with error:%s\n", gai_strerror(ret));
      return ret;
    }
  }

  for (rp = results; rp; rp = rp->ai_next) {
    ct->ctrl_connfd = socket(rp->ai_family,rp->ai_socktype,rp->ai_protocol);
    if(ct->ctrl_connfd == -1) {
      errno_save = errno;
      continue;
    }

    ret = setsockopt(ct->ctrl_connfd, SOL_SOCKET, SO_REUSEADDR,
      (const char *)&optval, sizeof(optval));

    if (ret) {
      errno_save = -errno;
      close(ct->ctrl_connfd);
      continue;
    }

    if (ct->opts.cli_port != 0 ) {
      in_addr.sin_family = AF_INET;
      in_addr.sin_port = htons(ct->opts.cli_port);
      in_addr.sin_addr.s_addr = htonl(INADDR_ANY);

      ret = bind(ct->ctrl_connfd, (struct sockaddr *)&in_addr, sizeof(in_addr));
      if (ret == -1) {
        errno_save = errno;
        close(ct->ctrl_connfd);
        continue;
      }
    }
    ret = connect(ct->ctrl_connfd, rp->ai_addr, rp->ai_addrlen);
    if (ret != -1) {
      break;
    }
    errno_save = errno;
    close(ct->ctrl_connfd);
  }

  if (!rp || ret == -1) {
    ret = -errno_save;
    ct->ctrl_connfd = -1;
    fprintf(stderr,"failed to connect: %s\n", strerror(errno_save));
  }

  freeaddrinfo(results);
  dbgprintf("CLIENT: connected to server.");
  return ret;
}

/**
 * The control plane: listen to client.
 */
static int ctrl_init_server(struct lfpf_ctxt * ct) {
  struct sockaddr_in ctrl_addr = {};
  int optval = 1;
  int listenfd;
  int ret;

  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenfd == -1) {
    ret = -errno;
    fprintf(stderr,"fail to create server socket:%d\n", ret);
    return ret;
  }

  ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
    (const char *)&optval, sizeof(optval));

  if (ret == -1) {
    ret = -errno;
    fprintf(stderr,"setsockopt(SO_REUSEADDR) failed with error:%d\n",ret);
    return ret;
  }

  ctrl_addr.sin_family = AF_INET;
  ctrl_addr.sin_port = htons(ct->opts.svr_port);
  ctrl_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  ret = bind(listenfd, (struct sockaddr *)&ctrl_addr, sizeof(ctrl_addr));
  if (ret == -1) {
    ret = -errno;
    fprintf(stderr,"bind() failed with error:%d\n",ret);
    close(ct->ctrl_connfd);
    ct->ctrl_connfd = -1;
    return ret;
  }

  ret = listen(listenfd, 10);
  if (ret == -1) {
    ret = -errno;
    fprintf(stderr, "listen() failed with error:%d", ret);
    close(listenfd);
    ct->ctrl_connfd = -1;
    return ret;
  }

  ct->ctrl_connfd = accept(listenfd, NULL, NULL);
  if(ct->ctrl_connfd == -1) {
    ret = -errno;
    fprintf(stderr, "accept() failed with error:%d", ret);
    close(listenfd);
    return ret;
  }
  close(listenfd);

  dbgprintf("SERVER: connected.");

  return ret;
}

/**
 * Initialize the context according to hints
 */
static int init_context(struct lfpf_ctxt *ct) {
  int ret = 0;
  ct->timeout_sec = -1;
  ct->ctrl_connfd = -1;
  ct->eq_attr.wait_obj = FI_WAIT_UNSPEC;

  ct->hints->domain_attr->mr_mode = FI_MR_LOCAL | FI_MR_VIRT_ADDR | FI_MR_PROV_KEY;
  if (ct->opts.use_odp) {
    ct->hints->domain_attr->mr_mode |= FI_MR_MMU_NOTIFY;
  } else {
    ct->hints->domain_attr->mr_mode |= FI_MR_ALLOCATED;
  }
  if (strcmp(ct->hints->fabric_attr->prov_name,"sockets") == 0) {
    ct->hints->domain_attr->mr_mode = FI_MR_BASIC;
  }
  ct->hints->mode = FI_CONTEXT;

  //establish the control plane
  if (ct->opts.dst_addr != NULL) {
    ret = ctrl_init_client(ct);
  } else {
    ret = ctrl_init_server(ct);
  }
  return ret;
}

static int send_name(struct lfpf_ctxt *ct, struct fid *endpoint) {
  char local_name[64];
  size_t addrlen;
  uint32_t len;
  int ret;

  addrlen = sizeof(local_name);
  ret = fi_getname(endpoint, local_name, &addrlen);
  if (ret) {
    fprintf(stderr,"fi_getname() failed with error=%d\n",errno);
    return ret;
  }

  if (addrlen > sizeof(local_name)) {
    fprintf(stderr,"Address exceeds control buffer lenght\n");
    return -1;
  }

  len = htonl(addrlen);
  ret = send(ct->ctrl_connfd,&addrlen,sizeof(addrlen),0);
  if(ret != sizeof(addrlen)) {
    fprintf(stderr,"send name length failed with error:%d(%s)\n",errno,strerror(errno));
    return ret;
  }
  ret = send(ct->ctrl_connfd,local_name,addrlen,0);
  if(ret != addrlen) {
    fprintf(stderr, "send name failed with error:%d(%s)\n",errno,strerror(errno));
    return ret;
  }
  return 0;
}

static int recv_name(struct lfpf_ctxt *ct) {
  size_t len;
  int ret;

  ret = recv(ct->ctrl_connfd,&len,sizeof(len),MSG_WAITALL);
  if (ret != sizeof(len)) {
    fprintf(stderr, "1) recv() failed with error:%d(%s) len=%ld, ret=%d\n",errno,strerror(errno),sizeof(len),ret);
    return ret;
  }
  if (len > sizeof(ct->rem_name)) {
    fprintf(stderr, "Address length exceeds address storage\n.");
    return -1;
  }
  ret = recv(ct->ctrl_connfd,ct->rem_name,len,MSG_WAITALL);
  if (ret != len) {
    fprintf(stderr, "2) recv() failed with error:%d(%s) len=%ld, ret=%d\n",errno,strerror(errno),len,ret);
    return ret;
  }
  dbgprintf("Received name");
  ct->hints->dest_addr = malloc(len);
  if(!ct->hints->dest_addr) {
    fprintf(stderr, "Failed to allocated memory for destination address\n");
    return -1;
  }

  memcpy(ct->hints->dest_addr, ct->rem_name, len);
  ct->hints->dest_addrlen = len;

  dbgprintf("ct->hints->dest_addr=[");
  for(int i=0;i<len;i++) {
    printf("%-2x ",(0xff&((char*)ct->hints->dest_addr)[i]));
  }
  printf("]\n");

  return 0;
}

/**
 * exhange the names after client and server connect.
 */
static int exchange_names_connected(struct lfpf_ctxt *ct) {
  int ret = 0;
  if (ct->opts.dst_addr) {
    ret = recv_name(ct);
    if (ret) {
      return ret;
    }
    dbgprintf("client hints:\n%s.",fi_tostr(ct->hints,FI_TYPE_INFO));
    CALL_FI_API(ret=fi_getinfo(LFPF_VERSION, NULL, NULL, 0, ct->hints, &(ct->fi)),"fi_getinfo");
    dbgprintf("client info:\n%s.",fi_tostr(ct->fi,FI_TYPE_INFO));
  } else {
    ret = send_name(ct, &ct->pep->fid);
    if (ret) {
      return ret;
    }
  }
  return ret;
}

/**
 * exhange the memory key.
 */
static int exchange_mr_info(struct lfpf_ctxt *ct) {
  int ret = 0;

  dbgprintf("sending local mr key:%lx",ct->local_mr_key);
  if (send(ct->ctrl_connfd,&ct->local_mr_key,sizeof(ct->local_mr_key),MSG_CONFIRM) < 0) {
    fprintf(stderr, "failed to send the memory key, errcode=%d, error=%s\n", errno, strerror(errno));
    return -1;
  }
  dbgprintf("sending local mr address:%p",ct->buf);
  if (send(ct->ctrl_connfd,&ct->buf,sizeof(ct->buf),MSG_CONFIRM) < 0) {
    fprintf(stderr, "failed to send the memory address, errcode=%d, error=%s\n", errno, strerror(errno));
    return -1;
  }


  if (recv(ct->ctrl_connfd,&ct->remote_mr_key,sizeof(ct->remote_mr_key),MSG_WAITALL) != 
    sizeof(ct->remote_mr_key)) {
    fprintf(stderr, "failed to read the memory key, errcode=%d, error=%s\n", errno, strerror(errno));
    return -1;
  }
  dbgprintf("remote_mr_key received:%lx",ct->remote_mr_key);

  if (recv(ct->ctrl_connfd,&ct->remote_fi_addr,sizeof(ct->buf),MSG_WAITALL) != 
    sizeof(ct->remote_fi_addr)) {
    fprintf(stderr, "failed to read the memory address, errcode=%d, error=%s\n", errno, strerror(errno));
    return -1;
  }
  dbgprintf("remote buf received:%p",(void*)ct->remote_fi_addr);

  return ret;
}

/**
 * allocate message buffer
 * ct->buf = [...recv buf...][...send buf...]
 */
static int alloc_message_buf(struct lfpf_ctxt *ct) {
  int ret = 0;
  long alignment = 1;

  alignment = sysconf(_SC_PAGESIZE);
  if (alignment < 0) {
    ret = -1;
    fprintf(stderr,"sysconf(_SC_PAGESIZE) failed with %d\n", errno);
    return ret;
  }
  ret = posix_memalign(&(ct->buf), (size_t)alignment, ct->common_opts.buf_size<<1);
  if (ret) {
    fprintf(stderr,"memalign failed with %d\n", errno);
    return ret;
  }
  memset(ct->buf, 0, ct->common_opts.buf_size<<1);
  ct->rx_buf = ct->buf;
  ct->tx_buf = (char*)ct->buf + ct->common_opts.buf_size;

  if (ct->opts.dst_addr) {
    memset(ct->buf, 'c', ct->common_opts.buf_size); // client
  } else {
    memset(ct->buf, 's', ct->common_opts.buf_size); // server
  }

  return ret;
}

/**
 * initialize endpoint
 */
static int init_endpoint(struct lfpf_ctxt *ct, struct fi_info *fi) {
  int ret;

  // 1 - open completion queues
  if (ct->cq_attr.format == FI_CQ_FORMAT_UNSPEC)
    ct->cq_attr.format = FI_CQ_FORMAT_CONTEXT;
  ct->cq_attr.wait_obj = FI_WAIT_NONE;
  ct->cq_attr.size = fi->tx_attr->size;
  ret = fi_cq_open(ct->domain, &(ct->cq_attr), &(ct->txcq), &(ct->txcq));
  if (ret) {
    fprintf(stderr, "fi_cq_open txcq failed with error:%d, %s\n", -ret, fi_strerror(-ret));
    return ret;
  }
  ret = fi_cq_open(ct->domain, &(ct->cq_attr), &(ct->rxcq), &(ct->rxcq));
  if (ret) {
    fprintf(stderr, "fi_cq_open txcq failed with error:%d, %s\n", -ret, fi_strerror(-ret));
    return ret;
  }
  // 2 - open endpoint
  ret = fi_endpoint(ct->domain, fi, &(ct->ep), NULL);
  if (ret) {
    fprintf(stderr, "fi_endpoint failed with error: %d, %s\n", -ret, fi_strerror(-ret));
    return ret;
  }
  // 3 - bind them together
  if(ct->eq)
    ret = fi_ep_bind(ct->ep, &(ct->eq)->fid, 0);
  if (ret) {
    fprintf(stderr, "binding event queue failed: %d\n",ret);
    return ret;
  }
  if(ct->av)
    ret = fi_ep_bind(ct->ep, &(ct->av)->fid, 0);
  if (ret) {
    fprintf(stderr, "binding address vector failed: %d\n",ret);
    return ret;
  }
  if(ct->txcq)
    ret = fi_ep_bind(ct->ep, &(ct->txcq)->fid, FI_TRANSMIT);
  if (ret) {
    fprintf(stderr, "binding tx completion queue queue failed: %d, %s\n",ret, fi_strerror(-ret));
    return ret;
  }
  if(ct->rxcq)
    ret = fi_ep_bind(ct->ep, &(ct->rxcq)->fid, FI_RECV);
  if (ret) {
    fprintf(stderr, "binding rx completion queue queue failed: %d\n",ret);
    return ret;
  }
  ret = fi_enable(ct->ep); // enable endpoint
  if (ret) {
    fprintf(stderr, "enable endpoint failed with error: %d\n",ret);
    return ret;
  }
  return 0;
}

/**
 * Initialize the passive endpoint on the client side:
 */
static int init_client_ep(struct lfpf_ctxt *ct) {
  int ret = 0;
  CALL_FI_API(fi_fabric(ct->fi->fabric_attr, &(ct->fabric), NULL), "fi_fabric()");
  CALL_FI_API(fi_eq_open(ct->fabric,&(ct->eq_attr),&(ct->eq), NULL), "fi_eq_open()");
  CALL_FI_API(fi_domain(ct->fabric,ct->fi, &(ct->domain), NULL), "fi_domain()");
  return ret;
}

/**
 * Start the RDMA Client - establish the data plane
 */
static int start_rdma_client(struct lfpf_ctxt *ct) {
  struct fi_eq_cm_entry entry;
  uint32_t event;
  ssize_t rd;
  int ret;

  // 1 - initialize context
  dbgprintf("initialize the context...");
  ret = init_context(ct);
  if (ret) {
    return ret;
  }

  // 1.5 - send options
  dbgprintf("send options...");
  ret = send(ct->ctrl_connfd,&(ct->common_opts),sizeof(ct->common_opts),MSG_CONFIRM);
  if(ret != sizeof(ct->common_opts)) {
    fprintf(stderr,"failed to send options to server.");
    return errno;
  }

  // 2 - recv name
  dbgprintf("exchange name with server...");
  ret = exchange_names_connected(ct);
  if (ret) {
    return ret;
  }

  // 3 - initialize the endpoint.
  dbgprintf("initialize the client endpoint...");
  ret = init_client_ep(ct);
  if (ret) {
    return ret;
  }

  // 4 - allocate buffer
  dbgprintf("allocate buffer...");
  ret = alloc_message_buf(ct);
  if (ret) {
    return ret;
  }

  // 5 - register memory
  dbgprintf("allocate memory...");
  CALL_FI_API( fi_mr_reg(ct->domain, ct->buf, ct->common_opts.buf_size<<1,
    FI_SEND | FI_RECV | FI_READ | FI_WRITE | FI_REMOTE_READ | FI_REMOTE_WRITE,
    // 0, 0, 0, &(ct->mr), NULL), "fi_mr_reg()" );
    0, LFPF_MR_KEY, 0, &(ct->mr), NULL), "fi_mr_reg()" );
  ct->local_mr_key = fi_mr_key(ct->mr);
  if (ct->local_mr_key == FI_KEY_NOTAVAIL) {
    fprintf(stderr,"fail to get mr_key.\n");
    return -1;
  }
  dbgprintf("client memory registered:[%p-%p].request key=0x%lx,received key=0x%lx",
    ct->buf,ct->buf+(ct->common_opts.buf_size<<1),
    // (int64_t)0,fi_mr_key(ct->mr));
    (int64_t)LFPF_MR_KEY,fi_mr_key(ct->mr));

  // 6 - initialize the endpoint
  dbgprintf("initialize endpoint...");
  ret = init_endpoint(ct,ct->fi);
  if (ret) {
    return ret;
  }

  // 7 - handle connection
  dbgprintf("connecting server endpoint...");
  CALL_FI_API(fi_connect(ct->ep, ct->rem_name, NULL, 0),"fi_connect()");
  rd = fi_eq_sread(ct->eq, &event, &entry, sizeof(entry), -1, 0);
  if (rd != sizeof(entry)) {
    fprintf(stderr, "Unknown CM event: fi_eq_sread() return ret(%ld)!=sizeof(entry)\n",rd);
    return -1;
  }
  if (event != FI_CONNECTED || entry.fid != &(ct->ep->fid)) {
    fprintf(stderr,"Unexpected CM event %d fid %p (ep %p)\n",
      event, entry.fid, ct->ep);
    return -1;
  }
  dbgprintf("Client connected!");

  // 8 - exchange the memory key:
  dbgprintf("exchange the memory info...");
  ret = exchange_mr_info(ct);
  if (ret) {
    return ret;
  }

  return 0;
}

/**
 * Initialize the passive endpoint on the server side:
 */
static int init_server_pep(struct lfpf_ctxt *ct) {
  int ret = 0;
  dbgprintf("Passive EP hints:%s\n",fi_tostr(ct->hints,FI_TYPE_INFO));
  CALL_FI_API(fi_getinfo(LFPF_VERSION,NULL,NULL,0,ct->hints,&(ct->fi_pep)),"fi_getinfo()");
  CALL_FI_API(fi_fabric(ct->fi_pep->fabric_attr, &(ct->fabric), NULL),"fi_fabric()");
  CALL_FI_API(fi_eq_open(ct->fabric, &(ct->eq_attr), &(ct->eq), NULL),"fi_eq_open()");
  CALL_FI_API(fi_passive_ep(ct->fabric,ct->fi_pep,&(ct->pep),NULL),"fi_passive_ep()");
  CALL_FI_API(fi_pep_bind(ct->pep, &(ct->eq->fid), 0),"fi_pep_bind()");
  CALL_FI_API(fi_listen(ct->pep),"fi_listen");
  return ret;
}

/** 
 * Start the RDMA Server - establish the data plane
 */
static int start_rdma_server(struct lfpf_ctxt *ct) {
  struct fi_eq_cm_entry entry;
  uint32_t event;
  ssize_t rd;
  int ret;

  // 1 - initialize context
  dbgprintf("initialize the context...");
  ret = init_context(ct);
  if (ret) {
    return ret;
  }

  // 1.5 receive options from client
  dbgprintf("get options...");
  ret = recv(ct->ctrl_connfd,&(ct->common_opts),sizeof(ct->common_opts),MSG_WAITALL);
  if (ret != sizeof(ct->common_opts)) {
    fprintf(stderr,"failed to receive options,error=%d\n",errno);
    return -errno;
  }

  // 2 - initialize the passive endpoint
  dbgprintf("initialize passive endpoint...");
  ret = init_server_pep(ct);
  if (ret) {
    return ret;
  }

  // 3 - send name
  dbgprintf("exchange name with client...");
  ret = exchange_names_connected(ct);
  if (ret) {
    return ret;
  }

  // 4 - accepting connection
  dbgprintf("waiting for connection request...");
  rd = fi_eq_sread(ct->eq, &event, &entry, sizeof(entry), -1,0);
  if (rd != sizeof(entry)) {
    fprintf(stderr,"failed to read connection state with error:%ld\n",rd);
    return -1;
  }

  ct->fi = entry.info;
  if (event != FI_CONNREQ) {
    fprintf(stderr, "Unexpected CM event %d\n", event);
    fi_reject(ct->pep, ct->fi->handle, NULL, 0);
    return -1;
  }

  CALL_FI_API(ret=fi_domain(ct->fabric, ct->fi, &(ct->domain), NULL),"fi_domain()");
  dbgprintf("Server endpoint connected!");

  // 5 - allocate buffer
  dbgprintf("allocate memory...");
  ret = alloc_message_buf(ct);
  if (ret) {
    return ret;
  }

  // 6 - register memory
  dbgprintf("register memory...");
  CALL_FI_API( fi_mr_reg(ct->domain, ct->buf, ct->common_opts.buf_size<<1,
    FI_SEND | FI_RECV | FI_READ | FI_WRITE | FI_REMOTE_READ | FI_REMOTE_WRITE,
    0, LFPF_MR_KEY, 0, &(ct->mr), NULL), "fi_mr_reg()" );
  ct->local_mr_key = fi_mr_key(ct->mr);
  if (ct->local_mr_key == FI_KEY_NOTAVAIL) {
    fprintf(stderr,"fail to get mr_key.\n");
    return -1;
  }
  dbgprintf("server memory registered:[%p-%p].request key=0x%lx,received key=0x%lx",
    ct->buf,ct->buf+(ct->common_opts.buf_size<<1),
    (int64_t)LFPF_MR_KEY,fi_mr_key(ct->mr));
    // (int64_t)LFPF_MR_KEY,fi_mr_key(ct->mr));

  // 7 - binding the endpoint resources
  dbgprintf("binding the endpoint resources.");
  ret = init_endpoint(ct,ct->fi);
  if (ret) {
    fprintf(stderr, "Binding the endpoint failed with error %d\n", ret);
    return ret;
  }

  // 8 - accept
  dbgprintf("accept the connection.");
  ret = fi_accept(ct->ep, NULL, 0);
  if (ret) {
    fprintf(stderr, "fi_accept failed with error: %d, %s\n", ret, fi_strerror(-ret));
    fi_reject(ct->pep, ct->fi->handle, NULL, 0);
    return ret;
  }
  dbgprintf("read connection confirmation from the new created endpoint...");
  rd = fi_eq_sread(ct->eq, &event, &entry, sizeof(entry), -1, 0);
  if (rd != sizeof(entry)) {
    fprintf(stderr, "fi_eq_sread() failed.");
    fi_reject(ct->pep, ct->fi->handle, NULL, 0);
    return -1;
  }
  if (event != FI_CONNECTED || entry.fid != &(ct->ep->fid)) {
    fprintf(stderr, "Failed to get FI_CONNECTED. event:%d, fid:%p, ep:%p\n",
      event, entry.fid, ct->ep);
    fi_reject(ct->pep, ct->fi->handle, NULL, 0);
    return -1;
  }

  // 9 - exchange memory key
  dbgprintf("exchange memory info...");
  ret = exchange_mr_info(ct);
  if (ret) {
    return ret;
  }

  return 0;
}


static int comp_double (const void *e1, const void * e2) {
  double d1 = *((double*)e1);
  double d2 = *((double*)e2);
  if (d1 > d2) return 1;
  if (d1 < d2) return -1;
  return 0;
}
/**
 * finding the average,10pct,90pct, and deviation, of an array of double
 * @PARAM raw - raw data array
 * @PARAM len - length of the array
 * @PARAM stat - statistics of the array
 */
static void get_arr_stat(double raw[],int len,double stat[]) {
  double sum = 0.0f,var = 0.0f;
  double sorted[len];
  int i;

  memcpy(sorted,raw,sizeof(double)*len);
  qsort(sorted,len,sizeof(double),comp_double);

  for(i=0;i<len;i++)
    sum += sorted[i];

  stat[0] = sum/len;
  stat[1] = sorted[len/10];
  stat[2] = sorted[len*9/10];

  for(i=0;i<len;i++)
    var += (sorted[i]-stat[0])*(sorted[i]-stat[0])/(len-1);

  stat[3] = sqrt(var);
}


/**
 * run one sided rdma test
 */
static int run_one_sided_rdma(struct lfpf_ctxt *ct) {
  const char * done = "DONE";
  char done_buf[8];

  if(ct->opts.dst_addr) { // client
    // test
    for (int isRead=0;isRead<2;isRead++) {
      struct timespec tv_start, tv_end;
      int num_slots = ct->common_opts.buf_size / ct->common_opts.tx_size;
      dbgprintf("num_slots=%d",num_slots);
      ssize_t num_in_the_air = 0;
      char buf[sizeof(struct fi_cq_err_entry)*ct->common_opts.transfer_depth];
      ssize_t num_comp;
      int i;

      // throughput
      clock_gettime(CLOCK_REALTIME,&tv_start);
      for(i=0;i<ct->common_opts.iterations;i++) {
#if 1
        struct fi_msg_rma msg;
        struct iovec iov;
        void *desc = (void *) ct->local_mr_key;
        struct fi_rma_iov rma_iov;
        //call send/recv
        if(isRead) {
          iov.iov_base = ct->rx_buf+(i%num_slots)*ct->common_opts.rx_size;
          iov.iov_len = ct->common_opts.rx_size;
          msg.msg_iov = &iov;
          msg.desc = &desc;
          msg.iov_count = 1;
          msg.addr = ct->remote_fi_addr;
          rma_iov.addr = (LF_USE_VADDR?ct->remote_fi_addr:0) + (i%num_slots)*ct->common_opts.rx_size;
          rma_iov.len = ct->common_opts.rx_size;
          rma_iov.key = ct->remote_mr_key;
          msg.rma_iov = &rma_iov;
          msg.rma_iov_count = 1;
          msg.context = NULL;
          CALL_FI_API(fi_readmsg(ct->ep,&msg,FI_COMPLETION),"fi_readmsg");
        } else {
          iov.iov_base = ct->tx_buf+(i%num_slots)*ct->common_opts.rx_size;
          iov.iov_len = ct->common_opts.tx_size;
          msg.msg_iov = &iov;
          msg.desc = &desc;
          msg.iov_count = 1;
          msg.addr = ct->remote_fi_addr;
          rma_iov.addr = (LF_USE_VADDR?ct->remote_fi_addr:0) + (i%num_slots)*ct->common_opts.tx_size;
          rma_iov.len = ct->common_opts.rx_size;
          rma_iov.key = ct->remote_mr_key;
          msg.rma_iov = &rma_iov;
          msg.rma_iov_count = 1;
          msg.context = NULL;
          CALL_FI_API(fi_writemsg(ct->ep,&msg,FI_COMPLETION),"fi_writemsg");
        }
#else
        // call send
        if(isRead){
          dbgprintf("local read buffer=%p",ct->rx_buf+(i%num_slots)*ct->common_opts.rx_size);
          CALL_FI_API(fi_read(
            ct->ep,
            ct->rx_buf+(i%num_slots)*ct->common_opts.rx_size,
            ct->common_opts.rx_size,
            (void *) ct->local_mr_key,
            ct->remote_fi_addr,
            (LF_USE_VADDR?ct->remote_fi_addr:0) + (i%num_slots)*ct->common_opts.rx_size,
            ct->remote_mr_key,
            NULL),
            "fi_read()");
        } else {
          dbgprintf("local write buffer=%p",ct->tx_buf+(i%num_slots)*ct->common_opts.tx_size);
          CALL_FI_API(fi_write(
            ct->ep,
            ct->tx_buf+(i%num_slots)*ct->common_opts.tx_size,
            ct->common_opts.tx_size,
            (void *) ct->local_mr_key,
            ct->remote_fi_addr,
            (LF_USE_VADDR?ct->remote_fi_addr:0) + (i%num_slots)*ct->common_opts.tx_size,
            ct->remote_mr_key,
            NULL),
            "fi_write()");
        }
#endif
        //waiting for completion
        if (++num_in_the_air == ct->common_opts.transfer_depth) {
          do {
            num_comp = fi_cq_read(ct->txcq, buf, 1);
          } while(num_comp == -FI_EAGAIN);
          if (num_comp == 1){
            num_comp = fi_cq_read(ct->txcq, buf, --num_in_the_air);
            if ((num_comp < 0) && (-num_comp != FI_EAGAIN)) {
              fprintf(stderr, "[1] fi_cq_read() failed with error:%ld,%s\n",
                num_comp, fi_strerror(-num_comp));
              return -1;
            }
            if (num_comp >0 ) {
              num_in_the_air -= num_comp;
            }
          } else {
            fprintf(stderr, "[2]fi_cq_read() failed with error:%ld,%s\n",
              num_comp, fi_strerror(-num_comp));
            return -1;
          }
        }
      }
      //waiting for the rest completion
      while (num_in_the_air > 0) {
        do{
          num_comp = fi_cq_read(ct->txcq, buf, num_in_the_air);
        } while(num_comp == -FI_EAGAIN);

        if (num_comp < 0) {
          fprintf(stderr, "[3] fi_cq_read() failed with error:%ld,%s\n",
            num_comp, fi_strerror(-num_comp));
          return -1;
        }

        num_in_the_air -= num_comp;
      }
      clock_gettime(CLOCK_REALTIME,&tv_end);

      if (isRead)
        printf("one sided rdma read performance\n");
      else
        printf("one sided rdma write performance\n");
      printf("\ttimespan:\t%.3f ms\n",TIMESPAN_MILLSEC(tv_start,tv_end));
      printf("\tthroughput:\t%.3f GByte/s\n",THROUGHPUT_GBPS(
        ct->common_opts.iterations*ct->common_opts.tx_size,tv_start,tv_end));

      // latency: we test only 1000 operations
      #define NUM_LAT_SAMPLE (1000)
      double lat[NUM_LAT_SAMPLE];
      double lat_stat[4];
      for (i=0;i<NUM_LAT_SAMPLE;i++) {
        clock_gettime(CLOCK_REALTIME,&tv_start);
        if(isRead){
          CALL_FI_API(fi_read(
            ct->ep,
            ct->rx_buf+(i%num_slots)*ct->common_opts.rx_size,
            ct->common_opts.rx_size,
            (void *)ct->local_mr_key, // wired api
            ct->remote_fi_addr,
            (LF_USE_VADDR?ct->remote_fi_addr:0) + (i%num_slots)*ct->common_opts.rx_size,
            ct->remote_mr_key,
            NULL),
            "fi_read()");
        } else {
          CALL_FI_API(fi_write(
            ct->ep,
            ct->tx_buf+(i%num_slots)*ct->common_opts.tx_size,
            ct->common_opts.tx_size,
            (void *)ct->local_mr_key, // wired api
            ct->remote_fi_addr,
            (LF_USE_VADDR?ct->remote_fi_addr:0) + (i%num_slots)*ct->common_opts.tx_size,
            ct->remote_mr_key,
            NULL),
            "fi_write()");
        }
        do {
          num_comp = fi_cq_read(ct->txcq, buf, 1);
        } while(num_comp == -FI_EAGAIN);
        clock_gettime(CLOCK_REALTIME,&tv_end);
        lat[i] = TIMESPAN_NANOSEC(tv_start,tv_end);
        if(num_comp != 1) {
          fprintf(stderr,"fi_cq_read() failed with error:%ld,%s\n",
            num_comp,fi_strerror(-num_comp));
          return num_comp;
        }
      }
      get_arr_stat(lat,NUM_LAT_SAMPLE,lat_stat);
      printf("\tAverage latency: \t%.1f ns\n", lat_stat[0]);
      printf("\t10pct: \t%.1f ns\n", lat_stat[1]);
      printf("\t90pct: \t%.1f ns\n", lat_stat[2]);
      printf("\tstandard deviation: \t%.1f\n", lat_stat[3]);
    }
    // send 'done' command
    if (send(ct->ctrl_connfd,done,strlen(done),MSG_CONFIRM) < 0) {
      fprintf(stderr, "failed to send 'done' message.");
      return -1;
    }
    usleep(1000000);
  } else { // server
    while(recv(ct->ctrl_connfd, done_buf, strlen(done),MSG_WAITALL) != strlen(done) < 0) {}
    if (strncmp(done_buf,done,4)){
      fprintf(stderr, "Unknown message received:[%-2x %-2x %-2x %-2x]\n",
        done_buf[0],done_buf[1],done_buf[2],done_buf[3]);
    } 
  }
}

/**
 * run two sided rdma test
 */
static int run_two_sided_rdma(struct lfpf_ctxt *ct) {
  const char * done = "DONE";
  char done_buf[8];
  struct timespec tv_start, tv_end;
  int num_slots = ct->common_opts.buf_size / ct->common_opts.tx_size;
  dbgprintf("num_slots=%d",num_slots);
  ssize_t num_in_the_air = 0;
  char buf[sizeof(struct fi_cq_err_entry)*ct->common_opts.transfer_depth];
  ssize_t num_comp;
  int i;

  if (ct->opts.dst_addr) { // client
    //test throughput
    clock_gettime(CLOCK_REALTIME,&tv_start);
    for(i=0;i<ct->common_opts.iterations;i++) {
      CALL_FI_API(fi_send(
        ct->ep,
        ct->tx_buf+(i%num_slots)*ct->common_opts.rx_size,
        ct->common_opts.rx_size,
        (void *) ct->local_mr_key,
        0,
        NULL),
        "fi_send");
      // wait for completion
      if (++num_in_the_air == ct->common_opts.transfer_depth) {
        do {
          num_comp = fi_cq_read(ct->txcq, buf, 1);
        } while(num_comp == -FI_EAGAIN);
        if (num_comp == 1){
          num_comp = fi_cq_read(ct->txcq, buf, --num_in_the_air);
          if ((num_comp < 0) && (-num_comp != FI_EAGAIN)) {
            fprintf(stderr, "[1] fi_cq_read() failed with error:%ld,%s\n",
              num_comp, fi_strerror(-num_comp));
            return -1;
          }
          if (num_comp >0 ) {
            num_in_the_air -= num_comp;
          }
        } else {
          fprintf(stderr, "[2]fi_cq_read() failed with error:%ld,%s\n",
            num_comp, fi_strerror(-num_comp));
          return -1;
        }
      }
    }

    //waiting for the rest completion
    while (num_in_the_air > 0) {
      do{
        num_comp = fi_cq_read(ct->txcq, buf, num_in_the_air);
      } while(num_comp == -FI_EAGAIN);

      if (num_comp < 0) {
        fprintf(stderr, "[3] fi_cq_read() failed with error:%ld,%s\n",
          num_comp, fi_strerror(-num_comp));
        return -1;
      }

      num_in_the_air -= num_comp;
    }

    clock_gettime(CLOCK_REALTIME,&tv_end);
    printf("two sided rdma write performance\n");
    printf("\ttimespan:\t%.3f ms\n",TIMESPAN_MILLSEC(tv_start,tv_end));
    printf("\tthroughput:\t%.3f GByte/s\n",THROUGHPUT_GBPS(
      ct->common_opts.iterations*ct->common_opts.tx_size,tv_start,tv_end));

    dbgprintf("begin send latency packets.");
    // test latency
    double lat[NUM_LAT_SAMPLE];
    double lat_stat[4];
    for(i=0;i<NUM_LAT_SAMPLE;i++) {
      clock_gettime(CLOCK_REALTIME,&tv_start);
      CALL_FI_API(fi_send(
        ct->ep,
        ct->tx_buf+(i%num_slots)*ct->common_opts.rx_size,
        ct->common_opts.rx_size,
        (void *) ct->local_mr_key,
        0,
        NULL),
        "fi_send");
      // wait for completion
      do {
        num_comp = fi_cq_read(ct->txcq, buf, 1);
      } while(num_comp == -FI_EAGAIN);

      clock_gettime(CLOCK_REALTIME,&tv_end);
      lat[i] = TIMESPAN_NANOSEC(tv_start,tv_end);
      if(num_comp != 1) {
        fprintf(stderr,"fi_cq_read() failed with error:%ld,%s\n",
          num_comp,fi_strerror(-num_comp));
        return num_comp;
      }
      dbgprintf("pkt-%d acknowledged.",i);
    }
    dbgprintf("finish send latency packets.");
    get_arr_stat(lat,NUM_LAT_SAMPLE,lat_stat);
    printf("\tAverage latency: \t%.1f ns\n", lat_stat[0]);
    printf("\t10pct: \t%.1f ns\n", lat_stat[1]);
    printf("\t90pct: \t%.1f ns\n", lat_stat[2]);
    printf("\tstandard deviation: \t%.1f\n", lat_stat[3]);

    // send 'done' command
    if (send(ct->ctrl_connfd,done,strlen(done),MSG_CONFIRM) < 0) {
      fprintf(stderr, "failed to send 'done' message.");
      return -1;
    }
    usleep(1000000);
  } else { // server
    for(i=0;i<(ct->common_opts.iterations + NUM_LAT_SAMPLE);i++) { // for both throughput and latency test
      dbgprintf("post recv pkt-%d.",i);
      CALL_FI_API(fi_recv(
        ct->ep,
        ct->rx_buf+(i%num_slots)*ct->common_opts.rx_size,
        ct->common_opts.rx_size,
        (void *)ct->local_mr_key,
        0,
        NULL),
        "fi_recv()");
      // wait for completion
      if (++num_in_the_air == ct->common_opts.transfer_depth) {
        do {
          num_comp = fi_cq_read(ct->rxcq, buf, 1);
        } while(num_comp == -FI_EAGAIN);
        if (num_comp == 1){
          num_comp = fi_cq_read(ct->rxcq, buf, --num_in_the_air);
          if ((num_comp < 0) && (-num_comp != FI_EAGAIN)) {
            fprintf(stderr, "[1] fi_cq_read() failed with error:%ld,%s\n",
              num_comp, fi_strerror(-num_comp));
            return -1;
          }
          if (num_comp >0 ) {
            num_in_the_air -= num_comp;
          }
        } else {
          fprintf(stderr, "[2]fi_cq_read() failed with error:%ld,%s\n",
            num_comp, fi_strerror(-num_comp));
          return -1;
        }
      }
    }

    //waiting for the rest completion
    while (num_in_the_air > 0) {
      do{
        num_comp = fi_cq_read(ct->rxcq, buf, num_in_the_air);
      } while(num_comp == -FI_EAGAIN);

      if (num_comp < 0) {
        fprintf(stderr, "[3] fi_cq_read() failed with error:%ld,%s\n",
          num_comp, fi_strerror(-num_comp));
        return -1;
      }

      num_in_the_air -= num_comp;
    }
    // wait for 'done' command
    while(recv(ct->ctrl_connfd, done_buf, strlen(done),MSG_WAITALL) != strlen(done) < 0) {}
    if (strncmp(done_buf,done,4)){
      fprintf(stderr, "Unknown message received:[%-2x %-2x %-2x %-2x]\n",
        done_buf[0],done_buf[1],done_buf[2],done_buf[3]);
    }
  }
}

static int clean_res(struct lfpf_ctxt *ct) {
  if (ct->mr) {
    CALL_FI_API(fi_close(&ct->mr->fid),"close mr");
  }
  if (ct->ep) {
    CALL_FI_API(fi_close(&ct->ep->fid),"close endpoint");
  }
  if (ct->pep) {
    CALL_FI_API(fi_close(&ct->pep->fid),"close passive endpoint");
  }
  if (ct->rxcq) {
    CALL_FI_API(fi_close(&ct->rxcq->fid),"close rxcq");
  }
  if (ct->txcq) {
    CALL_FI_API(fi_close(&ct->txcq->fid),"close mr");
  }
  if (ct->av) {
    CALL_FI_API(fi_close(&ct->av->fid),"close address vector");
  }
  if (ct->eq) {
    CALL_FI_API(fi_close(&ct->eq->fid),"close address vector");
  }
  if (ct->domain) {
    CALL_FI_API(fi_close(&ct->domain->fid),"close domain");
  }
  if (ct->fabric) {
    CALL_FI_API(fi_close(&ct->fabric->fid),"close fabric");
  }
  if (ct->buf) {
    free(ct->buf);
    ct->buf = ct->rx_buf = ct->tx_buf = NULL;
    ct->common_opts.buf_size = ct->common_opts.rx_size = ct->common_opts.tx_size = 0;
  }
  if (ct->fi_pep) {
    fi_freeinfo(ct->fi_pep);
    ct->fi_pep = NULL;
  }

  if (ct->fi) {
    fi_freeinfo(ct->fi);
    ct->fi = NULL;
  }

  if (ct->hints) {
    fi_freeinfo(ct->hints);
    ct->hints = NULL;
  }

  if (ct->ctrl_connfd != -1) {
    dbgprintf("disconnected...");
    close(ct->ctrl_connfd);
    ct->ctrl_connfd = -1;
  }

  return 0;
}

static int run(struct lfpf_ctxt *ct) {
  int ret = 0;

  // 1 - start
  if (ct->opts.dst_addr) {
    ret = start_rdma_client(ct);
    if (ret) {
      fprintf(stderr,"failed to start rdma client.\n");
      return ret;
    }
  } else {
    ret = start_rdma_server(ct);
    if (ret) {
      fprintf(stderr, "failed to start rdma server.\n");
      return ret;
    }
  }

  // 2 - run workload
  switch(ct->common_opts.workload) {
  case LFPFRDMA1:
    run_one_sided_rdma(ct);
    break;
  case LFPFRDMA2:
    run_two_sided_rdma(ct);
    break;
  default:
    break;
  }

  // 3 - clean up
  dbgprintf("call clean_res()");
  clean_res(ct);
  return -1;
}

///////////////////////////////////////////////////////////////////////////////
// 4. main entrance
///////////////////////////////////////////////////////////////////////////////
static struct option lfpf_opts[] = {
  { "domain",    1, NULL, 'd'},
  { "provider",  1, NULL, 'p'},
  { "endpoint",  1, NULL, 'e'},
  { "workload",  1, NULL, 'w'},
  { "bufsize",   1, NULL, 'b'},
  { "xsize",     1, NULL, 's'},
  { "depth",     1, NULL, 'D'},
  { "iter",      1, NULL, 'i'},
  { "bufofst",   0, NULL, 'o'},
  { "odp",       0, NULL, 'O'},
  { "cliport",   1, NULL, 'C'},
  { "svrport",   1, NULL, 'S'},
  { "help",      0, NULL, 'h'},
  { 0, 0, 0, 0 }
};

void print_usage(const char *name, const char *desc) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "  %s [OPTIONS]\t\tstart server\n", name);
  fprintf(stderr, "  %s [OPTIONS] <srv_addr>\tconnect to server\n", name);

  if (desc) {
    fprintf(stderr, "\n%s\n", desc);
  }

  fprintf(stderr, "\nOptions:\n");
  fprintf(stderr, " %-20s %s\n", "--(d)omain <domain>","domain name");
  fprintf(stderr, " %-20s %s\n", "--(p)rovider <provider>",
    "specific provider name: sockets|verbs|.. (sockets)");
  fprintf(stderr, " %-20s %s\n", "--(e)ndpoint <ep_type>",
    "endpoint type: msg|rdm|dgarm (msg)");
  fprintf(stderr, " %-20s %s\n", "--(w)orkload <workload>",
    "workload type: rdma1|rdma2|info ");
  fprintf(stderr, " %-20s %s\n", "--(b)ufsize <buffersize>",
    "total size of mr buffersize (65536)");
  fprintf(stderr, " %-20s %s\n", "--x(s)ize <transfer size>",
    "transfer size (4096)");
  fprintf(stderr, " %-20s %s\n", "--depth|-D <transfer depth>",
    "send/receive queue depth (16)");
  fprintf(stderr, " %-20s %s\n", "--odp|-O",
    "enable ODP(on-demand-paging)");
  fprintf(stderr, " %-20s %s\n", "--(i)ter <num_iter>",
    "number of iterations (1000)");
//  fprintf(stderr, " %-20s %s\n", "--buf(o)fst",
//    "use buffer offset for RDMA. By default, we use virtual address for RDMA. for 'socket providers, please enable this feature.'");
  fprintf(stderr, " %-20s %s\n", "--cliport|-C <client port>",
    "the port number used by the client (10055)");
  fprintf(stderr, " %-20s %s\n", "--svrport|-S <server port>",
    "the port number used by the server (10056)");
}

int parse_arguments(struct lfpf_ctxt *ct, int argc, char ** argv) {
  int ret = 0;
  char op;
  while ((op = getopt_long(argc,argv,"hd:p:e:w:b:s:D:i:oOC:S:",lfpf_opts,NULL)) != -1) {
    switch (op) {
      /* Domain */
      case 'd':
        ct->hints->domain_attr->name = strdup(optarg);
        ct->opts.domain = optarg;
        break;

      /* Provider */
      case 'p':
        ct->hints->fabric_attr->prov_name = strdup(optarg);
        ct->opts.provider = optarg;
        break;

      /* Endpoint */
      case 'e':
        if (!strncasecmp("msg", optarg, 3) && (strlen(optarg) == 3)) {
                ct->hints->ep_attr->type = FI_EP_MSG;
        } else if (!strncasecmp("rdm", optarg, 3) &&
                   (strlen(optarg) == 3)) {
                ct->hints->ep_attr->type = FI_EP_RDM;
        } else if (!strncasecmp("dgram", optarg, 5) &&
                   (strlen(optarg) == 5)) {
                ct->hints->ep_attr->type = FI_EP_DGRAM;
        } else {
                fprintf(stderr, "Unknown endpoint : %s\n", optarg);
                exit(EXIT_FAILURE);
        }
        break;

      /* Workload */
      case 'w':
        if (!strncasecmp("rdma1", optarg, 5) && (strlen(optarg) == 5)) {
          ct->common_opts.workload = LFPFRDMA1;
        } else if (!strncasecmp("rdma2", optarg, 5) && (strlen(optarg) == 5)) {
          ct->common_opts.workload = LFPFRDMA2;
        } else if (!strncasecmp("info", optarg, 4) && (strlen(optarg) == 4)) {
          ct->common_opts.workload = LFPFINFO;
        }
        break;

      /* Buffer Size */
      case 'b':
        ct->common_opts.buf_size = atoi(optarg);
        break;

      /* Transfer Size */
      case 's':
        ct->common_opts.tx_size = ct->common_opts.rx_size = atoi(optarg);
        break;

      /* Operation Depth*/
      case 'D':
        ct->common_opts.transfer_depth = atoi(optarg);
        break;

      /* Iterations */
      case 'i':
        ct->common_opts.iterations = atoi(optarg);
        break;

      case 'o':
        ct->common_opts.use_buffer_offset = 1;
        break;

      case 'O':
        ct->opts.use_odp = 1;
        break;

      /* Client Port */
      case 'C':
        ct->opts.cli_port = atoi(optarg);
        break;

      /* Server Port */
      case 'S':
        ct->opts.svr_port = atoi(optarg);
        break;

      case '?':
      case 'h':
        ct->common_opts.workload = LFPFNONE;
        break;

      default:
        fprintf(stderr, "Unknown argument: %c\n",op);
    }
  }

  if (optind < argc)
    ct->opts.dst_addr = argv[optind];

  return ret;
}

int main(int argc, char ** argv) {
  int ret = -1;
  struct lfpf_ctxt context={}; // initialize to zero
  
  if(ret = default_context(&context)) {
    return ret;
  }
  parse_arguments(&context,argc,argv);
  switch(context.common_opts.workload) {
  case LFPFINFO:
    ret = show_endpoints(&context);
    break;
  case LFPFRDMA1:
  case LFPFRDMA2:
    run(&context);
    break;
  case LFPFNONE: 
    print_usage(argv[0],"Libfabric Performance Tool");
    break;
  default:
    fprintf(stderr,"Unknown workload:%d\n",context.common_opts.workload);
  }
  return ret;
}
