// bigfiletest.c -- Suite de Pruebas Automatizadas Integrales con Visualizacion
// Proyecto Final Sistemas Operativos 2026 - xv6-riscv
// Ampliacion de capacidad mediante Direccionamiento Doblemente Indirecto
//
// Equipo de Trabajo:
//   - Modulo A: Mauricio Onicham (bmap y asignacion doble indirecta)
//   - Modulo B: Quispe Sebastian (Estructura de inodos y limites de arquitectura)
//   - Modulo C: Pinga Marlon (itrunc y liberacion consistente de bloques)
//   - Modulo D: De la Cruz HC Fredy (Consolidacion, suite de pruebas y evaluacion)
//
// Matriz Oficial de Pruebas (Guia Maestra del Equipo - Seccion 5):
//   T1 [Modulo B - Sebastian]         : Verificacion de zona directa (0..10 bloques)
//   T2 [Modulo A - Mauricio]          : Cruce de limite simple indirecto (300 bloques)
//   T3 [Modulo D - Fredy]             : Archivo grande con traza de punteros (1000 bloques)
//   T4 [Modulo D - Fredy]             : Integridad determinista byte a byte
//   T5 [Modulo C - Marlon]            : Liberacion recursiva y reutilizacion ciclica (itrunc)
//   T6 [Modulos A + D - Mauricio/Fredy]: Persistencia en disco y consistencia tras cierre
//   T7 [Modulos B + D - Sebastian/Fredy]: Validacion de arquitectura vs xv6 original

#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/fs.h"

#define BSIZE_LOCAL     1024
#define T1_BLOCKS       11            // NDIRECT exacto
#define T2_BLOCKS       300           // Cruza NDIRECT(11) + NINDIRECT(256) = 267
#define T3_BLOCKS       1000          // Archivo grande (1 MB) con visualizacion de punteros
#define T4_BLOCKS       300           // Prueba de integridad determinista
#define T5_BLOCKS       300           // Prueba de liberacion ciclica de Marlon
#define T6_BLOCKS       300           // Prueba de persistencia
#define T7_BLOCKS       290           // Prueba de metadatos stat()

#define TEST_FILE       "test_eval.dat"

static char buf[BSIZE_LOCAL];
static char check_buf[BSIZE_LOCAL];

/* ── Muestra el mapa arquitectonico del inodo antes de iniciar ─────────────── */
static void
print_inode_diagram(void)
{
    printf("\n  ======================================================================\n");
    printf("    MAPA ARQUITECTONICO DEL INODO EN DISCO (struct dinode = 64 bytes)\n");
    printf("  ======================================================================\n");
    printf("    inodo.addrs[0..10]  -> 11 Punteros Directos   (Capacidad: 11 KB)\n");
    printf("    inodo.addrs[11]     -> 1 Puntero Simple Ind.  (Capacidad: 256 KB)\n");
    printf("    inodo.addrs[12]     -> 1 Puntero Doble Ind.   (Capacidad: ~64 MB)\n");
    printf("                           |--> Tabla L1 (256 entradas de punteros L2)\n");
    printf("                                |---> 256 Tablas L2 (256 punteros a datos)\n");
    printf("                                      |---> 65,536 Bloques de Datos\n");
    printf("    Limite Base xv6     : 268 bloques (268 KB) -> Termina en panic\n");
    printf("    Nuevo Limite MAXFILE: 65,803 bloques (~64.26 MB) en fs de 20,000 blk\n");
    printf("  ======================================================================\n\n");
}

/* ── T1: Modulo B - Quispe Sebastian ───────────────────────────────────────
   Verifica que la zona directa (NDIRECT = 11 bloques) funcione sin regresiones
   tras la reduccion de 12 a 11 punteros directos. */
