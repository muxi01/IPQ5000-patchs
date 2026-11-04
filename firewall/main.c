#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>

#define PORT                8080
#define MAX_PROTECTED_PORTS 64
#define BUFFER_SIZE         8192
#define MAX_CMD_LEN         2048
#define MAX_OUTPUT_LEN      8192
#define MAX_PATH_LEN        256
#define MAX_HEADERS         64

static char  protected_ports[MAX_PROTECTED_PORTS][8];
static int   protected_count = 0;
static char  base_url[MAX_PATH_LEN] = "";

__attribute__((weak)) extern char _binary_fw_html_start;
__attribute__((weak)) extern char _binary_fw_html_end;

typedef struct {
    char name[128];
    char value[4096];
} HttpHeader;

typedef struct {
    char method[16];
    char path[MAX_PATH_LEN];
    char body[BUFFER_SIZE];
    int  body_len;
    HttpHeader headers[MAX_HEADERS];
    int  header_count;
} HttpRequest;

static int server_fd = -1;

static void sig_handler(int sig) {
    (void)sig;
    if (server_fd >= 0) {
        close(server_fd);
    }
    exit(0);
}

static int is_port_protected(const char *port) {
    for (int i = 0; i < protected_count; i++) {
        if (strcmp(protected_ports[i], port) == 0) {
            return 1;
        }
    }
    return 0;
}

static char* url_decode(char *dst, const char *src, int max_len) {
    char *start = dst;
    while (*src && (dst - start) < max_len - 1) {
        if (*src == '%' && isxdigit(*(src+1)) && isxdigit(*(src+2))) {
            char hex[3] = {*(src+1), *(src+2), 0};
            *dst++ = (char)strtol(hex, NULL, 16);
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
    return start;
}

static int parse_http_request(const char *raw, HttpRequest *req) {
    memset(req, 0, sizeof(HttpRequest));
    const char *p = raw;
    const char *end = raw + strlen(raw);

    int n = 0;
    while (p < end && *p != ' ' && *p != '\r' && *p != '\n' && n < 15) {
        req->method[n++] = *p++;
    }
    req->method[n] = '\0';
    if (*p == ' ') {
        p++;
    }

    n = 0;
    while (p < end && *p != ' ' && *p != '\r' && *p != '\n' && n < MAX_PATH_LEN - 1) {
        req->path[n++] = *p++;
    }
    req->path[n] = '\0';

    while (p < end - 1 && !(p[0] == '\r' && p[1] == '\n')) {
        p++;
    }
    if (p < end) {
        p += 2;
    }

    while (p < end && req->header_count < MAX_HEADERS) {
        if (p[0] == '\r' && p[1] == '\n') {
            p += 2;
            break;
        }
        const char *colon = strchr(p, ':');
        if (!colon) {
            break;
        }
        int name_len = colon - p;
        if (name_len > 127) {
            name_len = 127;
        }
        memcpy(req->headers[req->header_count].name, p, name_len);
        req->headers[req->header_count].name[name_len] = '\0';
        p = colon + 1;
        while (p < end && (*p == ' ' || *p == '\t')) {
            p++;
        }
        n = 0;
        while (p < end && *p != '\r' && *p != '\n' && n < 4095) {
            req->headers[req->header_count].value[n++] = *p++;
        }
        req->headers[req->header_count].value[n] = '\0';
        req->header_count++;
        if (p < end - 1 && p[0] == '\r' && p[1] == '\n') {
            p += 2;
        }
    }

    if (p < end) {
        int len = end - p;
        if (len > BUFFER_SIZE - 1) {
            len = BUFFER_SIZE - 1;
        }
        memcpy(req->body, p, len);
        req->body[len] = '\0';
        req->body_len = len;
    }
    return 0;
}

static void json_escape(char *dst, const char *src, int max_len) {
    int n = 0;
    while (*src && n < max_len - 2) {
        switch (*src) {
            case '"':  dst[n++] = '\\'; dst[n++] = '"';  break;
            case '\\': dst[n++] = '\\'; dst[n++] = '\\'; break;
            case '\n': dst[n++] = '\\'; dst[n++] = 'n';  break;
            case '\r': dst[n++] = '\\'; dst[n++] = 'r';  break;
            case '\t': dst[n++] = '\\'; dst[n++] = 't';  break;
            default:   dst[n++] = *src; break;
        }
        src++;
    }
    dst[n] = '\0';
}

static int json_get_string(const char *json, const char *key, char *value, int max_len) {
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *pos = strstr(json, search);
    if (!pos) {
        return -1;
    }
    pos += strlen(search);
    while (*pos && (*pos == ' ' || *pos == ':' || *pos == '\t')) {
        pos++;
    }
    if (*pos != '"') {
        return -1;
    }
    pos++;
    int n = 0;
    while (*pos && *pos != '"' && n < max_len - 1) {
        if (*pos == '\\') {
            pos++;
            if (*pos == 'n') {
                value[n++] = '\n';
            } else if (*pos == 'r') {
                value[n++] = '\r';
            } else if (*pos == 't') {
                value[n++] = '\t';
            } else if (*pos == '\\') {
                value[n++] = '\\';
            } else if (*pos == '"') {
                value[n++] = '"';
            } else {
                value[n++] = '\\';
                value[n++] = *pos;
            }
        } else {
            value[n++] = *pos;
        }
        pos++;
    }
    value[n] = '\0';
    return 0;
}

static int exec_cmd(const char *cmd, char *output, int max_len) {
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        snprintf(output, max_len, "Error: popen failed: %s", strerror(errno));
        return -1;
    }
    int total = 0;
    while (fgets(output + total, max_len - total - 1, fp) != NULL) {
        total = strlen(output);
        if (total >= max_len - 256) {
            break;
        }
    }
    int status = pclose(fp);
    output[max_len - 1] = '\0';
    if (total == 0) {
        output[0] = '\0';
    }
    return WEXITSTATUS(status);
}

static void send_response(int client_fd, int code, const char *status_text,
                          const char *content_type, const char *body) {
    char header[4096];
    int hdr_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n",
        code, status_text, content_type, strlen(body));
    write(client_fd, header, hdr_len);
    write(client_fd, body, strlen(body));
}

