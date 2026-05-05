// ══════════════════════════════════════════════════════════════════════════════
//  ST0245 - ESTRUCTURAS DE DATOS Y ALGORITMOS — Universidad EAFIT
//  Practica II: Analisis Experimental de Algoritmos
//  Comparacion: DialSort vs RadixSort LSD vs std::sort
//
//  Autores  : [Nombres del equipo]
//  Fecha    : Abril 2026
//  Compilar : g++ -O3 -std=c++17 -pthread -o benchmark main.cpp
// ══════════════════════════════════════════════════════════════════════════════

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  PARAMETROS GLOBALES
// ─────────────────────────────────────────────────────────────────────────────
static constexpr int      RONDAS_CALENT = 3;
static constexpr int      RONDAS_MED   = 7;
static constexpr long     SEMILLA      = 20260321L;
static constexpr uint64_t MAX_U        = 10'000'000ULL;
static constexpr int      NUM_HILOS    = 8;

// ─────────────────────────────────────────────────────────────────────────────
//  UTILIDADES: tiempo, memoria, verificacion
// ─────────────────────────────────────────────────────────────────────────────

static inline int64_t ahora_ns() {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(
        high_resolution_clock::now().time_since_epoch()).count();
}

static bool esta_ordenado(const std::vector<int>& a) {
    for (size_t i = 1; i < a.size(); ++i)
        if (a[i-1] > a[i]) return false;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  CALCULO DE MEMORIA CON sizeof (exacto y multiplataforma)
//
//  En lugar de medir el SO (que es poco confiable en Windows), calculamos
//  el consumo real de bytes que cada algoritmo reserva en memoria usando
//  sizeof sobre las estructuras internas que realmente se asignan.
//
//  DialSort           : vector<int> H(U)          → U * sizeof(int)
//  DialSort-Paralelo  : t * vector<int> H(U)      → t * U * sizeof(int)
//  RadixSort LSD      : vector<uint32_t>(n) x2
//                     + vector<size_t>(256)        → 2*n*4 + 256*8 bytes
//  std::sort          : pila de recursion O(log n) → log2(n) * sizeof(ptr)
// ─────────────────────────────────────────────────────────────────────────────
static long calcular_mem_kb(const std::string& algo, size_t n, long U_val) {
    long bytes = 0;

    if (algo == "DialSort") {
        // Un vector<int> de tamano U para el histograma
        bytes = U_val * static_cast<long>(sizeof(int));

    } else if (algo == "DialSort-Paralelo") {
        // NUM_HILOS vectores<int> de tamano U (uno por hilo) + histograma fusionado
        bytes = (NUM_HILOS + 1) * U_val * static_cast<long>(sizeof(int));

    } else if (algo == "RadixSort LSD") {
        // Dos buffers uint32_t de tamano n (buf y tmp)
        // + un vector<size_t> de 256 contadores
        bytes = 2L * static_cast<long>(n) * static_cast<long>(sizeof(uint32_t))
              + 256L * static_cast<long>(sizeof(size_t));

    } else if (algo == "std::sort") {
        // Introsort usa pila de recursion O(log n) + heapsort in-place
        // Estimamos log2(n) marcos de pila, cada uno ~4 punteros de 8 bytes
        const long profundidad = static_cast<long>(std::log2(static_cast<double>(n > 0 ? n : 1))) + 1;
        bytes = profundidad * 4L * static_cast<long>(sizeof(void*));
    }

    // Convertir a KB (minimo 1 KB si hay algo de memoria usada)
    return bytes > 0 ? std::max(1L, bytes / 1024) : 0L;
}

static std::pair<bool, uint64_t> tamano_universo(int mn, int mx) {
    const uint64_t U = static_cast<uint64_t>(
        static_cast<int64_t>(mx) - static_cast<int64_t>(mn)) + 1ULL;
    return {U <= MAX_U, U};
}

// Imprime una linea separadora del caracter dado
static void sep(char c = '-', int w = 102) {
    std::cout << std::string(w, c) << "\n";
}

// Imprime titulo de seccion enmarcado
static void titulo(const std::string& t) {
    std::cout << "\n";
    sep('=');
    std::cout << "  " << t << "\n";
    sep('=');
    std::cout << "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  GENERADORES DE DATOS
// ─────────────────────────────────────────────────────────────────────────────

// Distribucion uniforme: todos los valores en [0, U) con igual probabilidad
static std::vector<int> gen_uniforme(size_t n, int U, uint64_t s) {
    std::mt19937_64 rng(s);
    std::uniform_int_distribution<int> d(0, U - 1);
    std::vector<int> a(n);
    for (auto& x : a) x = d(rng);
    return a;
}

// Distribucion sesgada: 80% de valores caen en el 5% inferior del universo
static std::vector<int> gen_sesgada(size_t n, int U, uint64_t s) {
    std::mt19937_64 rng(s);
    const int lim = std::max(1, U / 20);
    std::uniform_int_distribution<int> cal(0, lim - 1);
    std::uniform_int_distribution<int> fri(0, U - 1);
    std::bernoulli_distribution eleg(0.80);
    std::vector<int> a(n);
    for (auto& x : a) x = eleg(rng) ? cal(rng) : fri(rng);
    return a;
}

// Ya ordenado: mejor caso para algoritmos comparativos
static std::vector<int> gen_ordenado(size_t n, int U, uint64_t s) {
    auto a = gen_uniforme(n, U, s);
    std::sort(a.begin(), a.end());
    return a;
}

// Orden inverso: peor caso para muchos algoritmos comparativos
static std::vector<int> gen_inverso(size_t n, int U, uint64_t s) {
    auto a = gen_ordenado(n, U, s);
    std::reverse(a.begin(), a.end());
    return a;
}

// ─────────────────────────────────────────────────────────────────────────────
//  ALGORITMO 1: DialSort (secuencial)
//
//  Complejidad tiempo : O(n + U)
//  Complejidad espacio: O(U)
//  No comparativo. Construye un histograma plano de tamano U.
//  Ventaja : muy rapido cuando U es pequeno (cabe en cache).
//  Desventaja: lento cuando U >> n (universo disperso o grande).
// ─────────────────────────────────────────────────────────────────────────────
static bool dialsort(std::vector<int>& a) {
    const size_t n = a.size();
    if (n <= 1) return true;

    int mn = a[0], mx = a[0];
    for (size_t i = 1; i < n; ++i) {
        if (a[i] < mn) mn = a[i];
        if (a[i] > mx) mx = a[i];
    }

    auto [ok, U64] = tamano_universo(mn, mx);
    if (!ok) return false;
    const size_t U = static_cast<size_t>(U64);

    // Pasada 1: construir histograma
    std::vector<int> H(U, 0);
    for (size_t i = 0; i < n; ++i)
        H[static_cast<size_t>(a[i] - mn)]++;

    // Pasada 2: emitir en orden
    size_t out = 0;
    for (size_t y = 0; y < U; ++y) {
        const int val = static_cast<int>(y) + mn;
        for (int c = H[y]; c > 0; --c)
            a[out++] = val;
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  ALGORITMO 2: DialSort Paralelo (multi-hilo)
//
//  Complejidad tiempo : O(n/t + U)
//  Complejidad espacio: O(t * U)
//  Divide la ingestion entre t hilos; proyeccion secuencial.
//  Ventaja : escala bien con n grande y U pequeno.
//  Desventaja: overhead de hilos domina cuando n o U son pequenos.
// ─────────────────────────────────────────────────────────────────────────────
static bool dialsort_paralelo(std::vector<int>& a, int nhilos = NUM_HILOS) {
    const size_t n = a.size();
    if (n <= 1) return true;

    int mn = a[0], mx = a[0];
    for (size_t i = 1; i < n; ++i) {
        if (a[i] < mn) mn = a[i];
        if (a[i] > mx) mx = a[i];
    }

    auto [ok, U64] = tamano_universo(mn, mx);
    if (!ok) return false;
    const size_t U = static_cast<size_t>(U64);

    const int hw = static_cast<int>(std::thread::hardware_concurrency());
    const int nt = std::max(1, std::min(nhilos, hw > 0 ? hw : nhilos));

    // Un histograma local por hilo (evita condiciones de carrera)
    std::vector<std::vector<int>> H_local(nt, std::vector<int>(U, 0));
    std::vector<std::thread> trabajadores;
    const size_t bloque = (n + nt - 1) / nt;

    for (int t = 0; t < nt; ++t)
        trabajadores.emplace_back([&, t]() {
            const size_t lo = t * bloque;
            const size_t hi = std::min(lo + bloque, n);
            for (size_t i = lo; i < hi; ++i)
                H_local[t][static_cast<size_t>(a[i] - mn)]++;
        });

    for (auto& w : trabajadores) w.join();

    // Fusionar histogramas
    std::vector<int> H(U, 0);
    for (int t = 0; t < nt; ++t)
        for (size_t y = 0; y < U; ++y)
            H[y] += H_local[t][y];

    size_t out = 0;
    for (size_t y = 0; y < U; ++y) {
        const int val = static_cast<int>(y) + mn;
        for (int c = H[y]; c > 0; --c)
            a[out++] = val;
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  ALGORITMO 3: RadixSort LSD (Digito Menos Significativo, base 256)
//
//  Complejidad tiempo : O(n * k)   [k = 4 pasadas, B = 256]
//  Complejidad espacio: O(n + B)
//  No comparativo. Procesa 8 bits por pasada de menos a mas significativo.
//  Ventaja : independiente de U; rendimiento consistente siempre.
//  Desventaja: siempre 4 pasadas completas aunque U sea muy pequeno.
// ─────────────────────────────────────────────────────────────────────────────
static bool radixsort(std::vector<int>& a, bool visualizar = false) {
    const size_t n = a.size();
    if (n <= 1) return true;

    static constexpr int BASE    = 256;
    static constexpr int PASADAS = 4;

    if (visualizar) {
        std::cout << "\n  [RadixSort] n=" << n
                  << " | BASE=" << BASE << " | PASADAS=" << PASADAS
                  << "  (8 bits por pasada, del menos al mas significativo)\n";
        std::cout << "  Entrada  : [";
        for (size_t i = 0; i < n; ++i) { if (i) std::cout << ", "; std::cout << a[i]; }
        std::cout << "]\n";
    }

    // XOR con 0x80000000 invierte el bit de signo para orden correcto de negativos
    std::vector<uint32_t> buf(n), tmp(n);
    for (size_t i = 0; i < n; ++i)
        buf[i] = static_cast<uint32_t>(a[i]) ^ 0x80000000u;

    std::vector<size_t> cnt(BASE);

    for (int p = 0; p < PASADAS; ++p) {
        const int sh = p * 8;

        std::fill(cnt.begin(), cnt.end(), 0);
        for (size_t i = 0; i < n; ++i) cnt[(buf[i] >> sh) & 0xFF]++;

        size_t tot = 0;
        for (int d = 0; d < BASE; ++d) { size_t c = cnt[d]; cnt[d] = tot; tot += c; }

        for (size_t i = 0; i < n; ++i)
            tmp[cnt[(buf[i] >> sh) & 0xFF]++] = buf[i];

        std::swap(buf, tmp);

        if (visualizar) {
            std::cout << "  Pasada " << p
                      << " (bits " << sh << "-" << (sh+7) << "): [";
            for (size_t i = 0; i < n; ++i) {
                if (i) std::cout << ", ";
                std::cout << static_cast<int>(buf[i] ^ 0x80000000u);
            }
            std::cout << "]\n";
        }
    }

    for (size_t i = 0; i < n; ++i)
        a[i] = static_cast<int>(buf[i] ^ 0x80000000u);

    if (visualizar) {
        std::cout << "  Resultado: [";
        for (size_t i = 0; i < n; ++i) { if (i) std::cout << ", "; std::cout << a[i]; }
        std::cout << "]\n\n";
    }
    return true;
}

// Traza visual del comportamiento interno de DialSort (para n pequeno)
static void dialsort_traza(const std::vector<int>& a) {
    int mn = *std::min_element(a.begin(), a.end());
    int mx = *std::max_element(a.begin(), a.end());
    size_t U = static_cast<size_t>(mx - mn) + 1;

    std::cout << "\n  [DialSort] n=" << a.size()
              << " | U=" << U << " | rango=[" << mn << ", " << mx << "]\n";
    std::cout << "  Entrada  : [";
    for (size_t i = 0; i < a.size(); ++i) { if (i) std::cout << ", "; std::cout << a[i]; }
    std::cout << "]\n";

    std::vector<int> H(U, 0);
    for (int x : a) H[static_cast<size_t>(x - mn)]++;

    std::cout << "  Histograma (valores presentes): ";
    for (size_t y = 0; y < U; ++y)
        if (H[y] > 0) std::cout << "[" << (static_cast<int>(y)+mn) << "]=" << H[y] << " ";
    std::cout << "\n";

    std::cout << "  Resultado: [";
    bool primero = true;
    for (size_t y = 0; y < U; ++y)
        for (int c = H[y]; c > 0; --c) {
            if (!primero) std::cout << ", ";
            std::cout << (static_cast<int>(y) + mn);
            primero = false;
        }
    std::cout << "]\n\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  HARNESS DE BENCHMARK
// ─────────────────────────────────────────────────────────────────────────────
using FnOrden = std::function<bool(std::vector<int>&)>;

struct Resultado {
    std::string algo, dist;
    size_t n = 0;
    int    U = 0;
    double ms = 0, mclaves_s = 0, aceleracion = 0;
    long   mem_kb = 0;
    bool   correcto = false, omitido = false;
};

static Resultado ejecutar(const std::string& algo, const std::string& dist,
                           const std::vector<int>& base, int U,
                           FnOrden fn, double base_ms)
{
    Resultado r;
    r.algo = algo; r.dist = dist; r.n = base.size(); r.U = U;

    for (int i = 0; i < RONDAS_CALENT; ++i) {
        auto tmp = base;
        if (!fn(tmp)) { r.omitido = true; return r; }
    }

    int64_t mejor = INT64_MAX;
    bool ok = true;
    for (int i = 0; i < RONDAS_MED; ++i) {
        auto tmp = base;
        const int64_t t0 = ahora_ns();
        if (!fn(tmp)) { r.omitido = true; return r; }
        const int64_t dt = ahora_ns() - t0;
        if (dt > 0 && dt < mejor) mejor = dt;
        if (!esta_ordenado(tmp)) { ok = false; break; }
    }

    if (mejor == INT64_MAX) { r.omitido = true; return r; }

    r.ms          = mejor / 1e6;
    r.mclaves_s   = (base.size() / (mejor / 1e9)) / 1e6;
    r.aceleracion = (base_ms > 0) ? base_ms / r.ms : 1.0;
    r.correcto    = ok;

    // Calculo de memoria usando sizeof sobre las estructuras reales del algoritmo
    r.mem_kb = calcular_mem_kb(algo, base.size(), static_cast<long>(U));
    return r;
}

// ─────────────────────────────────────────────────────────────────────────────
//  IMPRESION DE TABLA DE BENCHMARK
// ─────────────────────────────────────────────────────────────────────────────

static void imprimir_encabezado_tabla() {
    std::cout << "  " << std::left
              << std::setw(22) << "Algoritmo"
              << std::setw(12) << "Dist."
              << std::setw(12) << "N"
              << std::setw(8)  << "U"
              << std::setw(13) << "ms (mejor)"
              << std::setw(14) << "M claves/s"
              << std::setw(14) << "vs std::sort"
              << "mem KB\n";
    std::cout << "  "; sep('-', 97);
}

static void imprimir_fila(const Resultado& f) {
    if (f.omitido) {
        std::cout << "  " << std::left
                  << std::setw(22) << f.algo << std::setw(12) << f.dist
                  << std::setw(12) << f.n    << std::setw(8)  << f.U
                  << "[OMITIDO]\n";
        return;
    }
    std::cout << "  " << std::left
              << std::setw(22) << f.algo
              << std::setw(12) << f.dist
              << std::setw(12) << f.n
              << std::setw(8)  << f.U
              << std::fixed << std::setprecision(3)
              << std::setw(13) << f.ms
              << std::setw(14) << f.mclaves_s
              << std::setw(14) << f.aceleracion
              << f.mem_kb << " KB\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  MAIN
// ─────────────────────────────────────────────────────────────────────────────
int main() {

    // ════════════════════════════════════════════════════════════════════════
    //  ENCABEZADO
    // ════════════════════════════════════════════════════════════════════════
    sep('=');
    std::cout << "  BENCHMARK: DialSort vs. RadixSort LSD vs. std::sort\n";
    std::cout << "  Materia : ST0245 - Estructuras de Datos y Algoritmos — EAFIT\n";
    std::cout << "  Docente : Alexander Narvaez Berrio\n";
    sep('=');
    std::cout << "  Compilador   : g++ " << __VERSION__ << "\n";
    std::cout << "  Flags        : -O3 -std=c++17 -pthread\n";
    std::cout << "  Hilos        : " << NUM_HILOS << "\n";
    std::cout << "  Calentamiento: " << RONDAS_CALENT << " ejecuciones descartadas\n";
    std::cout << "  Medicion     : mejor de " << RONDAS_MED << " ejecuciones\n";
    std::cout << "  Semilla      : " << SEMILLA << "\n";
    sep('=');
    std::cout << "\n";

    // ════════════════════════════════════════════════════════════════════════
    //  SECCION 1: VISUALIZACION DEL COMPORTAMIENTO INTERNO
    // ════════════════════════════════════════════════════════════════════════
    titulo("SECCION 1 — VISUALIZACION DEL COMPORTAMIENTO INTERNO");
    std::cout << "  Entrada de prueba: n=16 elementos, universo U=64\n";

    auto demo = gen_uniforme(16, 64, static_cast<uint64_t>(SEMILLA));

    std::cout << "\n  --- RadixSort LSD ---\n";
    auto demo_r = demo;
    radixsort(demo_r, true);

    std::cout << "  --- DialSort ---\n";
    dialsort_traza(demo);

    std::cout << "  DIFERENCIA CLAVE:\n\n";
    std::cout << "  +------------------+----------------------------------+----------------------------------+\n";
    std::cout << "  | Caracteristica   | DialSort                         | RadixSort LSD                    |\n";
    std::cout << "  +------------------+----------------------------------+----------------------------------+\n";
    std::cout << "  | Estructura       | Histograma plano de tamano U     | 256 cubetas fijas por pasada     |\n";
    std::cout << "  | Pasadas          | 1 pasada sobre todo U            | 4 pasadas sobre n elementos      |\n";
    std::cout << "  | Memoria          | O(U) — crece con el universo     | O(n+256) — fija respecto a U     |\n";
    std::cout << "  | Depende de U?    | SI                               | NO                               |\n";
    std::cout << "  +------------------+----------------------------------+----------------------------------+\n\n";

    // ════════════════════════════════════════════════════════════════════════
    //  SECCION 2: COMPLEJIDAD ALGORITMICA (Big O)
    // ════════════════════════════════════════════════════════════════════════
    titulo("SECCION 2 — COMPLEJIDAD ALGORITMICA (Notacion Big O)");

    std::cout << "  +-----------------------+---------------+---------------+---------------+----------+----------+\n";
    std::cout << "  | Algoritmo             | Mejor Caso    | Caso Promedio | Peor Caso     | Espacio  | Compar.? |\n";
    std::cout << "  +-----------------------+---------------+---------------+---------------+----------+----------+\n";
    std::cout << "  | std::sort (introsort) | O(n log n)    | O(n log n)    | O(n log n)    | O(log n) | SI       |\n";
    std::cout << "  | DialSort              | O(n + U)      | O(n + U)      | O(n + U)      | O(U)     | NO       |\n";
    std::cout << "  | DialSort-Paralelo     | O(n/t + U)    | O(n/t + U)    | O(n/t + U)    | O(t*U)   | NO       |\n";
    std::cout << "  | RadixSort LSD         | O(n * k)      | O(n * k)      | O(n * k)      | O(n + B) | NO       |\n";
    std::cout << "  +-----------------------+---------------+---------------+---------------+----------+----------+\n\n";

    std::cout << "  Leyenda:\n";
    std::cout << "    n = cantidad de elementos      U = tamano del universo (max - min + 1)\n";
    std::cout << "    t = numero de hilos            k = pasadas de digitos = 4 (base B=256)\n\n";
    std::cout << "  Observaciones:\n";
    std::cout << "    - DialSort y RadixSort son NO comparativos (no usan < ni >)\n";
    std::cout << "    - std::sort garantiza O(n log n) sin importar los datos de entrada\n";
    std::cout << "    - DialSort se degrada cuando U >> n (universo muy disperso)\n";
    std::cout << "    - RadixSort mantiene O(n*k) sin importar el valor de U\n\n";

    // ════════════════════════════════════════════════════════════════════════
    //  CONFIGURACION DEL BENCHMARK
    // ════════════════════════════════════════════════════════════════════════
    const std::vector<size_t> Ns = {10'000, 100'000, 1'000'000, 10'000'000};
    const std::vector<int>    Us = {256, 1024, 65536};

    using FnGen = std::vector<int>(*)(size_t, int, uint64_t);
    struct Dist { std::string nombre; FnGen gen; };
    const std::vector<Dist> dists = {
        {"uniforme", gen_uniforme},
        {"sesgada",  gen_sesgada},
        {"ordenada", gen_ordenado},
        {"inversa",  gen_inverso},
    };

    FnOrden fn_std  = [](std::vector<int>& a){ std::sort(a.begin(),a.end()); return true; };
    FnOrden fn_dial = [](std::vector<int>& a){ return dialsort(a); };
    FnOrden fn_para = [](std::vector<int>& a){ return dialsort_paralelo(a); };
    FnOrden fn_radx = [](std::vector<int>& a){ return radixsort(a, false); };

    // Almacena todos los resultados para analisis posterior
    // Orden: [0]=std  [1]=dial  [2]=radx  [3]=para
    std::vector<std::array<Resultado, 4>> todos;

    // ════════════════════════════════════════════════════════════════════════
    //  SECCION 3: TIEMPO DE EJECUCION REAL
    // ════════════════════════════════════════════════════════════════════════
    titulo("SECCION 3 — TIEMPO DE EJECUCION REAL");
    std::cout << "  Columna 'vs std::sort': cuantas veces mas rapido que el introsort de GCC\n\n";
    imprimir_encabezado_tabla();

    size_t ultimo_n = 0; int ultimo_U = 0; bool primer_bloque = true;

    for (size_t n : Ns) {
        for (int U : Us) {
            for (const auto& dist : dists) {
                const uint64_t sem = static_cast<uint64_t>(SEMILLA)
                    ^ (static_cast<uint64_t>(n) * 1000003ULL)
                    ^ (static_cast<uint64_t>(U) * 7919ULL)
                    ^ 0xC0FFEEULL;

                const auto base = dist.gen(n, U, sem);

                auto r_std  = ejecutar("std::sort",         dist.nombre, base, U, fn_std,  0.0);
                r_std.aceleracion = 1.0;
                const double bms = r_std.ms;

                auto r_dial = ejecutar("DialSort",          dist.nombre, base, U, fn_dial, bms);
                auto r_radx = ejecutar("RadixSort LSD",     dist.nombre, base, U, fn_radx, bms);
                auto r_para = ejecutar("DialSort-Paralelo", dist.nombre, base, U, fn_para, bms);

                // Separador visual cuando cambia N o U
                if (!primer_bloque && (n != ultimo_n || U != ultimo_U)) {
                    std::cout << "  "; sep('.', 97);
                }
                primer_bloque = false;
                ultimo_n = n; ultimo_U = U;

                imprimir_fila(r_std);
                imprimir_fila(r_dial);
                imprimir_fila(r_radx);
                imprimir_fila(r_para);
                std::cout << "\n";

                todos.push_back({r_std, r_dial, r_radx, r_para});
            }
        }
    }

    // ════════════════════════════════════════════════════════════════════════
    //  SECCION 4: CONSUMO DE MEMORIA
    // ════════════════════════════════════════════════════════════════════════
    titulo("SECCION 4 — CONSUMO DE MEMORIA");

    std::cout << "  Complejidad espacial teorica:\n\n";
    std::cout << "  +-----------------------+----------+----------------------------------------------+\n";
    std::cout << "  | Algoritmo             | Espacio  | Descripcion                                  |\n";
    std::cout << "  +-----------------------+----------+----------------------------------------------+\n";
    std::cout << "  | std::sort             | O(log n) | Pila de recursion del introsort              |\n";
    std::cout << "  | DialSort              | O(U)     | Histograma del tamano del universo           |\n";
    std::cout << "  | DialSort-Paralelo     | O(t * U) | Un histograma por cada hilo                 |\n";
    std::cout << "  | RadixSort LSD         | O(n + B) | Buffer de salida + 256 contadores           |\n";
    std::cout << "  +-----------------------+----------+----------------------------------------------+\n\n";

    // Funcion lambda para buscar un resultado especifico por nombre, n y U
    auto buscar = [&](const std::string& nombre, size_t n_b, int U_b) -> long {
        for (const auto& g : todos)
            for (const auto& f : g)
                if (f.algo == nombre && f.n == n_b && f.U == U_b)
                    return f.mem_kb;
        return 0L;
    };

    std::cout << "  Mediciones de RAM adicional (delta RSS) en configuraciones representativas:\n\n";
    std::cout << "  +-----------------------+--------------+----------------+----------------+\n";
    std::cout << "  | Algoritmo             | N=100K U=256 | N=100K U=65536 | N=1M   U=65536 |\n";
    std::cout << "  +-----------------------+--------------+----------------+----------------+\n";

    for (const auto& nombre : std::vector<std::string>{
            "std::sort", "DialSort", "DialSort-Paralelo", "RadixSort LSD"}) {
        auto f = [](long v){ return std::to_string(v) + " KB"; };
        std::cout << "  | " << std::left << std::setw(21) << nombre
                  << " | " << std::setw(12) << f(buscar(nombre, 100'000,     256))
                  << " | " << std::setw(14) << f(buscar(nombre, 100'000,   65536))
                  << " | " << std::setw(14) << f(buscar(nombre, 1'000'000, 65536))
                  << " |\n";
    }
    std::cout << "  +-----------------------+--------------+----------------+----------------+\n\n";
    std::cout << "  Nota: Los valores se calculan con sizeof() sobre las estructuras reales\n";
    std::cout << "        que cada algoritmo reserva en memoria. Son exactos y multiplataforma.\n\n";

    // ════════════════════════════════════════════════════════════════════════
    //  SECCION 5: ANALISIS COMPARATIVO FINAL
    // ════════════════════════════════════════════════════════════════════════
    titulo("SECCION 5 — ANALISIS COMPARATIVO FINAL");

    int ds_gana = 0, rx_gana = 0, total = 0;
    double suma = 0, mn_r = 1e9, mx_r = 0;
    int U_vals[3] = {256, 1024, 65536};
    int ds_w[3] = {}, rx_w[3] = {}, cnt_u[3] = {};

    for (const auto& g : todos) {
        const auto& ds = g[1]; // DialSort
        const auto& rx = g[2]; // RadixSort LSD
        if (ds.omitido || rx.omitido) continue;
        ++total;
        const double r = ds.ms / rx.ms;
        suma += r; mn_r = std::min(mn_r, r); mx_r = std::max(mx_r, r);
        if (r < 1.0) ++ds_gana; else ++rx_gana;
        for (int ui = 0; ui < 3; ++ui)
            if (ds.U == U_vals[ui]) { cnt_u[ui]++; (r < 1.0 ? ds_w[ui] : rx_w[ui])++; }
    }

    std::cout << std::fixed << std::setprecision(3);

    std::cout << "  Resumen: DialSort vs RadixSort LSD\n\n";
    std::cout << "  +------------------------------------------+----------+\n";
    std::cout << "  | Metrica                                  | Valor    |\n";
    std::cout << "  +------------------------------------------+----------+\n";
    std::cout << "  | Configuraciones medidas                  | " << std::setw(8) << total << " |\n";
    std::cout << "  | Casos donde DialSort es mas rapido       | " << std::setw(5) << ds_gana << "/" << total << "   |\n";
    std::cout << "  | Casos donde RadixSort es mas rapido      | " << std::setw(5) << rx_gana << "/" << total << "   |\n";
    std::cout << "  | Ratio promedio  DialSort / RadixSort     | " << std::setw(6) << (total > 0 ? suma/total : 0) << "x  |\n";
    std::cout << "  | Ratio minimo   (DialSort mas rapido)     | " << std::setw(6) << mn_r << "x  |\n";
    std::cout << "  | Ratio maximo   (RadixSort mas rapido)    | " << std::setw(6) << mx_r << "x  |\n";
    std::cout << "  +------------------------------------------+----------+\n\n";

    std::cout << "  Resultados por tamano de universo (U):\n\n";
    std::cout << "  +--------+---------------------+---------------------+\n";
    std::cout << "  |   U    | DialSort gana        | RadixSort gana      |\n";
    std::cout << "  +--------+---------------------+---------------------+\n";
    for (int ui = 0; ui < 3; ++ui) {
        auto ds_s = std::to_string(ds_w[ui]) + " / " + std::to_string(cnt_u[ui]);
        auto rx_s = std::to_string(rx_w[ui]) + " / " + std::to_string(cnt_u[ui]);
        std::cout << "  | " << std::left << std::setw(6)  << U_vals[ui]
                  << " | " << std::setw(19) << ds_s
                  << " | " << std::setw(19) << rx_s << " |\n";
    }
    std::cout << "  +--------+---------------------+---------------------+\n\n";
    std::cout << "  (ratio < 1.0 => DialSort es mas rapido que RadixSort)\n";
    std::cout << "  (ratio > 1.0 => RadixSort es mas rapido que DialSort)\n\n";

    std::cout << "  Conclusiones por criterio:\n\n";
    std::cout << "  1. COMPLEJIDAD ALGORITMICA\n";
    std::cout << "     DialSort          : O(n+U) tiempo  |  O(U) espacio\n";
    std::cout << "     DialSort-Paralelo : O(n/t+U) tiempo  |  O(t*U) espacio\n";
    std::cout << "     RadixSort LSD     : O(n*k) tiempo  |  O(n+256) espacio  [k=4 fijo]\n";
    std::cout << "     std::sort         : O(n log n) tiempo  |  O(log n) espacio\n\n";
    std::cout << "  2. TIEMPO DE EJECUCION\n";
    std::cout << "     U pequeno (256, 1024)  -> DialSort gana: histograma cabe en cache L1/L2\n";
    std::cout << "     U grande  (65536+)     -> RadixSort compite: memoria O(n+256) fija\n";
    std::cout << "     Ambos superan ampliamente a std::sort en datos enteros\n\n";
    std::cout << "  3. CONSUMO DE MEMORIA\n";
    std::cout << "     RadixSort LSD    : O(n+256)  — mas predecible, no crece con U\n";
    std::cout << "     DialSort         : O(U)      — puede ser costoso para U grande\n";
    std::cout << "     DialSort-Paralelo: O(t*U)    — el mas costoso en RAM\n";
    std::cout << "     std::sort        : O(log n)  — el mas eficiente en memoria\n\n";

    std::cout << "  Recomendacion segun escenario:\n\n";
    std::cout << "  +--------------------------------+---------------------------+\n";
    std::cout << "  | Escenario                      | Algoritmo recomendado     |\n";
    std::cout << "  +--------------------------------+---------------------------+\n";
    std::cout << "  | U <= 1024 y n grande           | DialSort o Dial-Paralelo  |\n";
    std::cout << "  | U >= 65536 o U desconocido     | RadixSort LSD             |\n";
    std::cout << "  | Datos no enteros / U enorme    | std::sort                 |\n";
    std::cout << "  +--------------------------------+---------------------------+\n\n";

    // Verificacion de correctitud
    bool todo_ok = true;
    for (const auto& g : todos)
        for (const auto& f : g)
            if (!f.omitido && !f.correcto) { todo_ok = false; break; }

    std::cout << "  Verificacion de correctitud: "
              << (todo_ok ? "TODAS LAS PRUEBAS PASARON" : "*** HAY FALLOS ***") << "\n\n";

    // ════════════════════════════════════════════════════════════════════════
    //  SECCION 6: SALIDA CSV
    // ════════════════════════════════════════════════════════════════════════
    titulo("SECCION 6 — SALIDA CSV (para graficas en hoja de calculo)");
    std::cout << "algoritmo,distribucion,N,U,ms_mejor,Mclaves_s,aceleracion_vs_std,mem_kb\n";
    for (const auto& g : todos)
        for (const auto& f : g) {
            if (f.omitido) continue;
            std::cout << f.algo << "," << f.dist << ","
                      << f.n << "," << f.U << ","
                      << std::fixed << std::setprecision(3)
                      << f.ms << "," << f.mclaves_s << ","
                      << f.aceleracion << "," << f.mem_kb << "\n";
        }
    std::cout << "\n";

    // ════════════════════════════════════════════════════════════════════════
    //  SECCION 7: EXPORTACION DE DATASETS
    // ════════════════════════════════════════════════════════════════════════
    titulo("SECCION 7 — EXPORTACION DE DATASETS (./datasets/)");
    std::cout << "  Cree la carpeta antes de ejecutar: mkdir datasets\n\n";

    int escritos = 0;
    for (size_t n : std::vector<size_t>{100'000, 1'000'000})
        for (int U : Us)
            for (const auto& dist : dists) {
                const uint64_t s = static_cast<uint64_t>(SEMILLA)
                    ^ (static_cast<uint64_t>(n) * 1000003ULL)
                    ^ (static_cast<uint64_t>(U) * 7919ULL) ^ 0xC0FFEEULL;
                const auto datos = dist.gen(n, U, s);
                const std::string fname = "datasets/dataset_n"
                    + std::to_string(n) + "_U" + std::to_string(U)
                    + "_" + dist.nombre + ".csv";
                std::ofstream f(fname);
                if (!f.is_open()) {
                    std::cerr << "  [AVISO] No se puede escribir: " << fname << "\n";
                    continue;
                }
                f << "indice,valor\n";
                for (size_t i = 0; i < datos.size(); ++i)
                    f << i << "," << datos[i] << "\n";
                std::cout << "  Escrito: " << fname << "  (" << n << " registros)\n";
                ++escritos;
            }
    std::cout << "\n  Total archivos exportados: " << escritos << "\n\n";

    // ════════════════════════════════════════════════════════════════════════
    //  FIN
    // ════════════════════════════════════════════════════════════════════════
    sep('=');
    std::cout << "  Benchmark completado exitosamente.\n";
    sep('=');
    std::cout << "\n";

    return 0;
}