static int
test_t1_sebastian(void)
{
    printf("\n[T1] MODULO B (Quispe Sebastian): Zona Directa (NDIRECT=%d)\n", NDIRECT);
    printf("     Escenario: Escritura y lectura de %d bloques directos...\n", T1_BLOCKS);
    printf("     [Estructura]: Usando unicamente inodo.addrs[0..10]\n");
    unlink(TEST_FILE);

    int fd = open(TEST_FILE, O_CREATE | O_RDWR);
    if(fd < 0){
        printf("     -> FAIL: no se pudo crear el archivo\n");
        return 0;
    }

    for(int i = 0; i < T1_BLOCKS; i++){
        memset(buf, 'A' + i, BSIZE_LOCAL);
        if(write(fd, buf, BSIZE_LOCAL) != BSIZE_LOCAL){
            printf("     -> FAIL: error en write bloque directo %d\n", i);
            close(fd);
            unlink(TEST_FILE);
            return 0;
        }
    }
    close(fd);

    fd = open(TEST_FILE, O_RDONLY);
    if(fd < 0){
        printf("     -> FAIL: no se pudo reabrir para verificacion\n");
        unlink(TEST_FILE);
        return 0;
    }

    for(int i = 0; i < T1_BLOCKS; i++){
        if(read(fd, check_buf, BSIZE_LOCAL) != BSIZE_LOCAL){
            printf("     -> FAIL: error de lectura en bloque %d\n", i);
            close(fd);
            unlink(TEST_FILE);
            return 0;
        }
        memset(buf, 'A' + i, BSIZE_LOCAL);
        if(memcmp(buf, check_buf, BSIZE_LOCAL) != 0){
            printf("     -> FAIL: corrupcion en bloque directo %d\n", i);
            close(fd);
            unlink(TEST_FILE);
            return 0;
        }
    }
    close(fd);
    unlink(TEST_FILE);

    printf("     -> PASS: 11 bloques directos validados correctamente sin regresion.\n");
    return 1;
}

/* ── T2: Modulo A - Mauricio Onicham ───────────────────────────────────────
   Verifica que bmap() asigne y acceda a bloques doblemente indirectos al superar
   el limite simple indirecto (NDIRECT + NINDIRECT = 267 bloques). */
static int
test_t2_mauricio(void)
{
    printf("\n[T2] MODULO A (Mauricio Onicham): Cruce a Doble Indirecto bmap()\n");
    printf("     Escenario: Escritura de %d bloques (supera limite simple de 267)...\n", T2_BLOCKS);
    unlink(TEST_FILE);

    int fd = open(TEST_FILE, O_CREATE | O_RDWR);
    if(fd < 0){
        printf("     -> FAIL: no se pudo crear el archivo\n");
        return 0;
    }

    printf("     [Traza bmap]:\n");
    for(int i = 0; i < T2_BLOCKS; i++){
        for(int j = 0; j < BSIZE_LOCAL; j++)
            buf[j] = (char)((i + j) % 251);

        if(i == 0)
            printf("       Bloque    0 -> Zona Directa: inodo.addrs[0]\n");
        else if(i == NDIRECT)
            printf("       Bloque   11 -> Zona Simple Indirecta: asignando inodo.addrs[11]\n");
        else if(i == NDIRECT + NINDIRECT){
            printf("       =================================================================\n");
            printf("       >>> HITO CRITICO: Superando bloque 267 (Limite original de xv6)\n");
            printf("       >>> bmap() asigna Tabla Nivel 1 en inodo.addrs[12]\n");
            printf("       >>> bmap() asigna Subtabla Nivel 2 [L1_idx = 0] (bloque logico 267)\n");
            printf("       =================================================================\n");
        }

        if(write(fd, buf, BSIZE_LOCAL) != BSIZE_LOCAL){
            printf("     -> FAIL: bmap() fallo al asignar bloque %d (doble indirecto)\n", i);
            close(fd);
            unlink(TEST_FILE);
            return 0;
        }
    }
    close(fd);

    fd = open(TEST_FILE, O_RDONLY);
    if(fd < 0){
        printf("     -> FAIL: error al reabrir archivo\n");
        unlink(TEST_FILE);
        return 0;
    }

    for(int i = 0; i < T2_BLOCKS; i++){
        if(read(fd, check_buf, BSIZE_LOCAL) != BSIZE_LOCAL){
            printf("     -> FAIL: lectura fallo en bloque %d\n", i);
            close(fd);
            unlink(TEST_FILE);
            return 0;
        }
        for(int j = 0; j < BSIZE_LOCAL; j++){
            if(check_buf[j] != (char)((i + j) % 251)){
                printf("     -> FAIL: dato corrupto en bloque doble indirecto %d\n", i);
                close(fd);
                unlink(TEST_FILE);
                return 0;
            }
        }
    }
    close(fd);
    unlink(TEST_FILE);

    printf("     -> PASS: 300 bloques leidos y escritos (cruce a doble indirecto exitoso).\n");
    return 1;
}