static void handle_add_rule(const char *body, char *response, int max_len) {
    char ip[128] = {0}, port[32] = {0}, notes[512] = {0}, action[32] = {0};
    json_get_string(body, "ip", ip, sizeof(ip));
    json_get_string(body, "port", port, sizeof(port));
    json_get_string(body, "notes", notes, sizeof(notes));
    json_get_string(body, "action", action, sizeof(action));

    if (port[0] == '\0') {
        snprintf(response, max_len,
            "{\"success\":false,\"error\":\"Port is required\"}");
        return;
    }

    if (is_port_protected(port)) {
        snprintf(response, max_len,
            "{\"success\":false,\"error\":\"Port %s is protected\"}", port);
        return;
    }

    const char *target = "DROP";
    if (strcmp(action, "ACCEPT") == 0) {
        target = "ACCEPT";
    }

    char cmd[MAX_CMD_LEN];
    int has_ip = (ip[0] != '\0');
    if (notes[0] != '\0') {
        if (has_ip) {
            snprintf(cmd, sizeof(cmd),
                "ip6tables -I INPUT 1 -s %s -p tcp --dport %s "
                "-m comment --comment \"%s\" -j %s 2>&1",
                ip, port, notes, target);
        } else {
            snprintf(cmd, sizeof(cmd),
                "ip6tables -I INPUT 1 -p tcp --dport %s "
                "-m comment --comment \"%s\" -j %s 2>&1",
                port, notes, target);
        }
    } else {
        if (has_ip) {
            snprintf(cmd, sizeof(cmd),
                "ip6tables -I INPUT 1 -s %s -p tcp --dport %s -j %s 2>&1",
                ip, port, target);
        } else {
            snprintf(cmd, sizeof(cmd),
                "ip6tables -I INPUT 1 -p tcp --dport %s -j %s 2>&1",
                port, target);
        }
    }

    char cmd_output[MAX_OUTPUT_LEN] = {0};
    int ret = exec_cmd(cmd, cmd_output, sizeof(cmd_output));

    char escaped_cmd[MAX_CMD_LEN * 2];
    char escaped_out[MAX_OUTPUT_LEN * 2];
    json_escape(escaped_cmd, cmd, sizeof(escaped_cmd));
    json_escape(escaped_out, cmd_output, sizeof(escaped_out));

    snprintf(response, max_len,
        "{\"success\":%s,\"cmd\":\"%s\",\"output\":\"%s\",\"exit_code\":%d}",
        (ret == 0 && cmd_output[0] == '\0') ? "true" : "false",
        escaped_cmd, escaped_out, ret);
}

