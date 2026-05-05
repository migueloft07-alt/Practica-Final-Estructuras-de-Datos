# Practica-Final-Estructuras-de-Datos
DialSort vs RadixSort LSD — Benchmark y Simulador Visual

ST0245 — Estructuras de Datos y Algoritmos
Escuela de Ciencias Aplicadas e Ingeniería — Universidad EAFIT
Docente: Alexander Narváez Berrío | Abril 2026


👥 Integrantes del equipo
NombreCorreo[Nombre 1][correo@eafit.edu.co][Nombre 2][correo@eafit.edu.co]

📋 Descripción del proyecto
Este proyecto implementa y compara experimentalmente tres estrategias de ordenamiento de enteros:

DialSort (secuencial) — algoritmo no comparativo basado en histograma plano
DialSort Paralelo — variante multi-hilo de DialSort
RadixSort LSD (propuesta alternativa) — ordenamiento por dígitos, base 256, 4 pasadas

El análisis compara los algoritmos en términos de complejidad algorítmica (Big O), tiempo de ejecución real y consumo de memoria, variando el tamaño de entrada (n), el tamaño del universo (U) y la distribución de los datos.
Adicionalmente, el proyecto incluye un simulador visual interactivo en HTML que muestra el comportamiento interno de DialSort y RadixSort paso a paso.

🗂️ Estructura del repositorio
📁 untitled/
├── 📄 main.cpp              # Código fuente del benchmark en C++
├── 📄 simulador.html        # Simulador visual interactivo (abrir en navegador)
├── 📄 CMakeLists.txt        # Configuración de compilación
├── 📄 README.md             # Este archivo
└── 📁 datasets/             # Datasets generados automáticamente
    ├── dataset_n100000_U256_uniforme.csv
    ├── dataset_n100000_U256_sesgada.csv
    ├── dataset_n100000_U256_ordenada.csv
    ├── dataset_n100000_U256_inversa.csv
    ├── dataset_n100000_U1024_uniforme.csv
    ├── dataset_n100000_U1024_sesgada.csv
    ├── dataset_n100000_U1024_ordenada.csv
    ├── dataset_n100000_U1024_inversa.csv
    ├── dataset_n100000_U65536_uniforme.csv
    ├── dataset_n100000_U65536_sesgada.csv
    ├── dataset_n100000_U65536_ordenada.csv
    ├── dataset_n100000_U65536_inversa.csv
    ├── dataset_n1000000_U256_uniforme.csv
    ├── dataset_n1000000_U256_sesgada.csv
    ├── dataset_n1000000_U256_ordenada.csv
    ├── dataset_n1000000_U256_inversa.csv
    ├── dataset_n1000000_U1024_uniforme.csv
    ├── dataset_n1000000_U1024_sesgada.csv
    ├── dataset_n1000000_U1024_ordenada.csv
    ├── dataset_n1000000_U1024_inversa.csv
    ├── dataset_n1000000_U65536_uniforme.csv
    ├── dataset_n1000000_U65536_sesgada.csv
    ├── dataset_n1000000_U65536_ordenada.csv
    └── dataset_n1000000_U65536_inversa.csv

⚙️ Requisitos del sistema
HerramientaVersión mínimag++11.0 o superiorC++ estándarC++17CMake3.20 o superiorSistema operativoWindows 10/11, Linux, macOS

🚀 Instrucciones de compilación y ejecución
Opción 1 — Usando CLion (recomendado)

Abrir CLion y cargar la carpeta del proyecto
Verificar que CMakeLists.txt contenga:

cmake   cmake_minimum_required(VERSION 3.20)
   project(untitled)
   set(CMAKE_CXX_STANDARD 17)
   add_executable(untitled main.cpp)
   target_link_libraries(untitled psapi)

Presionar Ctrl + F9 para compilar
Presionar Shift + F10 para ejecutar

Opción 2 — Usando terminal (g++ directo)
bash# Linux / macOS
g++ -O3 -std=c++17 -pthread -o benchmark main.cpp
./benchmark

