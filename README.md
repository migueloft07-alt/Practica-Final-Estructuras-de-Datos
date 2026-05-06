# Practice II – Data Structures and Algorithms (C++)

**Miguel Jimenez Gomez – Juliana Sepulveda Saldarriaga**

ST0245 - SI001 - Data Structures and Algorithms  
School of Applied Sciences and Engineering – Universidad EAFIT  
Instructor: Alexander Narváez Berrío

---

## Description

C++ program that sorts integers using three non-comparative algorithms — **DialSort**, **Parallel DialSort**, and **RadixSort LSD** — and benchmarks them against `std::sort` (GCC introsort) on datasets of up to 10,000,000 records. Also includes an HTML visual simulator that shows DialSort and RadixSort step by step.

---

## Objective

Determine under which conditions each algorithm is the best choice, varying the input size (n), the universe size (U), and the data distribution.

---

## Project Structure

```
├── src/
│   └── DialsortVSRadixsort.cpp   # Full benchmark and dataset export
├── Simulador/
│   └── DialsortVSRadixsort.html  # Visual simulator (open in browser)
├── Informe/                      # Technical report (PDF/DOCX)
├── CMakeLists.txt
└── README.md
```

Running the binary automatically creates a `datasets/` folder with 24 CSV test files.

---

## Implemented Algorithms

### A) DialSort (sequential)

Non-comparative. Builds a flat histogram `H[0..U-1]` with a single pass over n, then iterates over H emitting each value `H[k]` times. Handles negative numbers by shifting by `mn`. Skipped if `U > 10,000,000`.

### B) Parallel DialSort

Same idea as DialSort, but the **histogram counting phase** is distributed across 8 threads. Each thread builds a local histogram without race conditions; the merge and final emission are sequential.

### C) RadixSort LSD (alternative proposal)

Non-comparative, base 256, 4 passes. Handles signed integers by applying XOR with `0x80000000` (flips the sign bit). Each pass counts frequencies in 256 buckets, computes prefix sums, and scatters stably. The number of passes is **fixed**: performance does not depend on U.

---

## Benchmark Parameters

| Parameter | Values |
|---|---|
| Input sizes (n) | 10,000 · 100,000 · 1,000,000 · 10,000,000 |
| Universe sizes (U) | 256 · 1,024 · 65,536 |
| Distributions | uniform, skewed (80% in bottom 5%), sorted, reverse |
| Rounds | 3 warm-up (discarded) + 7 measured (best time reported) |
| Parallel threads | 8 |
| Random seed | 20260321 |

---

## Compilation and Execution

**CLion:** open the project, let CMake configure, and run with `Shift + F10`.

**Command line (Linux / macOS):**

```bash
g++ -O3 -std=c++17 -pthread -o benchmark src/DialsortVSRadixsort.cpp
./benchmark
```

**Command line (Windows / MinGW):**

```bash
g++ -O3 -std=c++17 -pthread -o benchmark.exe src/DialsortVSRadixsort.cpp -lpsapi
.\benchmark.exe
```

To open the simulator, open `Simulador/DialsortVSRadixsort.html` in any browser.

> **Note:** If CLion shows the error `Cannot find source file`, make sure `CMakeLists.txt` references `src/DialsortVSRadixsort.cpp` and not just `DialsortVSRadixsort.cpp`.

---

## Sample Output

The program output is organized in 7 sections. Below is the general structure with a representative block from the timing section.
```
======================================================================================================
  BENCHMARK: DialSort vs. RadixSort LSD vs. std::sort
  Materia : ST0245 - Estructuras de Datos y Algoritmos — EAFIT
  Docente : Alexander Narvaez Berrio
======================================================================================================
  Compilador   : g++ 13.1.0
  Flags        : -O3 -std=c++17 -pthread
  Hilos        : 8
  Calentamiento: 3 ejecuciones descartadas
  Medicion     : mejor de 7 ejecuciones
  Semilla      : 20260321
======================================================================================================

SECCION 1 — VISUALIZACION DEL COMPORTAMIENTO INTERNO
  Traza paso a paso de DialSort y RadixSort sobre n=16 elementos.

SECCION 2 — COMPLEJIDAD ALGORITMICA
  Tabla Big O: mejor / promedio / peor caso y espacio.

SECCION 3 — TIEMPO DE EJECUCION REAL

  Algoritmo             Dist.       N           U       ms (mejor)   M claves/s    vs std::sort  mem KB
  -------------------------------------------------------------------------------------------------
  std::sort             uniforme    10000       256     1.054        9.486         1.000         1 KB
  DialSort              uniforme    10000       256     0.045        221.729       23.375        1 KB
  RadixSort LSD         uniforme    10000       256     0.249        40.128        4.230         80 KB
  DialSort-Paralelo     uniforme    10000       256     0.499        20.024        2.111         9 KB
  ...

SECCION 4 — CONSUMO DE MEMORIA
  Memoria reservada por cada algoritmo, calculada con sizeof sobre estructuras internas.

SECCION 5 — ANALISIS COMPARATIVO FINAL
  Conteo de victorias DialSort vs RadixSort por tamano de universo, ratios y conclusiones.

SECCION 6 — SALIDA CSV
  Datos exportables a hoja de calculo.

SECCION 7 — EXPORTACION DE DATASETS
  Genera 24 archivos CSV en datasets/ (4 distribuciones x 3 universos x 2 tamaños).

  Verificacion de correctitud: TODAS LAS PRUEBAS PASARON
```

> **Note:** Section 3 evaluates 48 combinations (4 sizes × 3 universes × 4 distributions) with 4 algorithms each, producing 192 rows. The block shown corresponds to the first case (n=10,000, U=256, uniform distribution); the rest follow the same format.

---

## Report

The program itself generates the full report automatically upon execution.
The output is organized in 7 sections that cover all required points:

- **Implementation approach** — Section 1 (step-by-step internal trace of each algorithm) and Section 2 (Big O complexity table)
- **Performance measurements** — Section 3 (execution times in ms, M keys/s, speedup vs `std::sort`) and Section 4 (memory usage)
- **Comparison between algorithms** — Section 5 (win count per universe size, ratios and conclusions)

The full output is saved in `Informe/` for reference.

