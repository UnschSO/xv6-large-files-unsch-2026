// bigfiletest.c -- Suite de pruebas T1-T7 para doble indirecto en xv6-riscv
// Modulo D: De la Cruz HC (fredydlc)
// Proyecto final Sistemas Operativos 2026
//
// T1 - Escritura/lectura en zona directa         (20 bloques)
// T2 - Cruce del limite simple->doble indirecto  (290 bloques)
// T3 - Integridad con patron por indice de bloque(290 bloques)
// T4 - Persistencia tras cierre y reapertura     (290 bloques)
// T5 - Liberacion y reutilizacion (10 ciclos)    (600 bloques c/u)
// T6 - Escritura profunda en doble indirecto     (767 bloques)
// T7 - Verificacion de tamano con stat()         (290 bloques)

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

#define BSIZE_LOCAL    1024
#define NBLOCKS_SMALL  20
#define NBLOCKS_MID    290
#define NBLOCKS_LARGE  600
#define NBLOCKS_DEEP   (NDIRECT + NINDIRECT + 500)
#define TESTFILE       "bigtest.tmp"

/* Escribe nblocks bloques de BSIZE bytes con patron = (base + i) % 256.
   Retorna -1 si todo OK, o el numero de bloque donde fallo. */
static int
write_blocks(int fd, int nblocks, char base)
{
    char buf[BSIZE_LOCAL];
    for (int i = 0; i < nblocks; i++) {
        memset(buf, (unsigned char)((base + i) % 256), BSIZE_LOCAL);
        if (write(fd, buf, BSIZE_LOCAL) != BSIZE_LOCAL)
            return i;
    }
    return -1;
}

/* Verifica nblocks bloques con mismo patron. Retorna -1 si OK, o bloque donde fallo. */
static int
verify_blocks(int fd, int nblocks, char base)
{
    char buf[BSIZE_LOCAL];
    char expected[BSIZE_LOCAL];
    for (int i = 0; i < nblocks; i++) {
        memset(expected, (unsigned char)((base + i) % 256), BSIZE_LOCAL);
        if (read(fd, buf, BSIZE_LOCAL) != BSIZE_LOCAL)
            return i;
        if (memcmp(buf, expected, BSIZE_LOCAL) != 0)
            return i;
    }
    return -1;
}

/* ── T1: Escritura y lectura en zona directa (20 bloques) ─────────────────── */
static void
t1_direct(void)
{
    printf("T1 [Zona directa, %d bloques]: ", NBLOCKS_SMALL);
    unlink(TESTFILE);
    int fd = open(TESTFILE, 0x202);
    if (fd < 0) { printf("FAIL (open)\n"); return; }

    if (write_blocks(fd, NBLOCKS_SMALL, 'A') >= 0) {
        close(fd); unlink(TESTFILE);
        printf("FAIL (write)\n"); return;
    }
    close(fd);

    fd = open(TESTFILE, 0);
    if (fd < 0) { unlink(TESTFILE); printf("FAIL (reopen)\n"); return; }

    int r = verify_blocks(fd, NBLOCKS_SMALL, 'A');
    close(fd);
    unlink(TESTFILE);
    printf(r < 0 ? "PASS\n" : "FAIL (datos incorrectos en bloque %d)\n", r);
}

/* ── T2: Cruce limite simple->doble indirecto (290 bloques) ──────────────── */
static void
t2_cross_indirect(void)
{
    printf("T2 [Cruce limite, %d bloques]: ", NBLOCKS_MID);
    unlink(TESTFILE);
    int fd = open(TESTFILE, 0x202);
    if (fd < 0) { printf("FAIL (open)\n"); return; }

    int r = write_blocks(fd, NBLOCKS_MID, 'B');
    close(fd);
    if (r >= 0) {
        unlink(TESTFILE);
        printf("FAIL (write fallo en bloque %d)\n", r); return;
    }

    fd = open(TESTFILE, 0);
    if (fd < 0) { unlink(TESTFILE); printf("FAIL (reopen)\n"); return; }
    r = verify_blocks(fd, NBLOCKS_MID, 'B');
    close(fd);
    unlink(TESTFILE);
    printf(r < 0 ? "PASS\n" : "FAIL (datos incorrectos en bloque %d)\n", r);
}

/* ── T3: Integridad con patron indexado (290 bloques) ────────────────────── */
static void
t3_integrity(void)
{
    char buf[BSIZE_LOCAL];
    char expected[BSIZE_LOCAL];
    printf("T3 [Integridad patron i*7+13, %d bloques]: ", NBLOCKS_MID);
    unlink(TESTFILE);
    int fd = open(TESTFILE, 0x202);
    if (fd < 0) { printf("FAIL (open)\n"); return; }

    for (int i = 0; i < NBLOCKS_MID; i++) {
        memset(buf, (unsigned char)((i * 7 + 13) % 256), BSIZE_LOCAL);
        if (write(fd, buf, BSIZE_LOCAL) != BSIZE_LOCAL) {
            close(fd); unlink(TESTFILE);
            printf("FAIL (write bloque %d)\n", i); return;
        }
    }
    close(fd);

    fd = open(TESTFILE, 0);
    if (fd < 0) { unlink(TESTFILE); printf("FAIL (reopen)\n"); return; }

    for (int i = 0; i < NBLOCKS_MID; i++) {
        memset(expected, (unsigned char)((i * 7 + 13) % 256), BSIZE_LOCAL);
        if (read(fd, buf, BSIZE_LOCAL) != BSIZE_LOCAL) {
            close(fd); unlink(TESTFILE);
            printf("FAIL (read bloque %d)\n", i); return;
        }
        if (memcmp(buf, expected, BSIZE_LOCAL) != 0) {
            close(fd); unlink(TESTFILE);
            printf("FAIL (corrupcion en bloque %d, esperado 0x%x)\n",
                   i, (unsigned char)((i * 7 + 13) % 256)); return;
        }
    }
    close(fd);
    unlink(TESTFILE);
    printf("PASS\n");
}

