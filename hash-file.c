#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gcrypt.h>

// renvoie la taille du fichier fd en octet
long tailleFichier(FILE * fd)
{
  long taille;

  if(fd)
  {
    fseek(fd, 0, SEEK_END);
    taille = ftell(fd);
    fseek(fd, 0, SEEK_SET);
  }

  return taille;
}

// renvoie le hash d'un fichier
char * hashFichier(char * fichier)
{
  // RECUPERATION DU FICHIER

  FILE * fd = fopen(fichier, "r");

  long taille = tailleFichier(fd);

  void * buffer = malloc(taille);

  fread(buffer, 1, taille, fd); // buffer contient maintenant les données du fichier

  // CALCUL DU HASH

  unsigned char hash[32]; // hash calculé (taille fixe car algo. fixe)

  gcry_md_hash_buffer(GCRY_MD_SHA256, hash, buffer, taille);

  // AFFICHAGE DU HASH

  char * sortie = malloc(sizeof(char) * ((32 * 2) + 1)), * p = sortie;

  int i;

  for(i = 0; i < 32; i++, p += 2)
  {
    snprintf(p, 3, "%02x", hash[i]);
  }

  fclose(fd);

  return sortie;
}

int main(int argc, char *argv[])
{
  hashFichier(argv[1]);
}