static void handle_delete_rule(const char *body, char *response, int max_len) {
    char ip[128] = {0}, port[32] = {0}, notes[128] = {0}, action[32] = {0}, num[32] = {0};
    json_get_string(body, "ip", ip, sizeof(ip));
    json_get_string(body, "port", port, sizeof(port));
    json_get_string(body, "notes", notes, sizeof(notes));
    json_get_string(body, "action", action, sizeof(action));
    json_get_string(body, "num", num, sizeof(num));

    if (num[0] != '\0') {
        char cmd[MAX_CMD_LEN];
        snprintf(cmd, sizeof(cmd),
            "ip6tables -D INPUT %s 2>&1", num);
        char cmd_output[MAX_OUTPUT_LEN] = {0};
        int ret = exec_cmd(cmd, cmd_output, sizeof(cmd_output));
        char escaped_cmd[MAX_CMD_LEN * 2];
        char escaped_out[MAX_OUTPUT_LEN * 2];
        json_escape(escaped_cmd, cmd, sizeof(escaped_cmd));
        json_escape(escaped_out, cmd_output, sizeof(escaped_out));
        snprintf(response, max_len,
            "{\"success\":%s,\"cmd\":\"%s\",\"output\":\"%s\",\"exit_code\":%d}",
            (ret == 0 && cmd_output[0] == '\0') ? "true" : "false",
            escaped_cmd, escaped_out, ret);
        return;
    }

    if (port[0] == '\0') {
        snprintf(response, max_len,
            "{\"success\":false,\"error\":\"Port is required\"}");
        return;
    }

    if (is_port_protected(port)) {
        snprintf(response, max_len,
            "{\"success\":false,\"error\":\"Port %s is protected\"}", port);
        return;
    }

    const char *target = "DROP";
    if (strcmp(action, "ACCEPT") == 0) {
        target = "ACCEPT";
    }

    char cmd[MAX_CMD_LEN];
    int has_ip = (ip[0] != '\0');

    if (notes[0] != '\0') {
        if (has_ip) {
            snprintf(cmd, sizeof(cmd),
                "ip6tables -D INPUT -s %s -p tcp --dport %s "
                "-m comment --comment \"%s\" -j %s 2>&1",
                ip, port, notes, target);
        } else {
            snprintf(cmd, sizeof(cmd),
                "ip6tables -D INPUT -p tcp --dport %s "
                "-m comment --comment \"%s\" -j %s 2>&1",
                port, notes, target);
        }
    } else {
        if (has_ip) {
            snprintf(cmd, sizeof(cmd),
                "ip6tables -D INPUT -s %s -p tcp --dport %s -j %s 2>&1",
                ip, port, target);
        } else {
            snprintf(cmd, sizeof(cmd),
                "ip6tables -D INPUT -p tcp --dport %s -j %s 2>&1",
                port, target);
        }
    }

    char cmd_output[MAX_OUTPUT_LEN] = {0};
    int ret = exec_cmd(cmd, cmd_output, sizeof(cmd_output));

    char escaped_cmd[MAX_CMD_LEN * 2];
    char escaped_out[MAX_OUTPUT_LEN * 2];
    json_escape(escaped_cmd, cmd, sizeof(escaped_cmd));
    json_escape(escaped_out, cmd_output, sizeof(escaped_out));

    snprintf(response, max_len,
        "{\"success\":%s,\"cmd\":\"%s\",\"output\":\"%s\",\"exit_code\":%d}",
        (ret == 0 && cmd_output[0] == '\0') ? "true" : "false",
        escaped_cmd, escaped_out, ret);
}

static void handle_list_rules(char *response, int max_len) {
    char output[MAX_OUTPUT_LEN] = {0};
    int ret = exec_cmd("ip6tables -L INPUT -n --line-numbers 2>&1",
                       output, sizeof(output));

    char escaped[MAX_OUTPUT_LEN * 2];
    json_escape(escaped, output, sizeof(escaped));

    snprintf(response, max_len,
        "{\"success\":true,\"output\":\"%s\",\"exit_code\":%d}",
        escaped, ret);
}

static char* read_html_file(const char *filename, char *buf, int max_len) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        return NULL;
    }
    int total = fread(buf, 1, max_len - 1, fp);
    fclose(fp);
    if (total <= 0) {
        return NULL;
    }
    buf[total] = '\0';
    return buf;
}

