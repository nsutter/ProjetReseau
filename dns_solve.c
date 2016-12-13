#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

#include "dns_solve.h"

struct addrinfo * host_to_ip(char * domain)
{
  struct addrinfo hints;
  struct addrinfo *result;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET6;
  int s;
  s= getaddrinfo(domain, NULL, &hints, &result);
  if (s != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
    exit(EXIT_FAILURE);
  }

  return result;
}

struct in6_addr getip(char * nd)
{
  int sfd;
  struct addrinfo *rp;
  struct addrinfo *result;
  struct in6_addr res;
  result = host_to_ip(nd);
  for (rp = result; rp != NULL; rp = rp->ai_next) {
    sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sfd == -1)
      continue;
    if (connect(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
    {
      struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)rp->ai_addr;
      res= ipv6->sin6_addr;
      break;
    }
    close(sfd);
  }
  if(rp == NULL)
  {
    printf("adresse non trouvée\n");
    exit(1);
  }
  close(sfd);
  freeaddrinfo(result);
  return res;
}
