#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <random>
#include <chrono>
#include <atomic>
#include <csignal>
#include <cstdlib>
#include <algorithm>
#include <array>
#include <cerrno>
#include <ctime>
#include <semaphore.h>
#include <pthread.h>
#include <stdexcept>
#include <system_error>
#include <cxxabi.h>

using namespace std;

volatile sig_atomic_t detener = 0;

void manejarSIGINT(int) {
    detener = 1;
}

pthread_mutex_t candadoSalida = PTHREAD_MUTEX_INITIALIZER;

struct Config {
    int minimo = 50;
    int maximo = 300;
    unsigned int semilla = 12345;
    int puma = 12;
    vector<int> perros = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
};

struct Tablero {
    pthread_mutex_t candado;
    pthread_cond_t cambio;

    vector<int> casillas = vector<int>(28, -1);
    vector<vector<int>> vecinos = vector<vector<int>>(28);

    bool terminado = false;
    bool pumaGana = false;
    bool perrosGanan = false;
    bool abandono = false;

    int perrosCapturados = 0;
    int movimientos = 0;

    // Pieza que estaba usando el mutex.
    int piezaEnSeccion = -1;

    string motivo;
};

void resolverAbandono(Tablero* t) {
    int piezaFallida = t->piezaEnSeccion;

    t->terminado = true;
    t->abandono = true;

    if (piezaFallida > 0) {
        t->pumaGana = true;
        t->perrosGanan = false;
        t->motivo = "Perro fallo inesperadamente; victoria del Puma por abandono";
    } else if (piezaFallida == 0) {
        t->pumaGana = false;
        t->perrosGanan = true;
        t->motivo = "Puma fallo inesperadamente; victoria de los perros por abandono";
    } else {
        t->motivo = "Fallo inesperado de un hilo; partida abortada";
    }
}

struct Candado {
    pthread_mutex_t* mutex;
    Tablero* tablero;

    explicit Candado(
        pthread_mutex_t* m,
        Tablero* t = nullptr,
        int pieza = -1
    ) : mutex(m), tablero(t) {

        int resultado = pthread_mutex_lock(mutex);

        if (resultado == EOWNERDEAD) {
            if (tablero != nullptr) {
                resolverAbandono(tablero);
                pthread_cond_broadcast(&tablero->cambio);
            }

            int recuperacion = pthread_mutex_consistent(mutex);

            if (recuperacion != 0) {
                pthread_mutex_unlock(mutex);
                throw system_error(
                    recuperacion,
                    generic_category(),
                    "pthread_mutex_consistent"
                );
            }
        } else if (resultado != 0) {
            throw system_error(
                resultado,
                generic_category(),
                "pthread_mutex_lock"
            );
        }

        if (tablero != nullptr) {
            tablero->piezaEnSeccion = pieza;
        }
    }

    ~Candado() {
        if (mutex != nullptr) {
            if (tablero != nullptr) {
                tablero->piezaEnSeccion = -1;
            }

            pthread_mutex_unlock(mutex);
        }
    }

    Candado(const Candado&) = delete;
    Candado& operator=(const Candado&) = delete;
};

bool inicializarTablero(Tablero& t) {
    pthread_mutexattr_t atributos;

    int resultado = pthread_mutexattr_init(&atributos);

    if (resultado != 0) {
        return false;
    }

    resultado = pthread_mutexattr_setrobust(
        &atributos,
        PTHREAD_MUTEX_ROBUST
    );

    if (resultado != 0) {
        pthread_mutexattr_destroy(&atributos);
        return false;
    }

    resultado = pthread_mutex_init(&t.candado, &atributos);

    pthread_mutexattr_destroy(&atributos);

    if (resultado != 0) {
        return false;
    }

    resultado = pthread_cond_init(&t.cambio, nullptr);

    if (resultado != 0) {
        pthread_mutex_destroy(&t.candado);
        return false;
    }

    return true;
}

sem_t mesasDisponibles;
atomic<int> siguientePartida{1};

string limpiar(string s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == string::npos) return "";

    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

bool leerLista(const string& texto, vector<int>& salida) {
    salida.clear();
    stringstream ss(texto);
    string parte;

    while (getline(ss, parte, ',')) {
        parte = limpiar(parte);
        if (parte.empty()) return false;

        try {
            size_t n;
            int x = stoi(parte, &n);
            if (n != parte.size()) return false;
            salida.push_back(x);
        } catch (...) {
            return false;
        }
    }

    return !salida.empty();
}

