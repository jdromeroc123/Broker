#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
// Librerías necesarias en Windows para sockets
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
// Hilo que escucha mensajes UDP recibidos
DWORD WINAPI escuchar_mensajes(LPVOID arg) {
    SOCKET socket_udp = *(SOCKET*)arg;
    struct sockaddr_in remitente;
    int tam = sizeof(remitente);
    char buffer[151];

    while (1) {
        int bytes = recvfrom(socket_udp, buffer, 150, 0,
                             (struct sockaddr*)&remitente, &tam);
        buffer[bytes] = '\0';
        if(strcmp(buffer,"")!=0){
            printf("\nMensaje recibido: %s\n> ", buffer);
            fflush(stdout);
        }
    }

    return 0;
}
SOCKET socket_udp; 
HANDLE hilo_receptor; // Manejador del hilo receptor

// Manejador de Ctrl+C (Windows) para cerrar el socket, el hilo y limpiar recursos
BOOL WINAPI manejar_ctrl_c(DWORD evento){
    if(evento == CTRL_C_EVENT || evento == CTRL_BREAK_EVENT){
        printf("Cerrando el socket y terminando la ejecucion...\n");
        closesocket(socket_udp);
        CloseHandle(hilo_receptor);
        WSACleanup();
        exit(0);
        }
    return FALSE;
}
#else 
// Librerías necesarias en Linux/Unix para sockets
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
// Hilo que escucha mensajes UDP recibidos
void* escuchar_mensajes(void* arg) {
    int socket_udp = *(int*)arg;
    struct sockaddr_in remitente;
    socklen_t tam = sizeof(remitente);
    char buffer[151];

    while (1) {
        int bytes = recvfrom(socket_udp, buffer, 150, 0,
                             (struct sockaddr*)&remitente, &tam);
        if (bytes < 0) {
            perror("Error al recibir mensaje");
            continue;
        }
        buffer[bytes] = '\0';
        printf("\n Mensaje recibido: %s\n> ", buffer);
        fflush(stdout);
    }

    return NULL;
}
int socket_udp;
pthread_t hilo_receptor; // ID del hilo receptor
// Manejador de señal SIGINT (Ctrl+C en Linux) para cerrar socket correctamente y terminar programa
void manejar_ctrl_c(int signo){
    printf("Cerrando el socket y terminando la ejecucion...\n");
    close(socket_udp);
    close(hilo_receptor);
    exit(0);
    }
#endif

// --------------------------------------------------------
// Función que lee el tópico del usuario y construye el mensaje SUB
// Formato: "SUB|topico|."
// --------------------------------------------------------
char* leer_mensaje() {
    char* mensaje = malloc(151 * sizeof(char));
    char* contenido = malloc(151 * sizeof(char));
    if (mensaje == NULL) {
        printf("Error al asignar memoria.\n");
        free(mensaje);
        free(contenido);
        return NULL;
    }
    // Prefijo obligatorio para indicar publicación
    mensaje[0] = 'S';
    mensaje[1] = 'U';
    mensaje[2] = 'B';
    mensaje[3] = '|';

    printf("Ingrese el topico al que desea suscribirse: \n");
    fgets(contenido, 151, stdin);

    int i =0;
    int len =strlen(contenido);
    while(i<len-1){
        mensaje[i+4]=contenido[i];
        i++;
    }
    // Copiar contenido ingresado justo después del "PUB|"
    mensaje[i+4]='|'; //Delimitador final
    mensaje[i+5]='.'; //Contenido dummy
    mensaje[i+6]='\0'; //Fin del string
    free(contenido);

    return mensaje;
}

// --------------------------------------------------------
// Función principal
// --------------------------------------------------------
int main(){
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

    // Solicitar dirección IP y puerto del bróker al usuario
    char* ip = malloc(16 * sizeof(char));
    char* puerto = malloc(6 * sizeof(char));

    if (ip == NULL || puerto == NULL){
        printf("Error al asignar memoria para la IP o el puerto del broker.\n");
        free(ip);
        free(puerto);
        return 1;
    }
    printf("Ingrese la direccion IP del broker: \n");
    scanf("%15s", ip);
    printf("Ingrese el puerto del broker: \n");
    scanf("%6s", puerto);
    getchar();// Capturar el '\n' restante del buffer

     // Configurar dirección del bróker
    struct sockaddr_in broker;
    broker.sin_family = AF_INET;
    broker.sin_port = htons(atoi(puerto));
    broker.sin_addr.s_addr = inet_addr(ip);

    free(ip);
    free(puerto);

    // Crear socket UDP e hilo para recibir mensajes entrantes
    socket_udp = socket(AF_INET, SOCK_DGRAM, 0);
    #ifdef _WIN32
    if(socket_udp == INVALID_SOCKET){
        printf("Error al crear el socket. \n");
        WSACleanup();
        return 1;
    }
    hilo_receptor = CreateThread(NULL, 0, escuchar_mensajes, &socket_udp, 0, NULL);
    if(hilo_receptor == NULL) {
        printf("Error al crear el hilo receptor.\n");
        return 1;
    }
    #else
    if (socket_udp < 0){
        printf("Error al crear el socket.\n");
        return 1;
    }
    pthread_create(&hilo_receptor, NULL, escuchar_mensajes, (void*)&socket_udp);
    #endif

    // Bucle principal: leer mensaje del usuario y enviarlo al bróker
    while(1){
        char* mensaje = leer_mensaje();
        if(mensaje!= NULL){
            int resultado = sendto(socket_udp, mensaje, strlen(mensaje), 0, (struct sockaddr*)&broker, sizeof(broker));
            #ifdef _WIN32
            if (resultado == SOCKET_ERROR){
                printf("Error al enviar el mensaje.\n");
            }
            #else
            if (resultado <0){
                printf("Error al enviar el mensaje. \n");
            }
            #endif
        // Liberar memoria luego del envío
        free(mensaje);
        }
    }
    return 0;
}