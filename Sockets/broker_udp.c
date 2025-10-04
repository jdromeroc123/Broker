#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
// Librerías específicas de Windows para sockets
#include <winsock2.h>
#include <ws2tcpip.h>
SOCKET socket_udp;
// Manejador de señales para cerrar el socket correctamente al presionar Ctrl+C
BOOL WINAPI manejar_ctrl_c(DWORD evento){
    if(evento == CTRL_C_EVENT || evento == CTRL_BREAK_EVENT){
        printf("Cerrando el socket y terminando la ejecucion...\n");
        closesocket(socket_udp);
        WSACleanup(); // Limpieza de recursos de Windows Sockets API
        exit(0);
        }
    return FALSE;
}
#else 
// Librerías específicas de Linux/UNIX
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
int socket_udp;
// Manejador de señal SIGINT (Ctrl+C) para cerrar el socket correctamente
void manejar_ctrl_c(int signo){
    printf("Cerrando el socket y terminando la ejecución...\n");
    close(socket_udp);
    exit(0);
    }
#endif

// Puerto en el que el bróker UDP escuchará los mensajes
#define PUERTO 61626

// --------------------
// Estructuras de datos
// --------------------

// Lista enlazada de suscriptores

struct Suscriptor{
    struct Suscriptor* siguiente;
    struct sockaddr_in addr; // Dirección del suscriptor
};

// Lista enlazada de tópicos
struct Topico{
    char nombre[50];
    struct Suscriptor* suscriptores;
    struct Topico* siguiente;
};

// Puntero al inicio de las listas de tópicos y suscriptores
struct Topico* topicos = NULL;
struct Suscriptor* suscriptores = NULL;

// -------------------------
// Manejo de tópicos
// -------------------------

// Crea un nuevo tópico si no existe y lo agrega a la lista
struct Topico* crear_topico(char* nombre){
    struct Topico* nuevo_topico = malloc(sizeof(struct Topico));
    if (nuevo_topico ==NULL){
        printf("Error al asignar memoria para el nuevo tópico.\n");
        return NULL;
    }
    strncpy(nuevo_topico->nombre, nombre, 50);
    nuevo_topico->suscriptores=NULL;
    nuevo_topico->siguiente = NULL;
    if(topicos == NULL){
        topicos = nuevo_topico;
    }else{
        struct Topico* actual = topicos;
        while(actual->siguiente != NULL){
            actual = actual->siguiente;
        }
        actual->siguiente = nuevo_topico;
    }
    printf("Topico %s creado\n", nombre);
    return topicos;
}

// Busca un tópico en la lista por nombre
struct Topico* buscar_topico(char* nombre){
    struct Topico* actual = topicos;
    while(actual !=NULL){
        if(strcmp(actual->nombre, nombre)==0){
            return actual;
        }
        actual = actual->siguiente;
    }
    return NULL;
}

// --------------------------
// Envío de mensajes por UDP
// --------------------------

// Envía el mensaje a todos los suscriptores del tópico
int enviar_mensaje(struct Topico* topico, char* mensaje){
    struct Suscriptor* actual = topico->suscriptores;
    while(actual!=NULL){
        int resultado = sendto(socket_udp, mensaje, strlen(mensaje), 0, (struct sockaddr*)&actual->addr, sizeof(actual->addr));
            #ifdef _WIN32
            if (resultado == SOCKET_ERROR){
                printf("Error al enviar el mensaje.\n");
            }
            #else
            if (resultado <0){
                printf("Error al enviar el mensaje. \n");
            }
            #endif
            actual=actual->siguiente;
    }
}


// Construye el mensaje final y lo envía a los suscriptores del tópico
int enviar_contenido(char* topico, char* contenido){
    struct Topico* existe = buscar_topico(topico);
                if(existe==NULL){
                    crear_topico(topico);
                }else{
                    strncat(topico, "/", 2);
                    strcat(topico, contenido);
                    enviar_mensaje(existe, topico);
                }
}

// -------------------------------
// Manejo de suscriptores
// -------------------------------

// Inserta un nuevo suscriptor en la lista del tópico
struct Topico* insertar_suscriptor(struct Topico* topico, struct Suscriptor* suscriptor){
    if(topico == NULL || suscriptor == NULL){
        printf("Errror: topico o suscrpitor nulo.\n");
        return NULL;
    }else{
        if(topico->suscriptores == NULL){
            topico->suscriptores = suscriptor;
        }else{
            struct Suscriptor* actual = topico->suscriptores;
            while(actual->siguiente != NULL){
                actual = actual->siguiente;
            }
            actual->siguiente = suscriptor;
        }
    }
    char puerto_str[6];
    sprintf(puerto_str, "%d", ntohs(suscriptor->addr.sin_port));
    printf("Usuario con direccion %s:%s suscrito a topico %s\n", inet_ntoa(suscriptor->addr.sin_addr), puerto_str, topico->nombre);
    return topico;
}

// Crea un nuevo suscriptor a partir de la dirección del remitente
struct Suscriptor* crear_suscriptor(struct sockaddr_in direccion){
    struct Suscriptor* nuevo_suscriptor = malloc(sizeof(struct Suscriptor));
    if (nuevo_suscriptor == NULL){
        return NULL;
    }else{
        nuevo_suscriptor->addr = direccion;
        nuevo_suscriptor->siguiente = NULL;
    }
    char puerto_str[6];
    sprintf(puerto_str, "%d", ntohs(direccion.sin_port));
    printf("Usuario con direccion %s:%s creado\n", inet_ntoa(nuevo_suscriptor->addr.sin_addr), puerto_str);
    return nuevo_suscriptor;
}