bool cargarConfig(const char* ruta, Config& c) {
    ifstream f(ruta);

    if (!f) {
        cerr << "No se pudo abrir el archivo de configuracion: " << ruta << '\n';
        return false;
    }

    string linea;

    while (getline(f, linea)) {
        linea = limpiar(linea);

        if (linea.empty() || linea[0] == '#') continue;

        size_t p = linea.find('=');

        if (p == string::npos) {
            cerr << "Linea invalida en configuracion: " << linea << '\n';
            return false;
        }

        string clave = limpiar(linea.substr(0, p));
        string valor = limpiar(linea.substr(p + 1));

        try {
            if (clave == "min_delay_ms") {
                c.minimo = stoi(valor);
            } else if (clave == "max_delay_ms") {
                c.maximo = stoi(valor);
            } else if (clave == "seed") {
                c.semilla = static_cast<unsigned int>(stoul(valor));
            } else if (clave == "puma_pos") {
                c.puma = stoi(valor);
            } else if (clave == "dogs_positions") {
                if (!leerLista(valor, c.perros)) {
                    cerr << "Lista dogs_positions invalida\n";
                    return false;
                }
            } else {
                cerr << "Parametro desconocido: " << clave << '\n';
                return false;
            }
        } catch (...) {
            cerr << "Valor invalido para " << clave << '\n';
            return false;
        }
    }

    if (c.minimo < 1 || c.maximo < c.minimo || c.maximo > 60000) {
        cerr << "Rango de retardos invalido\n";
        return false;
    }

    if (c.puma < 0 || c.puma > 27 || c.perros.size() != 12) {
        cerr << "La configuracion debe incluir un Puma y 12 perros\n";
        return false;
    }

    vector<int> posiciones = c.perros;
    posiciones.push_back(c.puma);
    sort(posiciones.begin(), posiciones.end());

    if (posiciones.front() < 0 ||
        posiciones.back() > 27 ||
        adjacent_find(posiciones.begin(), posiciones.end()) != posiciones.end()) {
        cerr << "Las posiciones iniciales deben ser distintas y estar entre 0 y 27\n";
        return false;
    }

    return true;
}

void conectar(Tablero& t, int a, int b) {
    t.vecinos[a].push_back(b);
    t.vecinos[b].push_back(a);
}

void crearVecinos(Tablero& t) {
    // Cuadricula 5x5: conexiones horizontales y verticales.
    for (int r = 0; r < 5; ++r) {
        for (int c = 0; c < 5; ++c) {
            int id = r * 5 + c;

            if (c < 4) conectar(t, id, id + 1);
            if (r < 4) conectar(t, id, id + 5);
        }
    }

    // Posiciones superiores: 25 = trono, 26 y 27 = intersecciones superiores.
    conectar(t, 25, 26);
    conectar(t, 25, 27);
    conectar(t, 25, 2);
    conectar(t, 26, 27);
    conectar(t, 26, 0);
    conectar(t, 26, 1);
    conectar(t, 27, 3);
    conectar(t, 27, 4);
}

string nombrePieza(int pieza) {
    if (pieza == 0) return "Puma";
    return "Perro " + to_string(pieza);
}

void imprimirTablero(const Tablero& t, int partida, int mesa) {
    Candado salida(&candadoSalida);

    cout << "\n[Partida " << partida << " | Mesa " << mesa
         << "] Tablero (P=Puma, numeros=perros, .=libre)\n";

    for (int r = 0; r < 5; ++r) {
        for (int c = 0; c < 5; ++c) {
            int x = t.casillas[r * 5 + c];

            if (x == -1) cout << ". ";
            else if (x == 0) cout << "P ";
            else cout << x << ' ';
        }
        cout << '\n';
    }

    auto mostrar = [&](int id) {
        int x = t.casillas[id];

        if (x == -1) return string("libre");
        if (x == 0) return string("Puma");

        return string("perro ") + to_string(x);
    };

    cout << "T(25): " << mostrar(25)
         << " | Superior izquierda(26): " << mostrar(26)
         << " | Superior derecha(27): " << mostrar(27)
         << " | perros capturados: " << t.perrosCapturados
         << " | movimientos: " << t.movimientos << '\n';
}

