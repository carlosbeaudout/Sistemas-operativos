# Planificador Dieciochero

## ¿Qué hace el programa?

El programa recibe un archivo con actividades y las ejecuta respetando las dependencias entre ellas.

Cada actividad se ejecuta como un proceso usando `fork()`.

También se usa un pipe para que los procesos avisen al proceso principal cuando terminan.

El programa permite indicar cuántos procesos pueden estar ejecutándose al mismo tiempo.

---

## Cómo compilar

Se utiliza:

```bash
g++ -Wall -Wextra -std=c++17 planificador.cpp -lpthread -o planificador
```

El programa no utiliza threads, pero se mantiene `-lpthread` porque es parte del comando indicado para la tarea.

---

## Cómo ejecutar

El programa se ejecuta con:

```bash
./planificador plan.txt K
```

Por ejemplo:

```bash
./planificador plan.txt 2
```

El número `2` indica que pueden ejecutarse como máximo 2 procesos al mismo tiempo.

---

## Archivo de entrada

El archivo tiene el siguiente formato:

```text
ID : Nombre : Tiempo : Dependencias
```

Ejemplo:

```text
1 : prender_carbon : 500 :
2 : comprar_carne : 1200 :
3 : comprar_pan : 300 :
4 : asar_longaniza : 800 : 1,2
5 : armar_choripan : 250 : 3,4
6 : servir_mesa : 100 : 5
```

Una actividad que no tenga tiempo recibe un tiempo aleatorio entre 100 y 5000 milisegundos.

Las dependencias indican qué actividades deben terminar antes.

Por ejemplo:

```text
4 : asar_longaniza : 800 : 1,2
```

significa que la actividad 4 necesita que terminen las actividades 1 y 2.

---

## Cómo funciona

Primero se lee el archivo y se guardan las actividades.

Después el programa revisa las dependencias de cada actividad.

Cuando una actividad puede comenzar, se crea un proceso con `fork()`.

El programa lleva un contador para no superar el número máximo `K` de procesos.

Cuando un proceso termina, envía un mensaje por el pipe con:

```text
PID:ID:estado
```

Por ejemplo:

```text
79308:1:0
```

El `0` significa que la actividad terminó correctamente.

El proceso principal recibe el mensaje y marca la actividad como terminada.

---

## Si una actividad falla

Si una actividad informa un estado distinto de `0`, se marca como fallida.

Las actividades que dependan de ella también se cancelan.

Las actividades que no dependan de ella pueden seguir ejecutándose.

---

## Ctrl+C

Si se presiona `Ctrl+C`, el programa recibe `SIGINT` y termina los procesos que estén ejecutándose.

---

## Pruebas realizadas

Se probó el programa con:

```bash
./planificador plan.txt 2
```

También se probó el manejo de errores de las actividades y la cancelación de sus dependencias.

Además, se realizó una prueba con 10.000 actividades:

```bash
./planificador plan10000.txt 10
```

Las 10.000 actividades fueron procesadas correctamente.

---

## Funciones principales

### `dependenciasTerminadas()`

Revisa si las dependencias de una actividad ya terminaron.

### `dependenciaFallida()`

Revisa si alguna dependencia falló.

### `manejarSIGINT()`

Se utiliza para recibir `Ctrl+C`.

### `main()`

Se encarga de leer el archivo, crear los procesos, usar el pipe y controlar las actividades.

---

## Decisiones de diseño

Se usó `fork()` porque la tarea pide trabajar con procesos.

Se usó un pipe para que los procesos puedan avisar cuando terminan.

El proceso principal se encarga de revisar las dependencias y decidir qué actividad puede ejecutarse.

No se utilizaron threads.
