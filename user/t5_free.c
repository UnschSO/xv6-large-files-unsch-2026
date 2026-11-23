
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

#define TEST_BLOCKS 600
#define BLOCK_SIZE 1024

char buffer[BLOCK_SIZE];

int
crear_archivo(char *nombre)
{
  int fd = open(nombre, O_CREATE | O_RDWR);
  if(fd < 0){
    printf("T5 ERROR: no se pudo crear %s\n", nombre);
    return -1;
  }

  for(int i = 0; i < TEST_BLOCKS; i++){
    memset(buffer, i % 251, sizeof(buffer));

    if(write(fd, buffer, sizeof(buffer)) != sizeof(buffer)){
      printf("T5 ERROR: escritura en bloque %d\n", i);
      close(fd);
      unlink(nombre);
      return -1;
    }
  }

  close(fd);
  return 0;
}

int
verificar_archivo(char *nombre)
{
  int fd = open(nombre, O_RDONLY);
  if(fd < 0)
    return -1;

  for(int i = 0; i < TEST_BLOCKS; i++){
    int n = read(fd, buffer, sizeof(buffer));
    if(n != sizeof(buffer)){
      printf("T5 ERROR: lectura en bloque %d\n", i);
      close(fd);
      return -1;
    }

    for(int j = 0; j < BLOCK_SIZE; j++){
      if((unsigned char)buffer[j] != i % 251){
        printf("T5 ERROR: datos incorrectos en bloque %d\n", i);
        close(fd);
        return -1;
      }
    }
  }

  close(fd);
  return 0;
}


int
main(void)
{
  for(int ciclo = 1; ciclo <= 10; ciclo++){
    printf("T5: ciclo %d de 10\n", ciclo);

    unlink("t5_a");
    unlink("t5_b");

    if(crear_archivo("t5_a") < 0){
      printf("T5 FAIL: no se pudo crear A\n");
      exit(1);
    }

    if(verificar_archivo("t5_a") < 0){
      printf("T5 FAIL: archivo A incorrecto\n");
      exit(1);
    }

    if(unlink("t5_a") < 0){
      printf("T5 FAIL: no se pudo eliminar A\n");
      exit(1);
    }

    if(crear_archivo("t5_b") < 0){
      printf("T5 FAIL: no se pudo crear B\n");
      exit(1);
    }

    if(verificar_archivo("t5_b") < 0){
      printf("T5 FAIL: archivo B incorrecto\n");
      exit(1);
    }

    if(unlink("t5_b") < 0){
      printf("T5 FAIL: no se pudo eliminar B\n");
      exit(1);
    }

    printf("T5: ciclo %d PASS\n", ciclo);
  }

  printf("T5 PASS: 10 ciclos de liberacion y reutilizacion correctos\n");
  exit(0);
}
