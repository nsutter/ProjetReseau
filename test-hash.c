#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gcrypt.h>
#include "hash-file.h"

int main(int argc, char *argv[])
{
  if(argc != 2)
  {
    exit(1);
  }

  printf("%s : %s\n", argv[1], hashFichier(argv[1]));

  int nb;

  char ** res = hashAllChunks(argv[1], &nb);

  for(int i = 0; i < nb; i++)
  {
    printf("CHUNK %d : %s\n", i, res[i]);
  }
}