# Windows (PowerShell con MinGW)
g++ -O3 -std=c++17 -pthread -o benchmark.exe main.cpp -lpsapi
.\benchmark.exe
Opción 3 — PowerShell desde cmake-build-debug
powershellcd C:\Users\User\CLionProjects\untitled\cmake-build-debug
.\untitled.exe

Nota: Los datasets se generan automáticamente en la carpeta datasets/ al ejecutar el programa. No es necesario crearla manualmente.


🌐 Simulador Visual
Abrir el archivo simulador.html directamente en cualquier navegador moderno (Chrome, Firefox, Edge).
Funcionalidades:

Visualización paso a paso del comportamiento interno de DialSort y RadixSort
Botón Paso para avanzar manualmente
Botón Auto para ejecución automática
Botón Aleatorio para generar vectores de prueba aleatorios
Soporte para vectores personalizados y valor máximo configurable


📊 Parámetros del benchmark
ParámetroValoresTamaño de entrada (n)100,000 · 1,000,000 · 10,000,000Tamaño del universo (U)256 · 1,024 · 65,536DistribucionesUniforme · Sesgada · Ordenada · InversaRondas de calentamiento1 (descartada)Rondas de medición3 (se reporta el mejor tiempo)Semilla aleatoria20260321Hilos (DialSort Paralelo)8

🧮 Complejidad algorítmica
AlgoritmoMejor CasoCaso PromedioPeor CasoEspacioComparativostd::sort (introsort)O(n log n)O(n log n)O(n log n)O(log n)✅ SíDialSortO(n + U)O(n + U)O(n + U)O(U)❌ NoDialSort ParaleloO(n/t + U)O(n/t + U)O(n/t + U)O(t·U)❌ NoRadixSort LSDO(n·k)O(n·k)O(n·k)O(n + B)❌ No

Leyenda: n = elementos · U = universo (max−min+1) · t = hilos · k = 4 pasadas · B = 256


📈 Resumen de resultados
Tiempo de ejecución

DialSort es el más rápido cuando U es pequeño (256, 1024): el histograma cabe completamente en caché L1/L2
RadixSort LSD es más consistente cuando U es grande (65,536+): su huella de memoria O(n+256) es fija e independiente de U
DialSort Paralelo supera a ambos para n = 10,000,000 con U pequeño gracias a los 8 hilos
Ambos superan ampliamente a std::sort en datos enteros (entre 5x y 52x más rápidos)

Consumo de memoria
AlgoritmoU=256 N=100KU=65536 N=100KU=65536 N=1Mstd::sort1 KB1 KB1 KBDialSort1 KB256 KB256 KBDialSort Paralelo9 KB2,304 KB2,304 KBRadixSort LSD783 KB783 KB7,814 KB
¿Qué algoritmo usar según el escenario?
EscenarioAlgoritmo recomendadoU ≤ 1,024 y n grandeDialSort o DialSort ParaleloU ≥ 65,536 o U desconocidoRadixSort LSDDatos no enteros o U enormestd::sort

📁 Formato de los datasets
Cada archivo CSV tiene el siguiente formato:
indice,valor
0,142
1,891
2,67
...
Nomenclatura de archivos:
dataset_n{N}_U{U}_{distribucion}.csv
Distribuciones incluidas:

uniforme — valores aleatorios uniformemente distribuidos en [0, U)
sesgada — 80% de valores en el 5% inferior del universo (hot-spot)
ordenada — datos ya ordenados de menor a mayor
inversa — datos ordenados de mayor a menor


🔍 Secciones del reporte generado
Al ejecutar el programa se generan las siguientes secciones:

Visualización del comportamiento interno — traza paso a paso con n=16 elementos
Complejidad algorítmica (Big O) — tabla comparativa de mejor, promedio y peor caso
Tiempo de ejecución real — benchmark completo con todas las combinaciones
Consumo de memoria — complejidad espacial teórica y mediciones
Análisis comparativo final — conclusiones y recomendaciones
Salida CSV — datos exportables para graficar en Excel u hojas de cálculo
Exportación de datasets — generación automática de los 24 archivos CSV