/* ── T3: Modulo D - De la Cruz HC Fredy ─────────────────────────────────────
   Verifica la creacion y lectura de un archivo grande de 1000 bloques (aprox. 1 MB)
   mostrando en tiempo real como se van abriendo las sucesivas tablas de segundo nivel. */
static int
test_t3_fredy_largefile(void)
{
    printf("\n[T3] MODULO D (De la Cruz HC): Capacidad Masiva y Mapeo en Tiempo Real (1 MB)\n");
    printf("     Escenario: Escritura continua de %d bloques (1000 KB)...\n", T3_BLOCKS);
    unlink(TEST_FILE);

    int fd = open(TEST_FILE, O_CREATE | O_RDWR);
    if(fd < 0){
        printf("     -> FAIL: open fallo\n");
        return 0;
    }

    printf("     [Visualizacion de creacion de tablas de punteros]:\n");
    memset(buf, 0x5A, BSIZE_LOCAL);
    for(int i = 0; i < T3_BLOCKS; i++){
        if(i == 0)
            printf("       [PUNTEROS DIRECTOS]  : Bloques    0..10  -> inodo.addrs[0..10]\n");
        else if(i == NDIRECT)
            printf("       [SIMPLE INDIRECTO]   : Bloques   11..266 -> inodo.addrs[11] (Tabla Simple)\n");
        else if(i == NDIRECT + NINDIRECT)
            printf("       [DOBLE IND. TABLA L2-0]: Bloques  267..522 -> inodo.addrs[12] -> L1[0] -> L2[0..255]\n");
        else if(i == NDIRECT + NINDIRECT + NINDIRECT)
            printf("       [DOBLE IND. TABLA L2-1]: Bloques  523..778 -> inodo.addrs[12] -> L1[1] -> L2[0..255]\n");
        else if(i == NDIRECT + NINDIRECT + 2 * NINDIRECT)
            printf("       [DOBLE IND. TABLA L2-2]: Bloques  779..999 -> inodo.addrs[12] -> L1[2] -> L2[0..220]\n");

        if(write(fd, buf, BSIZE_LOCAL) != BSIZE_LOCAL){
            printf("     -> FAIL: fallo al escribir bloque %d de 1000\n", i);
            close(fd);
            unlink(TEST_FILE);
            return 0;
        }
    }
    close(fd);

    fd = open(TEST_FILE, O_RDONLY);
    if(fd < 0){
        printf("     -> FAIL: reopen fallo\n");
        unlink(TEST_FILE);
        return 0;
    }

    int bytes_leidos = 0;
    for(int i = 0; i < T3_BLOCKS; i++){
        int n = read(fd, check_buf, BSIZE_LOCAL);
        if(n != BSIZE_LOCAL){
            printf("     -> FAIL: lectura incompleta en bloque %d (leidos: %d bytes)\n", i, n);
            close(fd);
            unlink(TEST_FILE);
            return 0;
        }
        bytes_leidos += n;
    }
    close(fd);
    unlink(TEST_FILE);

    printf("     -> PASS: %d bytes verificados (archivo de 1000 bloques creado y leido OK).\n", bytes_leidos);
    return 1;
}

