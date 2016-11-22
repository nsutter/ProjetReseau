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

#define BUF_SIZE 1024

//associe a un hash une liste d'ip + port
typedef struct {char * hash; char * addr; int port;} association;

void erreur(char *msg)
{
  printf("%s \n", msg);
  perror("");
  exit(-1);
}

static association * tableau;
static int longueur=0;

// renvoi 1 si le couple hash ip existe déjà
int test_existance(char * ad, int p, char * hash)
{
  int i;
  for(i=0; i<longueur; i++)
  {
    if(strcmp(tableau[i].hash, hash) == 0 && strcmp(tableau[i].addr, ad) == 0 && tableau[i].port == p)
      return 1;
  }
  return 0;
}

void ajout(char * ad, int p, char * hash)
{
  if(test_existance(ad, p, hash) == 1)
  {
    longueur= longueur +1;
    tableau= realloc(tableau, longueur * sizeof(association));
    tableau[longueur-1].hash= hash;
    tableau[longueur-1].addr= ad;
    tableau[longueur-1].port= p;
    free(hash);
  }
  else
  {
    free(ad);
    free(hash);
  }
}

char * concat(char* str1, char * str2)
{
  int lg1= strlen(str1);
  int lg2= strlen(str2);
  char * res= malloc(sizeof((lg1+lg2+2)*sizeof(char)));
  int i;
  for(i=0; i<lg1; i++)
  {
    res[i]=str1[i];
  }
  res[lg1]=' ';
  for(i=1; i<=lg2; i++)
  {
    res[i+lg1]= str2[i-1];
  }
  res[lg1+lg2+1]= '\0';
  return res;
}

int main(int argc, char **argv)
{
  int sockfd;
  char buf[BUF_SIZE];
  socklen_t addrlen;

  struct sockaddr_in6 my_addr; // in = internet
  struct sockaddr_in6 client;

  // check the number of args on command line
  if(argc != 3)
  {
    printf("nombre d'arguments insuffisants \n");
    exit(-1);
  }

  // socket factory
  if((sockfd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP)) == -1) // d'après man et pdf
  {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  // init local addr structure and other params
  my_addr.sin6_family      = AF_INET6;
  my_addr.sin6_port        = htons(atoi(argv[2])); // transformation en uint16_t
  addrlen                  = sizeof(struct sockaddr_in6);

  if(strcmp(argv[1], "localhost") == 0) my_addr.sin6_addr = in6addr_any;
  else
  {
    if(inet_pton(AF_INET6, argv[1], &(my_addr.sin6_addr)) != 1) erreur("inet_pton");
  }


  char str[INET6_ADDRSTRLEN];
  if (inet_ntop(AF_INET6, &(my_addr.sin6_addr), str, INET6_ADDRSTRLEN) == NULL) {
    perror("inet_ntop");
    exit(EXIT_FAILURE);
  }

  memset(buf,'\0',BUF_SIZE);

  // bind addr structure with socket
  if(bind(sockfd, (struct sockaddr *) &my_addr, addrlen) == -1)
  {
    perror("bind");
    close(sockfd);
    exit(EXIT_FAILURE);
  }

  printf("listening on %s port %s\n", str, argv[2]);

  while(42)
  {
    // reception de la chaine de caracteres
    if(recvfrom(sockfd, buf, BUF_SIZE, 0, (struct sockaddr *) &client, &addrlen) == -1){ erreur("recvfrom"); }

    if(strncmp(buf, "put ", 4) == 0)
    {
      char * hash= malloc((strlen(buf)-4)*sizeof(char));
      int i;
      for(i=4; buf[i] != '\0' && buf[i]!= '\0'; i++)
      {
        hash[i-4]= buf[i];
        hash[i-3]= '\0';
      }
      char * msg=concat("ACK_PUT", hash);
      if(sendto(sockfd, msg, strlen(msg), 0, (struct sockaddr *) &client, addrlen) == -1){ erreur("sendto"); }
      free(msg);
      int p;
      char * ad= malloc(sizeof(INET6_ADDRSTRLEN));
      if (inet_ntop(AF_INET6, &(client.sin6_addr), ad, INET6_ADDRSTRLEN) == NULL) {
        perror("inet_ntop");
        exit(EXIT_FAILURE);
      }
      p= ntohs(client.sin6_port);
      ajout(ad, p, hash);
    }
    else if(strncmp(buf, "get ", 4) == 0)
    {
      char * hash= malloc((strlen(buf)-4)*sizeof(char));
      int i;
      for(i=3; buf[i] != '\0' && buf[i]!= '\0'; i++)
      {
        hash[i-3]= buf[i];
        hash[i+1]= '\0';
      }

      free(hash);
    }

    // print the received char
    printf("%s\n", buf);
    int i;
    for(i=0; i<1024; i++){buf[i]='\0';}
  }

  // close the socket
  close(sockfd);

  return 0;
}
