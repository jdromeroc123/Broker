#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
SOCKET socket_udp;
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
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
int socket_udp;
void manejar_ctrl_c(int signo){
    printf("Cerrando el socket y terminando la ejecucion...\n");
    close(socket_udp);
    exit(0);
    }
#endif

char* leer_mensaje() {
    char* mensaje = malloc(151 * sizeof(char));
    char* contenido = malloc(151 * sizeof(char));
    if (mensaje == NULL) {
        printf("Error al asignar memoria.\n");
        free(mensaje);
        free(contenido);
        return NULL;
    }
    mensaje[0] = 'P';
    mensaje[1] = 'U';
    mensaje[2] = 'B';
    mensaje[3] = '|';

    printf("Ingrese el mensaje a enviar (Con formato 'topico|mensaje' y maximo 150 caracteres): \n");
    fgets(contenido, 151, stdin);

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

int main(){
    #ifdef _WIN32
    WSADATA wsaData;
    if(WSAStartup(MAKEWORD(2,2), &wsaData) !=0){
        printf("Error al iniciar Winsock. \n");
        return 1;
    }
    SetConsoleCtrlHandler(manejar_ctrl_c, TRUE);
    #else
    signal(SIGINT, manejar_ctrl_c);
    #endif

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
    getchar();

    struct sockaddr_in broker;
    broker.sin_family = AF_INET;
    broker.sin_port = htons(atoi(puerto));
    broker.sin_addr.s_addr = inet_addr(ip);

    free(ip);
    free(puerto);

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
        free(mensaje);
        }
    }
    return 0;
}