static void handle_protected_ports(char *response, int max_len) {
    int pos = snprintf(response, max_len, "{\"protected\":[");
    for (int i = 0; i < protected_count; i++) {
        pos += snprintf(response + pos, max_len - pos,
                        "%s\"%s\"", (i > 0 ? "," : ""), protected_ports[i]);
        if (pos >= max_len - 1) {
            break;
        }
    }
    snprintf(response + pos, max_len - pos, "]}");
}

static void serve_html_page(int client_fd) {
    char *html_buf = NULL;

    if (&_binary_fw_html_start != NULL && &_binary_fw_html_end != NULL) {
        size_t size = (size_t)(&_binary_fw_html_end - &_binary_fw_html_start);
        html_buf = malloc(size + 1);
        if (html_buf) {
            memcpy(html_buf, &_binary_fw_html_start, size);
            html_buf[size] = '\0';
        }
    }

    if (!html_buf) {
        html_buf = malloc(BUFFER_SIZE);
        if (!html_buf) {
            const char *err = "{\"error\":\"Out of memory\"}";
            send_response(client_fd, 500, "Internal Server Error",
                          "application/json", err);
            close(client_fd);
            return;
        }
        if (!read_html_file("./fw.html", html_buf, BUFFER_SIZE)) {
            free(html_buf);
            const char *err = "{\"error\":\"Failed to read fw.html\"}";
            send_response(client_fd, 500, "Internal Server Error",
                          "application/json", err);
            close(client_fd);
            return;
        }
    }

    if (base_url[0] != '\0') {
        char *head_close = strstr(html_buf, "</head>");
        if (head_close) {
            int prefix_len = (int)(head_close - html_buf);
            int tail_len   = (int)strlen(head_close);
            int total      = prefix_len + strlen(base_url) + 64 + tail_len;
            char *tmp = malloc(total + 1);
            if (tmp) {
                memcpy(tmp, html_buf, prefix_len);
                int slen = snprintf(tmp + prefix_len, strlen(base_url) + 64,
                                    "<script>var BASE_URL=\"%s\";</script>\n",
                                    base_url);
                memcpy(tmp + prefix_len + slen, head_close, tail_len);
                total = prefix_len + slen + tail_len;
                tmp[total] = '\0';
                send_response(client_fd, 200, "OK", "text/html; charset=utf-8", tmp);
                free(tmp);
                free(html_buf);
                close(client_fd);
                return;
            }
        }
    }

    send_response(client_fd, 200, "OK", "text/html; charset=utf-8", html_buf);
    free(html_buf);
    close(client_fd);
}

static void handle_request(int client_fd) {
    char raw[BUFFER_SIZE];
    ssize_t n = recv(client_fd, raw, sizeof(raw) - 1, 0);
    if (n <= 0) {
        close(client_fd);
        return;
    }
    raw[n] = '\0';

    HttpRequest req;
    parse_http_request(raw, &req);

    char route[128];

    if (strcmp(req.method, "OPTIONS") == 0) {
        const char *hdr =
            "HTTP/1.1 204 No Content\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
            "Access-Control-Allow-Headers: Content-Type\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n"
            "\r\n";
        write(client_fd, hdr, strlen(hdr));
        close(client_fd);
        return;
    }

    printf("method:%s path:%s\n",req.method,req.path);

    snprintf(route, sizeof(route), "%s/index.html", base_url);
    if (strcmp(req.path,"/") == 0 || strcmp(req.path,"/index.html") == 0 || strcmp(req.path, route) == 0) {
        if (strcmp(req.method, "GET") == 0) {
            serve_html_page(client_fd);
            return;
        }
    }

    snprintf(route, sizeof(route), "%s/api/add", base_url);
    if (strcmp(req.method, "POST") == 0 && strcmp(req.path, route) == 0) {
        char response[MAX_OUTPUT_LEN * 2];
        handle_add_rule(req.body, response, sizeof(response));
        send_response(client_fd, 200, "OK", "application/json; charset=utf-8", response);
        close(client_fd);
        return;
    }

    snprintf(route, sizeof(route), "%s/api/delete", base_url);
    if (strcmp(req.method, "POST") == 0 && strcmp(req.path, route) == 0) {
        char response[MAX_OUTPUT_LEN * 2];
        handle_delete_rule(req.body, response, sizeof(response));
        send_response(client_fd, 200, "OK", "application/json; charset=utf-8", response);
        close(client_fd);
        return;
    }

    snprintf(route, sizeof(route), "%s/api/list", base_url);
    if (strcmp(req.method, "POST") == 0 && strcmp(req.path, route) == 0) {
        char response[MAX_OUTPUT_LEN * 2];
        handle_list_rules(response, sizeof(response));
        send_response(client_fd, 200, "OK", "application/json; charset=utf-8", response);
        close(client_fd);
        return;
    }

    snprintf(route, sizeof(route), "%s/api/list", base_url);
    if (strcmp(req.method, "GET") == 0 && strcmp(req.path, route) == 0) {
        char response[MAX_OUTPUT_LEN * 2];
        handle_list_rules(response, sizeof(response));
        send_response(client_fd, 200, "OK", "application/json; charset=utf-8", response);
        close(client_fd);
        return;
    }

    snprintf(route, sizeof(route), "%s/api/protected", base_url);
    if (strcmp(req.method, "GET") == 0 && strcmp(req.path, route) == 0) {
        char response[MAX_OUTPUT_LEN * 2];
        handle_protected_ports(response, sizeof(response));
        send_response(client_fd, 200, "OK", "application/json; charset=utf-8", response);
        close(client_fd);
        return;
    }

    const char *not_found = "{\"error\":\"Not Found\"}";
    send_response(client_fd, 404, "Not Found", "application/json", not_found);
    close(client_fd);
}

