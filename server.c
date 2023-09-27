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
void send_error(FILE *fp);
void error_handling(char *message);

int main(int argc, char *argv[]) {
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    socklen_t clnt_adr_size;
    char buf[BUF_SIZE];
    pthread_t t_id;
    const char *listen_ip = "127.0.0.1"; // 默认监听地址
    int port = 8080;                     // 默认监听端口
    const char *document_root = ".";     // 默认主目录
    if (argc < 2) {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }
    if (argc >= 2) {
        listen_ip = argv[1];
    }
    if (argc >= 3) {
        port = atoi(argv[2]);
    }
    if (argc >= 4) {
        document_root = argv[3];
        if (chdir(document_root) != 0) {
            perror("chdir");
            exit(EXIT_FAILURE);
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
        clnt_sock =
            accept(serv_sock, (struct sockaddr *)&clnt_adr, &clnt_adr_size);
        printf("Connection Request : %s:%d\n", inet_ntoa(clnt_adr.sin_addr),
               ntohs(clnt_adr.sin_port));
        pthread_create(&t_id, NULL, request_handler, &clnt_sock);
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
    if (strstr(req_line, "HTTP/") == NULL) {
        send_error(clnt_write);
        fclose(clnt_read);
        fclose(clnt_write);
        return 0;
    }
    strcpy(method, strtok(req_line, " /"));
    strcpy(file_name, strtok(NULL, " /"));
    strcpy(ct, content_type(file_name));
    if (strcmp(method, "GET") != 0) {
        send_error(clnt_write);
        fclose(clnt_read);
        fclose(clnt_write);
        return 0;
    }
    fclose(clnt_read);
    send_data(clnt_write, ct, file_name);
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
        send_error(fp);
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
    strcpy(file_name, file);
    strtok(file_name, ".");
    strcpy(extension, strtok(NULL, "."));

    if (!strcmp(extension, "html") || !strcmp(extension, "htm"))
        return "text/html";
    else if (!strcmp(extension, "png"))
        return "image/png";
    else if (!strcmp(extension, "mp4"))
        return "video/mp4";
    else
        return "text/plain"; // 默认类型，您可以根据需要修改
}
void send_error(FILE *fp) {
    char protocol[] = "HTTP/1.0 400 Bad Request\r\n";
    char server[] = "Server:Linux Web Server \r\n";
    char cnt_len[] = "Content-length:2048\r\n";
    char cnt_type[] = "Content-type:text/html\r\n\r\n";
    char content[] =
        "<html><head><title>NETWORK</title></head>"
        "<body><font size=+5><br>发生错误！ 查看请求文件名和请求方式!"
        "</font></body></html>";
    fputs(protocol, fp);
    fputs(server, fp);
    fputs(cnt_len, fp);
    fputs(cnt_type, fp);
    fflush(fp);
}
void error_handling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