vector<int> movimientosValidos(const Tablero& t, int pieza) {
    vector<int> opciones;
    int origen = -1;

    for (int i = 0; i < 28; ++i) {
        if (t.casillas[i] == pieza) {
            origen = i;
            break;
        }
    }

    if (origen == -1) return opciones;

    // Movimiento normal a una posicion conectada y libre.
    for (int destino : t.vecinos[origen]) {
        if (t.casillas[destino] == -1) opciones.push_back(destino);
    }

    // El Puma puede saltar un perro si la casilla siguiente esta libre.
    if (pieza == 0) {
        vector<array<int, 3>> lineas;

        for (int r = 0; r < 5; ++r) {
            for (int c = 0; c < 5; ++c) {
                if (c + 2 < 5) {
                    lineas.push_back({r * 5 + c, r * 5 + c + 1, r * 5 + c + 2});
                }

                if (r + 2 < 5) {
                    lineas.push_back({r * 5 + c, (r + 1) * 5 + c, (r + 2) * 5 + c});
                }
            }
        }

        lineas.push_back({25, 26, 0});
        lineas.push_back({25, 27, 4});
        lineas.push_back({26, 1, 6});
        lineas.push_back({27, 3, 8});

        for (const auto& linea : lineas) {
            if (linea[0] == origen &&
                t.casillas[linea[1]] > 0 &&
                t.casillas[linea[2]] == -1) {
                opciones.push_back(linea[2] + 100);
            }

            if (linea[2] == origen &&
                t.casillas[linea[1]] > 0 &&
                t.casillas[linea[0]] == -1) {
                opciones.push_back(linea[0] + 100);
            }
        }
    }

    return opciones;
}

int perroCapturadoEntre(int origen, int destino) {
    for (int r = 0; r < 5; ++r) {
        for (int c = 0; c < 5; ++c) {
            if (c + 2 < 5) {
                int a = r * 5 + c;
                int b = r * 5 + c + 1;
                int d = r * 5 + c + 2;

                if ((a == origen && d == destino) ||
                    (d == origen && a == destino)) {
                    return b;
                }
            }

            if (r + 2 < 5) {
                int a = r * 5 + c;
                int b = (r + 1) * 5 + c;
                int d = (r + 2) * 5 + c;

                if ((a == origen && d == destino) ||
                    (d == origen && a == destino)) {
                    return b;
                }
            }
        }
    }

    const int extras[4][3] = {
        {25, 26, 0},
        {25, 27, 4},
        {26, 1, 6},
        {27, 3, 8}
    };

    for (const auto& linea : extras) {
        if ((linea[0] == origen && linea[2] == destino) ||
            (linea[2] == origen && linea[0] == destino)) {
            return linea[1];
        }
    }

    return -1;
}

void registrarEvento(int partida, int mesa, const string& mensaje) {
    Candado salida(&candadoSalida);
    cout << "[Partida " << partida << " | Mesa " << mesa << "] "
         << mensaje << '\n';
}