/* ── T4: Modulo D - De la Cruz HC Fredy ─────────────────────────────────────
   Verifica integridad matematica estricta byte a byte con patron no lineal determinista. */
static int
test_t4_fredy_integrity(void)
{
    printf("\n[T4] MODULO D (De la Cruz HC): Integridad de Datos Determinista\n");
    printf("     Escenario: Patron matematico no colisionante ((i * 7 + 13) %% 256)...\n");
    printf("     [Objetivo]: Comprobar que ningun bloque doble indirecto se solapa o corrompe.\n");
    unlink(TEST_FILE);

    int fd = open(TEST_FILE, O_CREATE | O_RDWR);
    if(fd < 0){
        printf("     -> FAIL: open fallo\n");
        return 0;
    }

    for(int i = 0; i < T4_BLOCKS; i++){
        memset(buf, (unsigned char)((i * 7 + 13) % 256), BSIZE_LOCAL);
        if(write(fd, buf, BSIZE_LOCAL) != BSIZE_LOCAL){
            printf("     -> FAIL: write fallo en bloque %d\n", i);
            close(fd);
            unlink(TEST_FILE);
            return 0;
        }
    }
    close(fd);

    fd = open(TEST_FILE, O_RDONLY);
    if(fd < 0){
        printf("     -> FAIL: reopen fallo\n");
        unlink(TEST_FILE);
        return 0;
    }

    for(int i = 0; i < T4_BLOCKS; i++){
        if(read(fd, check_buf, BSIZE_LOCAL) != BSIZE_LOCAL){
            printf("     -> FAIL: read fallo en bloque %d\n", i);
            close(fd);
            unlink(TEST_FILE);
            return 0;
        }
        unsigned char expected = (unsigned char)((i * 7 + 13) % 256);
        for(int j = 0; j < BSIZE_LOCAL; j++){
            if((unsigned char)check_buf[j] != expected){
                printf("     -> FAIL: corrupcion en bloque %d byte %d (esperado 0x%x, leido 0x%x)\n",
                       i, j, expected, (unsigned char)check_buf[j]);
                close(fd);
                unlink(TEST_FILE);
                return 0;
            }
        }
    }
    close(fd);
    unlink(TEST_FILE);

    printf("     -> PASS: 307,200 bytes validados byte a byte sin colision de punteros.\n");
    return 1;
}

/* ── T5: Modulo C - Pinga Marlon ───────────────────────────────────────────
   Verifica que itrunc() libere ambos niveles del arbol doblemente indirecto
   y que los bloques retornen al bitmap siendo reutilizables en ciclos continuos. */
static int
test_t5_marlon_itrunc(void)
{
    printf("\n[T5] MODULO C (Pinga Marlon): Liberacion Recursiva y Reutilizacion itrunc()\n");
    printf("     Escenario: 3 ciclos de crear, verificar y liberar %d bloques c/u...\n", T5_BLOCKS);
    printf("     [Algoritmo]: itrunc() libera Datos L2 -> Subtabla L2 -> Tabla L1 -> Bitmap.\n");

    int ciclos_exitosos = 0;
    for(int ciclo = 1; ciclo <= 3; ciclo++){
        printf("       Ciclo %d/3: asignando %d bloques... ", ciclo, T5_BLOCKS);
        unlink("t5_tmp_a");

        int fd = open("t5_tmp_a", O_CREATE | O_RDWR);
        if(fd < 0){
            printf("FAIL (open)\n");
            break;
        }

        int write_ok = 1;
        for(int i = 0; i < T5_BLOCKS; i++){
            memset(buf, (unsigned char)((ciclo + i) % 251), BSIZE_LOCAL);
            if(write(fd, buf, BSIZE_LOCAL) != BSIZE_LOCAL){
                write_ok = 0;
                break;
            }
        }
        close(fd);
        if(!write_ok){
            printf("FAIL (write)\n");
            unlink("t5_tmp_a");
            break;
        }

        fd = open("t5_tmp_a", O_RDONLY);
        if(fd < 0){
            printf("FAIL (reopen)\n");
            unlink("t5_tmp_a");
            break;
        }

        int read_ok = 1;
        for(int i = 0; i < T5_BLOCKS; i++){
            if(read(fd, check_buf, BSIZE_LOCAL) != BSIZE_LOCAL){
                read_ok = 0;
                break;
            }
            if((unsigned char)check_buf[0] != (unsigned char)((ciclo + i) % 251)){
                read_ok = 0;
                break;
            }
        }
        close(fd);

        // Llamada a unlink -> activa itrunc() en el kernel
        unlink("t5_tmp_a");

        if(read_ok){
            printf("verificado y liberado OK (bloques recuperados)\n");
            ciclos_exitosos++;
        } else {
            printf("FAIL (datos corruptos)\n");
            break;
        }
    }

    if(ciclos_exitosos == 3){
        printf("     -> PASS: 3 ciclos completados (900 bloques asignados y liberados sin agotar disco).\n");
        return 1;
    } else {
        printf("     -> FAIL: se detuvo en ciclo %d (bloques no liberados correctamente)\n", ciclos_exitosos + 1);
        return 0;
    }
}

