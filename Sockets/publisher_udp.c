#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
// Librerías necesarias en Windows para sockets
#include <winsock2.h>
#include <ws2tcpip.h>
SOCKET socket_udp;
// Manejador de Ctrl+C (Windows) para cerrar el socket y limpiar recursos
BOOL WINAPI manejar_ctrl_c(DWORD evento){
    if(evento == CTRL_C_EVENT || evento == CTRL_BREAK_EVENT){
        printf("Cerrando el socket y terminando la ejecucion...\n");
        closesocket(socket_udp);
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
int socket_udp;
// Manejador de señal SIGINT (Ctrl+C en Linux) para cerrar socket correctamente
void manejar_ctrl_c(int signo){
    printf("Cerrando el socket y terminando la ejecucion...\n");
    close(socket_udp);
    exit(0);
    }
#endif

// --------------------------------------------------------
// Función para leer mensaje del usuario y construirlo
// con el formato: PUB|topico|contenido
// --------------------------------------------------------
char* leer_mensaje() {
    // Reservar memoria para el mensaje completo y el contenido ingresado
    char* mensaje = malloc(151 * sizeof(char));
    char* contenido = malloc(151 * sizeof(char));
    if (mensaje == NULL) {
        printf("Error al asignar memoria.\n");
        free(mensaje);
        free(contenido);
        return NULL;
    }
    // Prefijo obligatorio para indicar publicación
    mensaje[0] = 'P';
    mensaje[1] = 'U';
    mensaje[2] = 'B';
    mensaje[3] = '|';

    printf("Ingrese el mensaje a enviar (Con formato 'topico|mensaje' y maximo 150 caracteres): \n");
    fgets(contenido, 151, stdin);

    // Copiar contenido ingresado justo después del "PUB|"
    int i =0;
    int len =strlen(contenido);
    while(i<len-1){
        mensaje[i+4]=contenido[i];
        i++;
    }
    mensaje[i+4]='\0';
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
    char* ip = malloc(16 * sizeof(char)); // Dirección IPv4 (hasta 15 chars + '\0')
    char* puerto = malloc(6 * sizeof(char)); // Puerto (máx 5 dígitos + '\0')

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
    broker.sin_port = htons(atoi(puerto)); // Convertir string a número de puerto
    broker.sin_addr.s_addr = inet_addr(ip); // Convertir string IP a formato binario

    free(ip);
    free(puerto);

    // Crear socket UDP
    socket_udp = socket(AF_INET, SOCK_DGRAM, 0);
    #ifdef _WIN32
    if(socket_udp == INVALID_SOCKET){
        printf("Error al crear el socket. \n");
        WSACleanup();
        return 1;
    }
    #else
    if (socket_udp < 0){
        printf("Error al crear el socket.\n");
        return 1;
    }
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