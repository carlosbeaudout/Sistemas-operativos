#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <cstdlib>

using namespace std;

volatile sig_atomic_t interrupcion = 0;
void manejarSIGINT(int) {
    interrupcion = 1;
}

struct Actividad {
    string id;
    string nombre;
    int tiempo;
    vector<string> dependencias;
    bool terminada = false;
    bool fallida = false;
    pid_t pid = 0;
};

bool dependenciasTerminadas(const Actividad& actividad, const vector<Actividad>& actividades) {

    for (string dependencia : actividad.dependencias) {

        for (const Actividad& a : actividades) {

            if (a.id == dependencia && !a.terminada) {
                return false;
            }
        }
    }

    return true;
}

bool dependenciaFallida(const Actividad& actividad, const vector<Actividad>& actividades) {

    for (string dependencia : actividad.dependencias) {

        for (const Actividad& a : actividades) {

            if (a.id == dependencia && a.fallida) {
                return true;
            }
        }
    }

    return false;
}

int main(int, char* argv[]){

    if (argv[1] == nullptr || argv[2] == nullptr) {
        cout << "Uso: ./planificador plan.txt K" << endl;
        return 1;
    }

    signal(SIGINT, manejarSIGINT);

    ifstream archivo(argv[1]);

    if(!archivo.is_open()){
        cout << "no se pudo abrir el plan.txt" << endl;
        return 1;
    }

    string linea;

    vector<Actividad> actividades;

    int pipefd[2];
    pipe(pipefd);

    int K = stoi(argv[2]);
    if (K <= 0) {
        cout << "K debe ser mayor que 0" << endl;
        return 1;
    }
    int procesosActivos = 0;
    size_t actividadesTerminadas = 0;

    while (getline(archivo, linea))
    {
        Actividad actividad;
        
        stringstream ss(linea);
        string tiempo;
        string dependencias;

        getline(ss, actividad.id, ':');
        actividad.id.pop_back();
        getline(ss, actividad.nombre, ':');
        getline(ss, tiempo, ':');
        getline(ss, dependencias, ':');

        if (tiempo == " " || tiempo.empty()) {
            actividad.tiempo = 100 + rand() % 4901;
        }
        else {
            actividad.tiempo = stoi(tiempo);
        }
        
        stringstream xDependencias(dependencias);
        string dependencia;

        while (getline(xDependencias, dependencia, ',')){
            if (dependencia[0] == ' ') {
            dependencia.erase(0, 1);}

            if (!dependencia.empty()) {
                actividad.dependencias.push_back(dependencia);
            }
        }
        

        actividades.push_back(actividad);
    }

    archivo.close();

    while (actividadesTerminadas < actividades.size()) {

        if (interrupcion) {
            cout << "Interrupcion recibida. Terminando procesos." << endl;

            for (size_t i = 0; i < actividades.size(); i++) {
                if (actividades[i].pid > 0 && !actividades[i].terminada && !actividades[i].fallida) {
                kill(actividades[i].pid, SIGTERM);
                }   
            }
        for (size_t i = 0; i < actividades.size(); i++) {
            if (actividades[i].pid > 0 && !actividades[i].terminada && !actividades[i].fallida) {
                waitpid(actividades[i].pid, nullptr, 0);
            }
        }
        break;
        }

        for (size_t i = 0; i < actividades.size(); i++) {

            if (!actividades[i].terminada && actividades[i].pid == 0 && dependenciaFallida(actividades[i], actividades)) {

                actividades[i].fallida = true;
                actividadesTerminadas++;

                cout << "Actividad cancelada: " << actividades[i].nombre << endl;
            }

            if (!actividades[i].terminada && actividades[i].pid == 0 && dependenciasTerminadas(actividades[i], actividades) && procesosActivos < K) {

                actividades[i].pid = fork();

                if (actividades[i].pid == -1) {
                    cout << "Error al crear el proceso: " << actividades[i].nombre << endl;
                    actividades[i].fallida = true;
                    actividadesTerminadas++;
                    continue;
                }

                if (actividades[i].pid == 0) {
                    cout << "Ejecutando actividad: " << actividades[i].nombre << endl;

                    usleep(actividades[i].tiempo * 1000);

                    int estado = 0;

                    close(pipefd[0]);

                    string mensaje = to_string(getpid()) + ":" + actividades[i].id + ":" + to_string(estado);
                    char buffer[20] = {};

                    for (size_t i = 0; i < mensaje.size() && i < 19; i++) {
                        buffer[i] = mensaje[i];
                    }

                    write(pipefd[1], buffer, 20);

                    return 0;
                }

            procesosActivos++;
            }
        }

        if (procesosActivos > 0) {
            char mensaje[20] = {};

            int bytes = read(pipefd[0], mensaje, sizeof(mensaje));

            if (interrupcion) {
                continue;
            }

            if (bytes <= 0) {
                continue;
            }   

            cout << "ID recibido por pipe: " << mensaje << endl;

            stringstream xMensaje(mensaje);

            string pidTexto;
            string idTerminado;
            string estadoTexto;

            getline(xMensaje, pidTexto, ':');
            getline(xMensaje, idTerminado, ':');
            getline(xMensaje, estadoTexto, ':');

            pid_t terminado = stoi(pidTexto);
            int estado = stoi(estadoTexto);

            waitpid(terminado, nullptr, 0);

            cout << "Termino un proceso con PID: " << terminado << endl;

            for (size_t i = 0; i < actividades.size(); i++) {

                if (actividades[i].id == idTerminado) {

                    if (estado == 0) {
                        actividades[i].terminada = true;
                        cout << "Actividad terminada: " << actividades[i].nombre << endl;
                    }
                    else {
                        actividades[i].fallida = true;
                        cout << "Actividad fallida: " << actividades[i].nombre << endl;
                    }

                    actividadesTerminadas++;
                }
            }

            procesosActivos--;
        }
    }
return 0;
}