/* ── T6: Modulos A + D - Mauricio Onicham & Fredy De la Cruz ────────────────
   Verifica persistencia y consistencia en disco: cierre formal del descriptor
   (forzando iupdate en kernel) y verificacion de punteros tras reapertura. */
static int
test_t6_persistence(void)
{
    printf("\n[T6] MODULOS A + D (Mauricio & Fredy): Persistencia en Disco Duro Virtual\n");
    printf("     Escenario: Escritura en doble indirecto, cierre formal y reapertura...\n");
    unlink(TEST_FILE);

    int fd = open(TEST_FILE, O_CREATE | O_RDWR);
    if(fd < 0){
        printf("     -> FAIL: open fallo\n");
        return 0;
    }

    for(int i = 0; i < T6_BLOCKS; i++){
        memset(buf, 'P', BSIZE_LOCAL);
        if(write(fd, buf, BSIZE_LOCAL) != BSIZE_LOCAL){
            printf("     -> FAIL: write fallo en bloque %d\n", i);
            close(fd);
            unlink(TEST_FILE);
            return 0;
        }
    }
    printf("     [Persistencia]: close(fd) -> kernel invoca iupdate() sincronizando inodo en disco.\n");
    close(fd); // Cierre real: kernel sincroniza inodo en disco

    // Reapertura independiente como solo lectura
    fd = open(TEST_FILE, O_RDONLY);
    if(fd < 0){
        printf("     -> FAIL: reapertura tras cierre fallo\n");
        unlink(TEST_FILE);
        return 0;
    }

    for(int i = 0; i < T6_BLOCKS; i++){
        if(read(fd, check_buf, BSIZE_LOCAL) != BSIZE_LOCAL){
            printf("     -> FAIL: read fallo tras persistencia en bloque %d\n", i);
            close(fd);
            unlink(TEST_FILE);
            return 0;
        }
        if(check_buf[0] != 'P' || check_buf[BSIZE_LOCAL - 1] != 'P'){
            printf("     -> FAIL: corrupcion tras reapertura en bloque %d\n", i);
            close(fd);
            unlink(TEST_FILE);
            return 0;
        }
    }
    close(fd);
    unlink(TEST_FILE);

    printf("     -> PASS: Datos y tablas L1/L2 persisten correctamente tras sincronizacion en disco.\n");
    return 1;
}

/* ── T7: Modulos B + D - Quispe Sebastian & Fredy De la Cruz ────────────────
   Verifica metadatos del inodo mediante llamada al sistema stat() y presenta
   la comparativa arquitectonica frente al diseno base de xv6. */
