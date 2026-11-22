#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

#define BLOQUES 300
#define TAM 1024

char datos[TAM];
char lectura[TAM];

int
main(void)
{
  int fd;
  int i, j;
  char *nombre = "t2_bmap.dat";

  printf("T2: Iniciando prueba de bmap...\n");

  fd = open(nombre, O_CREATE | O_RDWR);
  if (fd < 0) {
    printf("T2 ERROR: no se pudo crear el archivo\n");
    exit(1);
  }

  for (i = 0; i < BLOQUES; i++) {
    for (j = 0; j < TAM; j++)
      datos[j] = (i + j) % 251;

    if (write(fd, datos, TAM) != TAM) {
      printf("T2 ERROR: escritura en bloque %d\n", i);
      close(fd);
      exit(1);
    }
  }

  close(fd);
  printf("T2: Escritura completada\n");

  fd = open(nombre, O_RDONLY);
  if (fd < 0) {
    printf("T2 ERROR: no se pudo abrir el archivo\n");
    exit(1);
  }

  for (i = 0; i < BLOQUES; i++) {
    if (read(fd, lectura, TAM) != TAM) {
      printf("T2 ERROR: lectura en bloque %d\n", i);
      close(fd);
      exit(1);
    }

    for (j = 0; j < TAM; j++) {
      if ((uchar)lectura[j] != (i + j) % 251) {
        printf("T2 ERROR: dato incorrecto en bloque %d\n", i);
        close(fd);
        exit(1);
      }
    }
  }

  close(fd);

  printf("T2 PASS: 300 bloques escritos y verificados\n");
  exit(0);
}