/* ── T4: Persistencia tras cierre y reapertura (290 bloques) ─────────────── */
static void
t4_persistence(void)
{
    printf("T4 [Persistencia close/reopen, %d bloques]: ", NBLOCKS_MID);
    unlink(TESTFILE);
    int fd = open(TESTFILE, 0x202);
    if (fd < 0) { printf("FAIL (open)\n"); return; }

    if (write_blocks(fd, NBLOCKS_MID, 'D') >= 0) {
        close(fd); unlink(TESTFILE);
        printf("FAIL (write)\n"); return;
    }
    close(fd);  /* cierre real */

    fd = open(TESTFILE, 0);  /* segunda apertura independiente */
    if (fd < 0) { unlink(TESTFILE); printf("FAIL (reopen)\n"); return; }

    int r = verify_blocks(fd, NBLOCKS_MID, 'D');
    close(fd);
    unlink(TESTFILE);
    printf(r < 0 ? "PASS\n" : "FAIL (datos no persistieron, bloque %d)\n", r);
}

/* ── T5: Liberacion y reutilizacion - 10 ciclos (600 bloques c/u) ─────────
   Integrado del modulo C de Pinga Marlon (commit f41c6a9)              */
static void
t5_free_reuse(void)
{
    printf("T5 [Liberacion/reutilizacion, 10 ciclos x %d bloques]: ", NBLOCKS_LARGE);
    int ciclos_ok = 0;
    for (int c = 0; c < 10; c++) {
        unlink(TESTFILE);
        int fd = open(TESTFILE, 0x202);
        if (fd < 0) break;

        int r = write_blocks(fd, NBLOCKS_LARGE, (char)c);
        close(fd);
        if (r >= 0) break;

        fd = open(TESTFILE, 0);
        if (fd < 0) break;
        r = verify_blocks(fd, NBLOCKS_LARGE, (char)c);
        close(fd);
        unlink(TESTFILE);  /* liberar bloques */
        if (r < 0) ciclos_ok++;
        else break;
    }
    if (ciclos_ok == 10)
        printf("PASS (10 ciclos de liberacion y reutilizacion correctos)\n");
    else
        printf("FAIL (solo %d/10 ciclos correctos)\n", ciclos_ok);
}

/* ── T6: Escritura profunda en zona doblemente indirecta (767 bloques) ───── */
static void
t6_deep_double(void)
{
    int nb = NBLOCKS_DEEP;  /* NDIRECT + NINDIRECT + 500 = 767 */
    printf("T6 [Doble indirecto profundo, %d bloques]: ", nb);
    unlink(TESTFILE);
    int fd = open(TESTFILE, 0x202);
    if (fd < 0) { printf("FAIL (open)\n"); return; }

    int r = write_blocks(fd, nb, 'F');
    close(fd);
    if (r >= 0) {
        unlink(TESTFILE);
        printf("FAIL (write fallo en bloque %d)\n", r); return;
    }

    fd = open(TESTFILE, 0);
    if (fd < 0) { unlink(TESTFILE); printf("FAIL (reopen)\n"); return; }
    r = verify_blocks(fd, nb, 'F');
    close(fd);
    unlink(TESTFILE);
    printf(r < 0 ? "PASS\n" : "FAIL (datos incorrectos en bloque %d)\n", r);
}

/* ── T7: Verificacion de tamano con stat() (290 bloques) ─────────────────── */
static void
t7_stat(void)
{
    printf("T7 [Verificacion stat(), %d bloques]: ", NBLOCKS_MID);
    struct stat st;
    unlink(TESTFILE);
    int fd = open(TESTFILE, 0x202);
    if (fd < 0) { printf("FAIL (open)\n"); return; }

    if (write_blocks(fd, NBLOCKS_MID, 'G') >= 0) {
        close(fd); unlink(TESTFILE);
        printf("FAIL (write)\n"); return;
    }
    close(fd);

    if (stat(TESTFILE, &st) < 0) {
        unlink(TESTFILE); printf("FAIL (stat)\n"); return;
    }

    int expected = NBLOCKS_MID * BSIZE_LOCAL;
    unlink(TESTFILE);

    if ((int)st.size == expected)
        printf("PASS (size=%d bytes)\n", (int)st.size);
    else
        printf("FAIL (esperado %d, obtenido %d)\n", expected, (int)st.size);
}

/* ── main ─────────────────────────────────────────────────────────────────── */
int
main(void)
{
    printf("\n");
    printf("============================================================\n");
    printf("  SUITE MODULO D -- De la Cruz HC (fredydlc)\n");
    printf("  xv6-riscv: Direccionamiento Doblemente Indirecto\n");
    printf("  NDIRECT=%d  NINDIRECT=%d  MAXFILE=%d bloques (~%d MB)\n",
           (int)NDIRECT, (int)NINDIRECT, (int)MAXFILE, (int)(MAXFILE / 1024));
    printf("============================================================\n\n");

    t1_direct();
    t2_cross_indirect();
    t3_integrity();
    t4_persistence();
    t5_free_reuse();
    t6_deep_double();
    t7_stat();

    printf("\n============================================================\n");
    printf("  Suite completada -- ver resultados arriba\n");
    printf("============================================================\n\n");
    exit(0);
}