static void print_usage(const char *prog) {
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  -p, --port <port>         HTTP server port (default: 8080)\n");
    printf("  -P, --protect <ports>     Protected ports, comma-separated,\n");
    printf("                            read-only from frontend\n");
    printf("                            (e.g. -P 22,80,443)\n");
    printf("  -b, --base-url <url>      Base URL prefix for reverse proxy\n");
    printf("                            (e.g. -b /firewall)\n");
    printf("  -h, --help                Show this help message\n");
    printf("\nExamples:\n");
    printf("  %s                                    # default port 8080\n", prog);
    printf("  %s -p 9090                            # custom port\n", prog);
    printf("  %s -p 9090 -P 22,80,443               # with protected ports\n", prog);
    printf("  %s -p 8080 -P 22,80 -b /firewall       # with base URL\n", prog);
}

int main(int argc, char **argv) {
    int port = PORT;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        if ((strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) && i + 1 < argc) {
            port = atoi(argv[++i]);
            if (port <= 0 || port > 65535) {
                port = PORT;
            }
        }
        else if ((strcmp(argv[i], "-P") == 0 || strcmp(argv[i], "--protect") == 0) && i + 1 < argc) {
            char *list = argv[++i];
            char *token = strtok(list, ",");
            while (token && protected_count < MAX_PROTECTED_PORTS) {
                while (*token == ' ') {
                    token++;
                }
                int len = (int)strlen(token);
                while (len > 0 && token[len - 1] == ' ') {
                    token[--len] = '\0';
                }
                if (len > 0) {
                    strncpy(protected_ports[protected_count], token, 7);
                    protected_ports[protected_count][7] = '\0';
                    protected_count++;
                }
                token = strtok(NULL, ",");
            }
        }
        else if ((strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--base-url") == 0) && i + 1 < argc) {
            strncpy(base_url, argv[++i], MAX_PATH_LEN - 1);
            base_url[MAX_PATH_LEN - 1] = '\0';
            int blen = (int)strlen(base_url);
            while (blen > 0 && base_url[blen - 1] == '/') {
                base_url[--blen] = '\0';
            }
        }
        else {
            fprintf(stderr, "Unknown option: %s\nTry '%s --help' for usage.\n", argv[i], argv[0]);
            return 1;
        }
    }

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGCHLD, SIG_IGN);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("IPv6 Firewall Config Server started.\n");
    printf("Listening on port: %d\n", port);
    if (base_url[0] != '\0') {
        printf("Base URL: %s\n", base_url);
    }
    if (protected_count > 0) {
        printf("Protected ports: ");
        for (int i = 0; i < protected_count; i++) {
            printf("%s%s", protected_ports[i], (i < protected_count - 1) ? "," : "\n");
        }
    }
    printf("Open: http://localhost:%d%s/\n", port, base_url);
    printf("Press Ctrl+C to exit.\n");

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("accept");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        printf("[connect] %s:%d\n", client_ip, ntohs(client_addr.sin_port));

        handle_request(client_fd);
    }

    close(server_fd);
    return 0;
}