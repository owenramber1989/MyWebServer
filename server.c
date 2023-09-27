#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUF_SIZE 1024
#define SMALL_BUF 100

void *request_handler(void *arg);
void send_data(FILE *fp, char *ct, char *file_name);
char *content_type(char *file);
void send_error(FILE *fp, int status_code);
void error_handling(char *message);

int main(int argc, char *argv[]) {
    int serv_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    socklen_t clnt_adr_size;
    char buf[BUF_SIZE];
    pthread_t t_id;
    const char *listen_ip = "127.0.0.1"; // 默认监听地址
    int port = 8080;                     // 默认监听端口
    const char *document_root = ".";     // 默认主目录
    if (argc < 2 || argc > 7) {
        printf("Usage : %s | -a/A <addr> | -p/P <port> | -d/D <dir> |\n", argv[0]);
        exit(1);
    }
    for (int i = 1; i < argc; i++){
        if (!strcmp(argv[i],"-a") || !strcmp(argv[i],"-A") ){
            i++;
            listen_ip = argv[i];
        } else if (!strcmp(argv[i],"-p") || !strcmp(argv[i],"-P")) {
            i++;
            port = atoi(argv[i]);
        } else if (!strcmp(argv[i],"-d") || !strcmp(argv[i],"-D") ) {
            i++;
            document_root = argv[i];
        if (chdir(document_root) != 0) {
            perror("chdir");
            exit(EXIT_FAILURE);
        }
        }
    }
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = inet_addr(listen_ip);
    serv_adr.sin_port = htons(port);
    if (bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");
    if (listen(serv_sock, 20) == -1)
        error_handling("listen() error");

    while (1) {
        clnt_adr_size = sizeof(clnt_adr);
        int *clnt_sock = malloc(sizeof(int));
            *clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_adr, &clnt_adr_size);
        printf("Connection Request : \n    {   \n      Addr: %s\n      Port: %d\n    }\n", inet_ntoa(clnt_adr.sin_addr),
               ntohs(clnt_adr.sin_port));
        pthread_create(&t_id, NULL, request_handler, clnt_sock);
        pthread_detach(t_id);
    }
    close(serv_sock);
    return 0;
}

void *request_handler(void *arg) {
    int clnt_sock = *((int *)arg);
    char req_line[SMALL_BUF];
    FILE *clnt_read;
    FILE *clnt_write;

    char method[10];
    char ct[15];
    char file_name[30];

    clnt_read = fdopen(clnt_sock, "r");
    clnt_write = fdopen(dup(clnt_sock), "w");
    fgets(req_line, SMALL_BUF, clnt_read);
    printf("    CMD: %s", req_line);
    if (strstr(req_line, "HTTP/") == NULL) {
        printf("    RESULT: 400 BAD REQUEST\n");
        send_error(clnt_write, 400);
        fclose(clnt_read);
        fclose(clnt_write);
        return 0;
    }
    sscanf(req_line, "%[^ ] %[^ ]", method, file_name);
    memmove(file_name,file_name+1,strlen(file_name));
    if (access(file_name, F_OK) == -1) {
        printf("    RESULT: 404 FILE NOT FOUND\n");
        send_error(clnt_write, 404); // Not Found
        fclose(clnt_read);
        fclose(clnt_write);
        return 0;
    }
    strcpy(ct, content_type(file_name));
    if (strcmp(method, "GET") != 0) {
        printf("    RESULT: 400 BAD REQUEST\n");
        send_error(clnt_write, 400);
        fclose(clnt_read);
        fclose(clnt_write);
        return 0;
    }
    fclose(clnt_read);
    send_data(clnt_write, ct, file_name);
    printf("    RESULT: 200 OK\n");
    free(arg);
    return 0;
}
void send_data(FILE *fp, char *ct, char *file_name) {
    char protocol[] = "HTTP/1.1 200 OK\r\n";
    char server[] = "Server:Linux Web Server\r\n";
    // char cnt_len[] = "Content-Length:8192\r\n";
    char cnt_type[SMALL_BUF];
    char buf[BUF_SIZE];
    FILE *send_file;
    size_t read_len;

    sprintf(cnt_type, "Content-Type:%s\r\n\r\n", ct);
    send_file = fopen(file_name, "rb");
    if (send_file == NULL) {
        send_error(fp, 404);
        return;
    }

    // 传输头信息
    fputs(protocol, fp);
    fputs(server, fp);
    // fputs(cnt_len, fp);
    fputs(cnt_type, fp);

    // 传输请求数据
    while ((read_len = fread(buf, 1, BUF_SIZE, send_file)) > 0) {
        fwrite(buf, 1, read_len, fp);
        fflush(fp);
    }
    fclose(fp);
}
char *content_type(char *file) {
    char extension[SMALL_BUF];
    char file_name[SMALL_BUF];
    char* ext;
    strcpy(file_name, file);
    char *token = strtok(file_name, ".");
if (token != NULL) {
    token = strtok(NULL, ".");
    if (token != NULL) {
        strcpy(extension, token);
    } else {
        strcpy(extension, "");
    }
} else {
    strcpy(extension, "");
}

    if (!strcmp(extension, "html") || !strcmp(extension, "htm"))
        return "text/html";
    else if (!strcmp(extension, "png"))
        return "image/png";
    else if (!strcmp(extension, "mp4"))
        return "video/mp4";
    else
        return "text/plain"; // 默认类型，您可以根据需要修改
}
void send_error(FILE *fp, int status_code) {
    char protocol[50];
    char content[100];

    switch(status_code) {
        case 400:
            strcpy(protocol, "HTTP/1.1 400 Bad Request\r\n");
            strcpy(content, "<html><body>400 Bad Request</body></html>");
            break;
        case 404:
            strcpy(protocol, "HTTP/1.1 404 Not Found\r\n");
            strcpy(content, "<html><body>404 Not Found</body></html>");
            break;
        // 添加其他状态码处理逻辑
        default:
            strcpy(protocol, "HTTP/1.1 500 Internal Server Error\r\n");
            strcpy(content, "<html><body>500 Internal Server Error</body></html>");
            break;
    }

    char server[] = "Server:Linux Web Server\r\n";
    char cnt_len[] = "Content-length:2048\r\n";
    char cnt_type[] = "Content-type:text/html\r\n\r\n";

    fputs(protocol, fp);
    fputs(server, fp);
    fputs(cnt_len, fp);
    fputs(cnt_type, fp);
    fputs(content, fp);
    fflush(fp);
}
void error_handling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
