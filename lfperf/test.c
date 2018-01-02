#include <rdma/fabric.h>
#include <rdma/fi_errno.h>
#include <stdio.h>

void print_long_info(struct fi_info *info) {
/*
  struct fi_info *cur;
  for (cur = info; cur; cur=cur->next) {
    printf("---\n");
    printf("%s", fi_tostr(cur, FI_TYPE_INFO));
  }
*/
  struct fi_info *cur;
  int cnt = 0;
  for (cur = info; cur; cur = cur->next,cnt++) {
    printf("Endpoint %d)\n",cnt);
    printf("provider: %s\n", cur->fabric_attr->prov_name);
    printf("    fabric: %s\n", cur->fabric_attr->name),
    printf("    domain: %s\n", cur->domain_attr->name),
    printf("    version: %d.%d\n", FI_MAJOR(cur->fabric_attr->prov_version),
      FI_MINOR(cur->fabric_attr->prov_version));
    printf("    type: %s\n", fi_tostr(&cur->ep_attr->type, FI_TYPE_EP_TYPE));
    printf("    protocol: %s\n", fi_tostr(&cur->ep_attr->protocol, FI_TYPE_PROTOCOL));
  }

}

int main(int argc, char **argv){
  struct fi_info *info;
  int ret = fi_getinfo(FI_VERSION(FI_MAJOR_VERSION,FI_MINOR_VERSION),
    "192.168.9.31", // node
    NULL, // service
    0, // flags
    NULL, // hints
    &info);

  if (ret) {
    fprintf(stderr, "fi_getinfo: %d\n", ret);
    return ret;
  }

  print_long_info(info);

  fi_freeinfo(info);

  printf("create info\n");
  info = fi_allocinfo();
  fi_freeinfo(info);
  printf("delete info\n");

  return 0;
}
