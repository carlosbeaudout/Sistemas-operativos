# Planificador Dieciochero

## Descripción

Este programa simula la ejecución de un conjunto de actividades de un asado dieciochero. Las actividades pueden tener dependencias entre ellas, por lo que una actividad solo puede comenzar cuando todas sus dependencias hayan terminado correctamente.

El programa utiliza procesos (`fork()`), pipes para la comunicación entre procesos y señales para manejar la interrupción mediante `Ctrl+C`.

El programa fue desarrollado en C++17 y no utiliza threads.

---

## Compilación

Para compilar el programa se utiliza:

```bash
g++ -Wall -Wextra -std=c++17 planificador.cpp -lpthread -o planificador
```

Aunque se incluye `-lpthread` en el comando de compilación solicitado, el programa no utiliza threads.

---

## Ejecución

El programa recibe dos argumentos:

```bash
./planificador plan.txt K
```

Donde:

- `plan.txt`: archivo que contiene las actividades.
- `K`: cantidad máxima de procesos que pueden estar ejecutándose al mismo tiempo.

Por ejemplo:

```bash
./planificador plan.txt 2
```

---

## Formato del archivo de entrada

Cada línea del archivo representa una actividad:

```text
ID : Nombre_Actividad : tiempo_ms : Dependencia1, Dependencia2
```

Por ejemplo:

```text
1 : prender_carbon : 500 :
2 : comprar_carne : 1200 :
3 : comprar_pan : 300 :
4 : asar_longaniza : 800 : 1,2
5 : armar_choripan : 250 : 3,4
6 : servir_mesa : 100 : 5
```

Las actividades que no tienen duración reciben un tiempo aleatorio entre 100 y 5000 milisegundos.

Las dependencias indican qué actividades deben terminar antes de que una actividad pueda comenzar.

---

## Funcionamiento

### 1. Lectura del archivo

El programa abre el archivo indicado por el usuario y lee cada línea.

Cada actividad se guarda en una estructura `Actividad`, que contiene:

- ID.
- Nombre.
- Tiempo de ejecución.
- Dependencias.
- Estado de término.
- Estado de falla.
- PID del proceso asociado.

### 2. Dependencias

Antes de crear un proceso, el programa revisa si todas las dependencias de la actividad han terminado.

Si alguna dependencia todavía no ha terminado, la actividad espera.

Cuando todas terminan correctamente, la actividad puede comenzar.

### 3. Creación de procesos

Cada actividad se ejecuta en un proceso independiente mediante `fork()`.

El programa mantiene un contador de procesos activos para evitar superar el límite `K`.

Por ejemplo, si se ejecuta:

```bash
./planificador plan.txt 2
```

como máximo habrá dos actividades ejecutándose al mismo tiempo.

### 4. Comunicación mediante pipes

Cuando una actividad termina, el proceso hijo envía un mensaje mediante un pipe.

El mensaje tiene el siguiente formato:

```text
PID:ID:estado
```

Por ejemplo:

```text
79308:1:0
```

Donde:

- `79308` corresponde al PID del proceso.
- `1` corresponde al ID de la actividad.
- `0` indica que terminó correctamente.

El proceso padre recibe este mensaje y actualiza el estado de la actividad.

### 5. Espera de procesos

El programa utiliza `waitpid()` para esperar al proceso correspondiente después de recibir su mensaje por el pipe.

De esta manera no se utiliza espera activa o busy-waiting.

### 6. Manejo de errores

Si una actividad informa un estado distinto de `0`, se considera que falló.

Las actividades que dependan de una actividad fallida son canceladas.

La falla solamente afecta a la rama de actividades que depende de ella y no detiene todas las demás actividades.

### 7. Interrupción con Ctrl+C

El programa utiliza `SIGINT` para detectar cuando el usuario presiona `Ctrl+C`.

Al recibir la señal, el programa termina los procesos que se encuentran ejecutándose y finaliza la simulación.

---

## Pruebas realizadas

### Plan normal

Se probó el programa utilizando:

```bash
./planificador plan.txt 2
```

Las actividades se ejecutaron respetando sus dependencias y sin superar el límite de dos procesos activos.

### Aislamiento de errores

Se realizó una prueba provocando el fallo de una actividad.

El resultado fue que la actividad que falló fue marcada como fallida y sus actividades dependientes fueron canceladas, sin detener las actividades independientes.

### Interrupción

Se probó `Ctrl+C` durante la ejecución de una carga grande.

El programa recibió `SIGINT`, terminó los procesos activos y finalizó correctamente.

### Prueba de estrés

Se generó un archivo con 10.000 actividades y se ejecutó:

```bash
./planificador plan10000.txt 10
```

Las 10.000 actividades fueron procesadas correctamente sin que el programa terminara por errores de memoria o de comunicación mediante el pipe.

---

## Funciones principales

### `dependenciasTerminadas()`

Comprueba si todas las dependencias de una actividad ya terminaron.

### `dependenciaFallida()`

Comprueba si alguna de las dependencias de una actividad falló.

### `manejarSIGINT()`

Maneja la señal `SIGINT` producida por `Ctrl+C` y avisa al programa principal para terminar la ejecución.

### `main()`

Realiza la lectura del archivo, creación de procesos, comunicación mediante pipes, control del límite `K`, manejo de dependencias y finalización de los procesos.

---

## Decisiones de diseño

Se decidió utilizar procesos mediante `fork()` porque es uno de los objetivos principales de la tarea.

Se utiliza un pipe para que los procesos hijos puedan informar al proceso padre cuando terminan y entregar el ID de la actividad y su estado.

El proceso padre mantiene la información de las actividades y decide cuándo una nueva actividad puede comenzar según sus dependencias y el límite de procesos `K`.

No se utilizan threads ni mecanismos de sincronización basados en threads.