void* moverPieza(int partida, int mesa, int pieza, Tablero* t, Config* cfg) {
    try {
        // Pprueba de abandono quitar las rayitas para comprobar :D
        //if (pieza == 1) {
        //   throw runtime_error("Fallo simulado");
        //}
        unsigned int semilla = cfg->semilla +
            static_cast<unsigned int>(partida * 1009 + pieza * 7919 + 1);

        while (!detener) {
            int rango = cfg->maximo - cfg->minimo + 1;
            int retardo = cfg->minimo +
                static_cast<int>(rand_r(&semilla) % static_cast<unsigned int>(rango));

            this_thread::sleep_for(chrono::milliseconds(retardo));

            Candado lock(&t->candado, t, pieza);

            // Prueba con el Perro 1 dejando el mutex bloqueado.
            //if (pieza == 1) {
            //    lock.mutex = nullptr;
            //    lock.tablero = nullptr;
            //    pthread_exit(nullptr);
            //}

            if (t->terminado || detener) break;

            if (pieza > 0) {
                bool sigue = false;

                for (int i = 0; i < 28; ++i) {
                    if (t->casillas[i] == pieza) {
                        sigue = true;
                        break;
                    }
                }

                if (!sigue) break;
            }

            vector<int> opciones = movimientosValidos(*t, pieza);

            if (pieza == 0 && opciones.empty()) {
                t->terminado = true;
                t->perrosGanan = true;
                t->motivo = "Puma acorralado";

                registrarEvento(partida, mesa,
                    "Ganan los perros: el Puma no tiene movimientos validos.");

                pthread_cond_broadcast(&t->cambio);
                break;
            }

            if (opciones.empty()) continue;

            int eleccion = opciones[rand_r(&semilla) % opciones.size()];
            int destino = eleccion;
            bool captura = false;
            int origen = -1;

            for (int i = 0; i < 28; ++i) {
                if (t->casillas[i] == pieza) {
                    origen = i;
                    break;
                }
            }

            if (eleccion >= 100) {
                destino = eleccion - 100;
                captura = true;
            }

            if (origen < 0 || t->casillas[destino] != -1) continue;

            if (captura) {
                int medio = perroCapturadoEntre(origen, destino);

                if (medio < 0 || t->casillas[medio] <= 0) continue;

                t->casillas[medio] = -1;
                ++t->perrosCapturados;
            }

            t->casillas[origen] = -1;
            t->casillas[destino] = pieza;
            ++t->movimientos;

            string mensaje = nombrePieza(pieza) + " mueve " +
                to_string(origen) + " -> " + to_string(destino);

            if (captura) mensaje += " y captura un perro";

            registrarEvento(partida, mesa, mensaje);

            if (t->perrosCapturados >= 6) {
                t->terminado = true;
                t->pumaGana = true;
                t->motivo = "Puma capturo seis perros";

                registrarEvento(partida, mesa,
                    "Gana el Puma: capturo seis perros.");
            }

            imprimirTablero(*t, partida, mesa);
            pthread_cond_broadcast(&t->cambio);

            if (t->terminado) break;
        }
    } catch (const __cxxabiv1::__forced_unwind&) {
        throw;
    } catch (...) {
        Candado lock(&t->candado, t);

        if (!t->terminado) {
            t->terminado = true;
            t->abandono = true;
            t->pumaGana = (pieza > 0);
            t->perrosGanan = (pieza == 0);
            t->motivo = "Fallo interno de una pieza; victoria por abandono del oponente";

            registrarEvento(partida, mesa,
                "Fallo interno en " + nombrePieza(pieza) +
                ". Se aborta la partida.");

            pthread_cond_broadcast(&t->cambio);
        }
    }

    return nullptr;
}

void ejecutarPartida(int partida, int mesa, Config cfg) {
    Tablero tablero;

    if (!inicializarTablero(tablero)) {
        throw runtime_error("No se pudieron inicializar los mutex y condiciones");
    }

    crearVecinos(tablero);

    tablero.casillas[cfg.puma] = 0;

    for (int i = 0; i < 12; ++i) {
        tablero.casillas[cfg.perros[i]] = i + 1;
    }

    {
        Candado salida(&candadoSalida);
        cout << "\n[Matchmaker] Mesa fisica " << mesa
             << " asignada a la partida " << partida
             << " (Puma y equipo de perros).\n";
    }

    imprimirTablero(tablero, partida, mesa);

    // Comprobar si el Puma comienza acorralado.
    if (movimientosValidos(tablero, 0).empty()) {
        tablero.terminado = true;
        tablero.perrosGanan = true;
        tablero.motivo = "Puma acorralado desde el inicio";

        registrarEvento(partida, mesa,
            "Ganan los perros: el Puma comienza acorralado.");
    }

    vector<thread> piezas;

    try {
        // Solo crear los hilos si la partida no ha terminado.
        if (!tablero.terminado) {
            piezas.emplace_back([&] {
                moverPieza(partida, mesa, 0, &tablero, &cfg);
            });

            for (int i = 1; i <= 12; ++i) {
                piezas.emplace_back([&, i] {
                    moverPieza(partida, mesa, i, &tablero, &cfg);
                });
            }
        }
    } catch (...) {
        Candado lock(&tablero.candado, &tablero);
        tablero.terminado = true;
        tablero.abandono = true;
        tablero.motivo = "No se pudieron crear todos los hilos de la partida";
        pthread_cond_broadcast(&tablero.cambio);
    }

    {
    Candado lock(&tablero.candado, &tablero);

    while (!tablero.terminado) {
        if (detener) {
            tablero.terminado = true;
            tablero.abandono = true;
            tablero.motivo = "Servidor interrumpido por Ctrl+C";

            registrarEvento(partida, mesa,
                "Partida detenida por interrupcion del servidor.");

            pthread_cond_broadcast(&tablero.cambio);
            break;
        }

        timespec limite{};
        clock_gettime(CLOCK_REALTIME, &limite);
        limite.tv_nsec += 100000000;

        if (limite.tv_nsec >= 1000000000) {
            ++limite.tv_sec;
            limite.tv_nsec -= 1000000000;
        }

        int resultado = pthread_cond_timedwait(
            &tablero.cambio,
            &tablero.candado,
            &limite
        );

        if (resultado == EOWNERDEAD) {
            resolverAbandono(&tablero);

            int recuperacion = pthread_mutex_consistent(
                &tablero.candado
            );

            if (recuperacion != 0) {
                throw system_error(
                    recuperacion,
                    generic_category(),
                    "pthread_mutex_consistent"
                );
            }

            pthread_cond_broadcast(&tablero.cambio);
        } else if (resultado != 0 && resultado != ETIMEDOUT) {
            throw system_error(
                resultado,
                generic_category(),
                "pthread_cond_timedwait"
            );
        }
    }
}

    for (auto& h : piezas) {
        if (h.joinable()) h.join();
    }

    {
        Candado salida(&candadoSalida);
        cout << "[Juez Partida " << partida << " | Mesa " << mesa
             << "] Partida finalizada: " << tablero.motivo
             << ". Mesa liberada.\n";
    }

    pthread_cond_destroy(&tablero.cambio);
    pthread_mutex_destroy(&tablero.candado);
}

