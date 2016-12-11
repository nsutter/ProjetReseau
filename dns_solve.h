#ifndef DNS_SOLVE_H
#define DNS_SOLVE_H

struct in6_addr getip(char *);
struct addrinfo * host_to_ip(char *);
void erreur(char *);

#endif