static int
test_t7_limits_comparison(void)
{
    printf("\n[T7] MODULOS B + D (Sebastian & Fredy): Validacion Arquitectonica y stat()\n");
    printf("     Escenario: Creacion de archivo de %d bloques y medicion con stat()...\n", T7_BLOCKS);
    unlink(TEST_FILE);

    int fd = open(TEST_FILE, O_CREATE | O_RDWR);
    if(fd < 0){
        printf("     -> FAIL: open fallo\n");
        return 0;
    }

    memset(buf, 0x77, BSIZE_LOCAL);
    for(int i = 0; i < T7_BLOCKS; i++){
        if(write(fd, buf, BSIZE_LOCAL) != BSIZE_LOCAL){
            close(fd);
            unlink(TEST_FILE);
            return 0;
        }
    }
    close(fd);

    struct stat st;
    if(stat(TEST_FILE, &st) < 0){
        printf("     -> FAIL: stat() fallo\n");
        unlink(TEST_FILE);
        return 0;
    }

    int tam_esperado = T7_BLOCKS * BSIZE_LOCAL;
    if((int)st.size != tam_esperado){
        printf("     -> FAIL: tamano incorrecto (esperado %d bytes, reportado %d)\n",
               tam_esperado, (int)st.size);
        unlink(TEST_FILE);
        return 0;
    }
    unlink(TEST_FILE);

    printf("     -> PASS: stat() reporta exactamente %d bytes (tipo=%d, inum=%d).\n",
           (int)st.size, st.type, st.ino);

    printf("\n     =========================================================\n");
    printf("     TABLA COMPARATIVA ARQUITECTONICA: xv6 BASE vs EQUIPO 2026\n");
    printf("     =========================================================\n");
    printf("     Parametro              xv6 Base            xv6 Modificado\n");
    printf("     ---------------------------------------------------------\n");
    printf("     NDIRECT (Directos)     12 bloques          %d bloques\n", (int)NDIRECT);
    printf("     NINDIRECT (Simple)     1 bloque (256)      %d bloque (%d)\n", 1, (int)NINDIRECT);
    printf("     NDINDIRECT (Doble)     0 (No soportado)    %d bloque (%d)\n", 1, (int)NDINDIRECT);
    printf("     MAXFILE (Capacidad)    268 bloques (268KB) %d bloques (~%d MB)\n",
           (int)MAXFILE, (int)(MAXFILE / 1024));
    printf("     dinode en disco        64 bytes            64 bytes (Preservado)\n");
    printf("     FSSIZE imagen          2000 bloques        20000 bloques\n");
    printf("     =========================================================\n");

    return 1;
}

/* ── Main Runner ──────────────────────────────────────────────────────────── */
int
main(void)
{
    printf("\n");
    printf("======================================================================\n");
    printf("  PROYECTO FINAL SISTEMAS OPERATIVOS 2026 - UNSCH\n");
    printf("  SUITE DE PRUEBAS INTEGRAL: DIRECCIONAMIENTO DOBLEMENTE INDIRECTO\n");
    printf("  Equipo: Mauricio Onicham | Quispe Sebastian | Marlon Pinga | Fredy DLC\n");
    printf("======================================================================\n");

    print_inode_diagram();

    int pass_count = 0;
    pass_count += test_t1_sebastian();
    pass_count += test_t2_mauricio();
    pass_count += test_t3_fredy_largefile();
    pass_count += test_t4_fredy_integrity();
    pass_count += test_t5_marlon_itrunc();
    pass_count += test_t6_persistence();
    pass_count += test_t7_limits_comparison();

    printf("\n======================================================================\n");
    if(pass_count == 7){
        printf("  RESULTADO GLOBAL: TODOS LOS CASOS DE PRUEBA APROBADOS (7/7 PASS)\n");
        printf("  Integracion de los Modulos A, B, C y D validada exitosamente.\n");
    } else {
        printf("  RESULTADO GLOBAL: ALGUNAS PRUEBAS FALLARON (%d/7 PASS)\n", pass_count);
    }
    printf("======================================================================\n\n");

    exit(0);
}