void trabajadorMatchmaker(int mesa, int totalPares, Config cfg) {
    while (!detener) {
        int partida = siguientePartida.fetch_add(1);

        if (partida > totalPares) break;

        while (sem_wait(&mesasDisponibles) == -1) {
            if (errno == EINTR) {
                if (detener) return;
                continue;
            }

            perror("sem_wait");
            return;
        }

        if (detener) {
            sem_post(&mesasDisponibles);
            return;
        }

        try {
            ejecutarPartida(partida, mesa, cfg);
        } catch (...) {
            Candado salida(&candadoSalida);
            cerr << "[Matchmaker] Error inesperado en la partida "
                 << partida << ".\n";
        }

        if (sem_post(&mesasDisponibles) == -1) {
            perror("sem_post");
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 3 || argc > 4) {
        cerr << "Uso: ./torneo N M [config.txt]\n";
        return 1;
    }

    int N = 0;
    int M = 0;

    try {
        N = stoi(argv[1]);
        M = stoi(argv[2]);
    } catch (...) {
        cerr << "N y M deben ser numeros enteros.\n";
        return 1;
    }

    if (N < 0 || M <= 0) {
        cerr << "N debe ser >= 0 y M debe ser > 0.\n";
        return 1;
    }

    Config cfg;

    if (argc == 4 && !cargarConfig(argv[3], cfg)) {
        return 1;
    }

    if (sem_init(&mesasDisponibles, 0, static_cast<unsigned int>(M)) != 0) {
        perror("sem_init");
        return 1;
    }

    struct sigaction accion{};
    accion.sa_handler = manejarSIGINT;
    sigemptyset(&accion.sa_mask);
    accion.sa_flags = 0;

    if (sigaction(SIGINT, &accion, nullptr) != 0) {
        perror("sigaction");
        sem_destroy(&mesasDisponibles);
        return 1;
    }

    int pares = N / 2;

    {
        Candado salida(&candadoSalida);
        cout << "Servidor Matchmaker iniciado. Clientes: " << N
             << ", maximo de mesas: " << M << '\n';

        if (N % 2 != 0) {
            cout << "Aviso: queda un cliente sin pareja y no se le asigna mesa.\n";
        }
    }

    int cantidadTrabajadores = min(M, pares);
    vector<thread> trabajadores;
    trabajadores.reserve(static_cast<size_t>(cantidadTrabajadores));

    try {
        for (int i = 0; i < cantidadTrabajadores; ++i) {
            int numeroMesa = i + 1;

            trabajadores.emplace_back(
                trabajadorMatchmaker, numeroMesa, pares, cfg
            );
        }
    } catch (...) {
        detener = 1;
        cerr << "No se pudieron crear todos los trabajadores del Matchmaker.\n";
    }

    for (auto& h : trabajadores) {
        if (h.joinable()) h.join();
    }

    sem_destroy(&mesasDisponibles);

    {
        Candado salida(&candadoSalida);
        cout << "Servidor finalizado; recursos liberados.\n";
    }

    return 0;
}