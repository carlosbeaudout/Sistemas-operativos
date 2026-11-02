# Komikan Concurrente

## 1. Descripción

El programa implementa un torneo de Komikan en el que cada partida enfrenta a un Puma contra un equipo de doce perros. El servidor Matchmaker organiza las partidas y permite ejecutar varias mesas simultáneamente.

## 2. Compilación

Desde la carpeta donde se encuentra `torneo.cpp`, ejecutar:

```bash
g++ -Wall -Wextra -std=c++17 torneo.cpp -o torneo -pthread
```

## 3. Ejecución

El programa recibe la cantidad de clientes, el máximo de mesas simultáneas y, opcionalmente, un archivo de configuración.

```bash
./torneo N M [config.txt]
```

Ejemplo:

```bash
./torneo 4 2 config.txt
```

- `N`: cantidad de clientes que participarán en el torneo.
- `M`: máximo de mesas que pueden estar activas simultáneamente.
- `config.txt`: archivo opcional con la configuración inicial de las partidas.

Cada dos clientes forman una partida. Si queda un cliente sin pareja, no participa.

Para guardar la salida en un archivo:

```bash
./torneo 4 2 config.txt > salida.txt 2>&1
```

## 4. Configuración

El archivo de configuración puede contener los siguientes parámetros:

```text
min_delay_ms=50
max_delay_ms=300
seed=12345
puma_pos=12
dogs_positions=0,1,2,3,4,5,6,7,8,9,10,11
```

- `min_delay_ms` y `max_delay_ms`: intervalo de espera de los movimientos, en milisegundos.
- `seed`: semilla para la generación de retardos aleatorios.
- `puma_pos`: posición inicial del Puma.
- `dogs_positions`: posiciones iniciales de los doce perros.

Las posiciones iniciales deben ser válidas y no repetirse.

## 5. Funciones implementadas

- **Matchmaker:** organiza los clientes en parejas y asigna las partidas.
- **Ejecución concurrente:** permite que distintas partidas se desarrollen en paralelo, respetando el máximo de mesas establecido.
- **Hilos de piezas:** representan los movimientos del Puma y de los perros.
- **Juez:** supervisa el estado del tablero y determina el resultado de cada partida.
- **Reglas del juego:** el Puma gana al capturar seis perros y los perros ganan si el Puma queda sin movimientos válidos.
- **Sincronización:** utiliza mutexes, variables de condición y semáforos para proteger el tablero, coordinar los hilos y limitar las mesas activas.
- **Manejo de interrupciones:** permite detener el servidor mediante `Ctrl+C` y finalizar las partidas activas de forma ordenada.
- **Manejo de fallos:** contempla la terminación inesperada de un hilo mientras mantiene el mutex del tablero mediante un mutex robusto.

## 6. Justificación del diseño

Se utilizan hilos para permitir la ejecución concurrente de las piezas y las partidas. Los mutexes protegen el estado compartido del tablero para evitar modificaciones simultáneas. Las variables de condición permiten notificar cambios al juez, mientras que el semáforo limita la cantidad de partidas activas.

El Matchmaker separa la organización del torneo de la ejecución de cada partida. Cada mesa tiene su propio tablero y juez, lo que permite que las partidas funcionen de manera independiente.

Además, se contempla la interrupción del servidor y la finalización de los hilos para liberar los recursos utilizados. El mutex robusto permite detectar determinados fallos de hilos y tratar la partida como un abandono.

