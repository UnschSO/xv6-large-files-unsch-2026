# Proyecto Final de Sistemas Operativos: xv6-large-files (UNSCH 2026)

[![OS](https://img.shields.io/badge/OS-xv6--riscv-blue.svg)](https://github.com/mit-pdos/xv6-riscv)
[![Arch](https://img.shields.io/badge/Architecture-RISC--V-red.svg)](https://riscv.org/)
[![Status](https://img.shields.io/badge/Build-Passing-brightgreen.svg)]()
[![Tests](https://img.shields.io/badge/Tests-7%2F7%20Passed-success.svg)]()

> **Universidad Nacional de San Cristóbal de Huamanga**  
> **Facultad de Ingeniería de Minas, Geología y Civil**  
> **Escuela Profesional de Ingeniería de Sistemas**  
> **Asignatura:** Sistemas Operativos (2026)  

---

## 📌 Descripción del Proyecto

Este repositorio contiene la implementación oficial del soporte para **archivos de gran tamaño (Large Files)** en el sistema operativo educativo **xv6-riscv**. El objetivo central del proyecto es superar el límite original de 268 bloques (268 KB) por archivo mediante la incorporación de **direccionamiento doblemente indirecto** en el sistema de archivos (File System).

La solución optimiza la estructura del inodo pasando de 12 punteros directos a **11 punteros directos, 1 indirecto simple y 1 indirecto doble**, permitiendo direccionar hasta **65,803 bloques (~64.26 MB)** manteniendo estrictamente el tamaño en disco de cada inodo en **64 bytes**.

---

## 👥 Integrantes del Equipo y Asignación de Módulos

| Integrante | Módulo | Responsabilidad Principal | Archivos Clave |
| :--- | :---: | :--- | :--- |
| **TUTAYA QUISPE, JOSEMARIA SEBASTIAN** | **Módulo B** | **Estructura de Inodos:** Rediseño de constantes y sincronización de estructuras `dinode` en disco y memoria. | `kernel/fs.h`<br>`kernel/file.h` |
| **PIZARRO ACORI, ALEX MAURICIO** | **Módulo A** | **Direccionamiento en `bmap()`:** Lógica de asignación y resolución de bloques doblemente indirectos (L1 y L2). | `kernel/fs.c` |
| **INGA FLORES, MARLON STEVEEN** | **Módulo C** | **Liberación en `itrunc()`:** Deallocación recursiva de bloques de datos y tablas de punteros L1 y L2; prevención de fugas de memoria. | `kernel/fs.c` |
| **DE LA CRUZ HUARACA, FREDY JHOSMEL** | **Módulo D** | **Suite de Pruebas y Evaluación:** Diseño e implementación de la suite integral de pruebas T1–T7, integración del sistema y análisis de resultados. | `user/bigfiletest.c`<br>`Makefile`<br>`kernel/param.h` |

---

## 🏛️ Arquitectura del Inodo Extendido

### Comparativa Estructural
* **xv6 Original:** 12 directos + 1 indirecto simple ($12 + 256 = 268$ bloques $\approx 268\text{ KB}$).
* **xv6 Extendido:** 11 directos + 1 indirecto simple + 1 doble indirecto ($11 + 256 + 256 \times 256 = 65,803$ bloques $\approx 64.26\text{ MB}$).

```
Estructura en Disco (struct dinode = 64 bytes exactos):
=============================================================================
inodo.addrs[0..10]  --> 11 Punteros Directos               (Capacidad: 11 KB)
inodo.addrs[11]      --> 1 Puntero Simple Indirecto         (Capacidad: 256 KB)
                         +---> 1 Tabla con 256 punteros de datos
inodo.addrs[12]      --> 1 Puntero Doblemente Indirecto     (Capacidad: ~64 MB)
                         +---> Tabla Nivel 1 (256 punteros a Tablas L2)
                                 |---> 256 Tablas Nivel 2 (256 punteros c/u)
                                         |---> 65,536 Bloques de Datos
=============================================================================
Capacidad Teórica Máxima (MAXFILE): 65,803 bloques (~64.26 MB)
Capacidad del Sistema de Archivos configurada (FSSIZE): 20,000 bloques
```

---

## 🧪 Suite de Pruebas Automatizadas (`bigfiletest`)

Para validar rigurosamente la estabilidad y correctitud del sistema, el **Módulo D** implementó un programa en espacio de usuario (`user/bigfiletest.c`) compuesto por 7 pruebas funcionales exhaustivas:

* **`[T1]` Zona Directa (`NDIRECT=11`):** Valida la escritura y lectura exacta de los primeros 11 bloques directos (`addrs[0..10]`) verificando no regresión.
* **`[T2]` Cruce a Doble Indirecto (`bmap()`):** Escribe 300 bloques superando el límite tradicional de 268 bloques. Imprime la traza de asignación de la Tabla L1 (`addrs[12]`) y la primera Subtabla L2.
* **`[T3]` Capacidad Masiva y Mapeo en Tiempo Real:** Escribe 1,000 bloques (~1 MB) desplegando hitos de progresión a través de múltiples subtablas L2.
* **`[T4]` Verificación de Integridad de Datos:** Comprueba byte por byte la consistencia del contenido mediante el patrón aritmético determinista `byte = (i * 7 + 13) % 256`.
* **`[T5]` Liberación Completa y Reutilización (`itrunc()`):** Ejecuta 3 ciclos consecutivos de creación, llenado (300 bloques) y truncado total (`unlink`), certificando la devolución de bloques y ausencia de fugas en la bitmap (`balloc`).
* **`[T6]` Integridad del Inodo en Disco:** Verifica que la llamada al sistema `stat()` reporte con exactitud el tamaño acumulado del archivo y su tipo de inodo.
* **`[T7]` Límite Máximo del Sistema de Archivos:** Valida que el kernel soporte archivos que abarquen la totalidad del espacio disponible en disco sin entrar en estado de `panic`.

### Resultado de la Ejecución
```text
==================================================================
RESULTADO DE LA SUITE DE EVALUACION:
  Total de Pruebas Ejecutadas : 7
  Pruebas Superadas (PASS)    : 7
  Pruebas Fallidas (FAIL)     : 0
  Tasa de Exito               : 100.0%
==================================================================
>>> EVALUACION FINAL: TODOS LOS MODULOS FUNCIONAN CORRECTAMENTE <<<
```

---

## 🚀 Requisitos e Instalación

### Prerrequisitos (Ubuntu / Debian / WSL2)
Instalar el compilador cruzado para RISC-V, el emulador QEMU y las herramientas esenciales de desarrollo:
```bash
sudo apt-get update
sudo apt-get install -y git build-essential gdb-multiarch qemu-system-misc \
                        gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu
```

### Compilación del Kernel y Creación del Sistema de Archivos
Clonar el repositorio y compilar la imagen:
```bash
git clone https://github.com/UnschSO/xv6-large-files-unsch-2026.git
cd xv6-large-files-unsch-2026
make clean
make fs.img
```

### Ejecución en QEMU
Para iniciar el sistema operativo xv6 en el emulador:
```bash
make qemu
```

Una vez que xv6 haya iniciado y muestre el prompt `$`, ejecutar la suite automatizada:
```bash
$ bigfiletest
```

Para salir de QEMU, presionar: `Ctrl + A` y luego la tecla `X`.

---

## 📁 Estructura del Código Fuente

```text
├── kernel/
│   ├── fs.h          # Constantes NDIRECT, NDINDIRECT, MAXFILE y struct dinode (Módulo B)
│   ├── file.h        # Definición sincronizada de struct inode en memoria (Módulo B)
│   ├── fs.c          # Implementación de bmap() (Módulo A) e itrunc() (Módulo C)
│   ├── param.h       # Configuración global de FSSIZE = 20000
│   └── ...           # Manejo de interrupciones, traps, memoria virtual y procesos
├── user/
│   ├── bigfiletest.c # Suite de pruebas automatizadas T1-T7 (Módulo D)
│   └── ...           # Utilidades estándar de usuario de xv6
├── mkfs/
│   └── mkfs.c        # Constructor de la imagen de disco con soporte para el inodo extendido
└── Makefile          # Orquestación de compilación y enlaces de binarios de usuario
```

---

## 📄 Licencia y Reconocimientos
El código base de xv6 está desarrollado por MIT bajo licencia educativa. Las extensiones de direccionamiento doblemente indirecto y la suite de evaluación fueron desarrolladas para el curso de **Sistemas Operativos 2026** de la **Universidad Nacional de San Cristóbal de Huamanga (UNSCH)**.