// Añade un suscriptor a un tópico existente
struct Topico* suscribir(char* topico, struct sockaddr_in remitente){
    struct Topico* topico_encontrado = buscar_topico(topico);
    if(topico_encontrado==NULL){
        printf("El topico %s no existe\n", topico);
        return NULL;
    }else{
        struct Suscriptor* nuevo_suscriptor = crear_suscriptor(remitente);
        if (nuevo_suscriptor == NULL){
            return NULL;
        }else{
            insertar_suscriptor(topico_encontrado, nuevo_suscriptor);
            return topico_encontrado;
        }
    }
}

// ------------------------------
// Procesamiento de mensajes UDP
// ------------------------------

// Extrae instrucción, tópico y contenido del mensaje recibido

int descomponer_mensaje(char* mensaje,char** instruccion, char** topico, char** contenido){
    *instruccion = malloc(151 * sizeof(char));
    *topico = malloc(151 * sizeof(char));
    *contenido = malloc(151 * sizeof(char));
    if (*topico == NULL || *contenido == NULL || *instruccion == NULL){
        printf("Error al asignar memoria.\n");
        return 1;
    }
    int len = strlen(mensaje);
    int i = 0, j=0, k= 0;
    while(i<len && mensaje[i] != '|'){
        (*instruccion)[i]=mensaje[i];
        i++;
    }
    if (i == len || mensaje[i] != '|') {
        printf("Error: Mensaje no contiene separador para instruccion.\n");
        return 1;
    }
    (*instruccion)[i] = '\0';
    i++;
    while(j<len && mensaje[i]!='|'){
        (*topico)[j]=mensaje[i];
        i++;
        j++;
    }
    if (i == len || mensaje[i] != '|') {
        printf("Error: Mensaje no contiene separador para topico.\n");
        return 1;
    }
    (*topico)[j]='\0';
    i++;
    while(i<len){
        (*contenido)[k]=mensaje[i];
        i++;
        k++;
    }
    (*contenido)[k]='\0';
    return 0;
}

// Recibe un mensaje UDP, obtiene dirección del remitente y descompone el mensaje
int recibir_mensaje(char* mensaje, char** instruccion, char** topico, char** contenido, socklen_t* tamaño, struct sockaddr_in* remitente){
    int bytes_recibidos = recvfrom(socket_udp, mensaje, 151, 0, (struct sockaddr*)remitente, tamaño);
    mensaje[bytes_recibidos] = '\0';
    #ifdef _WIN32
    if (bytes_recibidos == SOCKET_ERROR){
        printf("Error al recibir el mensaje.\n");
        return 1;
    }
    #else
    if(bytes_recibidos <0){
        printf("Error al recibir el mensaje.\n");
        return 1;
    }
    #endif
    return descomponer_mensaje(mensaje, instruccion, topico, contenido);

}

// ------------------------
// Función principal
// ------------------------
int main(){

    struct sockaddr_in broker;
    broker.sin_family = AF_INET;
    broker.sin_port = htons(PUERTO);
    broker.sin_addr.s_addr = INADDR_ANY;

    //Inicialización de sockets según sistema operativo
    #ifdef _WIN32
    // Inicializar la librería Winsock (Windows)
    WSADATA wsaData;
    if(WSAStartup(MAKEWORD(2,2), &wsaData) !=0){
        printf("Error al iniciar Winsock. \n");
        return 1;
    }
    SetConsoleCtrlHandler(manejar_ctrl_c, TRUE);
    #else
    signal(SIGINT, manejar_ctrl_c);
    #endif

    // Creación del socket UDP
    socket_udp = socket(AF_INET, SOCK_DGRAM, 0);
    #ifdef _WIN32
    if(socket_udp == INVALID_SOCKET){
        printf("Error al crear el socket. \n");
        WSACleanup();
        return 1;
    }
    if (bind(socket_udp, (struct sockaddr*)&broker, sizeof(broker)) == SOCKET_ERROR) {
        printf("Error al vincular el socket con el puerto.\n");
        closesocket(socket_udp);
        WSACleanup();
        return 1;
    }
    #else
    if (socket_udp < 0){
        printf("Error al crear el socket.\n");
        return 1;
    }
    if (bind(socket_udp, (struct sockaddr*)&broker, sizeof(broker)) <0) {
        printf("Error al vincular el socket con el puerto.\n");
        close(socket_udp);
        return 1;
    }
    #endif

    printf("Broker UDP funcionando en el puerto %d\n", PUERTO);
    socklen_t tam_direccion =sizeof(struct sockaddr_in);
    struct sockaddr_in remitente;
    // Recibe mensaje y actúa según la instrucción
    while(1){
        char* mensaje = malloc(151* sizeof(char));
        char* instruccion = NULL;
        char* topico = NULL;
        char* contenido = NULL;
        if(recibir_mensaje(mensaje, &instruccion, &topico, &contenido, &tam_direccion, &remitente)== 0){
            if(strcmp(instruccion,"PUB")==0){
                enviar_contenido(topico, contenido);
            }
            else if(strcmp(instruccion,"SUB")==0){
                suscribir(topico, remitente);
            }
        }
        // Liberar memoria dinámica
        free(mensaje);
        free(instruccion);
        free(topico);
        free(contenido);
    }
}