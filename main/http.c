#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freertos/FreeRTOS.h>

#include <esp_wifi.h>
#include <esp_log.h>

#include "storage.h"
#include "utils.h"
#include <lwip/api.h>

#define TAG "ttsonde-http"

enum http_method {
    HTTP_ERROR,
    HTTP_GET,
    HTTP_POST
};

static char *method_to_str(enum http_method m) {
    switch (m) {
        case HTTP_ERROR: return "ERROR";
        case HTTP_GET: return "GET";
        case HTTP_POST: return "POST";
    }
    return "UNKNOWN";
}

struct http_request {
    enum http_method method;
    char path[64];
    char version[16];
    char *data;
    size_t data_len;
};

char http_buffer[8192] = {0};

void handle_index(struct netconn *conn, struct http_request) {
    const char hdr[] = "HTTP/1.1 200 OK\r\nContent-type: text/html\r\n\r\n";
    netconn_write(conn, hdr, sizeof(hdr) - 1, NETCONN_NOFLAG);

    FILE *f = fopen(LITTLEFS_PATH"/index.html", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open index.html");
        return;
    }
    http_buffer[fread(http_buffer, 1, sizeof(http_buffer) - 1, f)] = 0;
    fclose(f);
    netconn_write(conn, http_buffer, strlen(http_buffer), NETCONN_NOFLAG);
}

void handle_map(struct netconn *conn, struct http_request) {
    const char hdr[] = "HTTP/1.1 200 OK\r\nContent-type: text/html\r\n\r\n";
    netconn_write(conn, hdr, sizeof(hdr) - 1, NETCONN_NOFLAG);

    FILE *f = fopen(LITTLEFS_PATH"/map.html", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open map.html");
        return;
    }
    http_buffer[fread(http_buffer, 1, sizeof(http_buffer) - 1, f)] = 0;
    fclose(f);
    netconn_write(conn, http_buffer, strlen(http_buffer), NETCONN_NOFLAG);
}

void handle_config(struct netconn *conn, struct http_request) {
    const char hdr[] = "HTTP/1.1 200 OK\r\nContent-type: text/html\r\n\r\n";
    netconn_write(conn, hdr, sizeof(hdr) - 1, NETCONN_NOFLAG);

    FILE *f = fopen(LITTLEFS_PATH"/config.html", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open config.html");
        return;
    }
    http_buffer[fread(http_buffer, 1, sizeof(http_buffer) - 1, f)] = 0;
    fclose(f);
    netconn_write(conn, http_buffer, strlen(http_buffer), NETCONN_NOFLAG);
}

void handle_config_get(struct netconn *conn, struct http_request) {
    const char hdr[] = "HTTP/1.1 200 OK\r\nContent-type: application/json\r\n\r\n";
    netconn_write(conn, hdr, sizeof(hdr) - 1, NETCONN_NOFLAG);

    FILE *f = fopen(LITTLEFS_PATH"/config.json", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open config.json");
        return;
    }
    http_buffer[fread(http_buffer, 1, sizeof(http_buffer) - 1, f)] = 0;
    fclose(f);
    netconn_write(conn, http_buffer, strlen(http_buffer), NETCONN_NOFLAG);
}

void handle_config_post(struct netconn *conn, struct http_request req) {
    const char hdr[] = "HTTP/1.1 204 No Content\r\n\r\n";

    FILE *f = fopen(LITTLEFS_PATH"/config.json", "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open config.json");
        return;
    }
    fwrite(req.data, 1, req.data_len, f);
    fclose(f);
    netconn_write(conn, hdr, sizeof(hdr) - 1, NETCONN_NOFLAG);

    esp_restart();
}

static struct {
    char *path;
    enum http_method method;
    void (*handler)(struct netconn *conn, struct http_request req);
} pages[] = {
    {"/", HTTP_GET, handle_index},
    {"/map", HTTP_GET, handle_map},
    {"/config", HTTP_GET, handle_config},

    {"/config.json", HTTP_GET, handle_config_get},
    {"/config.json", HTTP_POST, handle_config_post},
};

size_t pages_count = sizeof(pages) / sizeof(*pages);

esp_err_t parse_http_request(struct netconn *conn, struct netbuf **inbuf, struct http_request *req) {
    char *buf;
    u16_t buflen;
    err_t err;

    err = netconn_recv(conn, inbuf);
    if (err != ERR_OK) {
        ESP_LOGE(TAG, "netconn_recv() failed.");
        return ESP_FAIL;
    }

    if (netbuf_data(*inbuf, (void **) &buf, &buflen) != ERR_OK) {
        ESP_LOGE(TAG, "netbuf_data() failed.");
        return ESP_FAIL;
    }

    char *header_end = strstr(buf, "\r\n\r\n");
    if (!header_end) {
        ESP_LOGE(TAG, "Invalid HTTP request: no header terminator");
        return ESP_FAIL;
    }

    *header_end = '\0';
    char *body = header_end + 4;

    char *saveptr;
    char *line = strtok_r(buf, "\r\n", &saveptr);
    if (!line) return ESP_FAIL;

    char *method_str = strtok(line, " ");
    char *path_str = strtok(NULL, " ");
    char *version_str = strtok(NULL, " ");

    if (!method_str || !path_str || !version_str) {
        ESP_LOGE(TAG, "Malformed request line");
        return ESP_FAIL;
    }

    if (strcmp(method_str, "GET") == 0) req->method = HTTP_GET;
    else if (strcmp(method_str, "POST") == 0) req->method = HTTP_POST;
    else {
        req->method = HTTP_ERROR;
        return ESP_FAIL;
    }

    strncpy(req->path, path_str, sizeof(req->path) - 1);
    req->path[strlen(req->path)] = '\0';

    strncpy(req->version, version_str, sizeof(req->version) - 1);
    req->version[strlen(req->version)] = '\0';

    req->data_len = 0;

    while ((line = strtok_r(NULL, "\r\n", &saveptr)) != NULL) {
        if (*line == '\0') continue;
        if (strncasecmp(line, "Content-Length:", 15) == 0) {
            req->data_len = atoi(line + 15);
        }
    }

    req->data = NULL;
    req->data_len = 0;

    if (req->data_len == 0) return ESP_OK;

    char *body_buf = malloc(req->data_len);
    size_t received = 0;

    size_t already = buflen - (body - buf);
    memcpy(body_buf, body, already);
    received += already;

    while (received < req->data_len) {
        struct netbuf *nb;
        if (netconn_recv(conn, &nb) != ERR_OK) break;

        char *b;
        u16_t l;
        netbuf_data(nb, (void**)&b, &l);

        size_t to_copy = req->data_len - received;
        if (l > to_copy) l = to_copy;

        memcpy(body_buf + received, b, l);
        received += l;

        netbuf_delete(nb);
    }

    req->data = body_buf;
    req->data_len = received;

    return ESP_OK;
}

static esp_err_t http_serve(struct netconn *conn) {
    struct netbuf *inbuf;
    struct http_request req;

    ERROR_CHECK(parse_http_request(conn, &inbuf, &req));

    ip_addr_t remote_ip;
    u16_t remote_port;
    netconn_getaddr(conn, &remote_ip, &remote_port, 0);
    ESP_LOGI(TAG, "[ "IPSTR" ] %s %s",
        IP2STR(&remote_ip.u_addr.ip4), method_to_str(req.method), req.path);

    int i;
    for (i = 0; i < pages_count; i++) {
        if (pages[i].method == req.method && strcmp(pages[i].path, req.path) == 0) {
            pages[i].handler(conn, req);
            break;
        }
    }
    if (i == pages_count) {
        const char http_404[] =
            "HTTP/1.1 404 Not Found\r\nContent-type: text/html\r\n\r\n404 NOT FOUND";
        netconn_write(conn, http_404, sizeof(http_404) - 1, NETCONN_NOFLAG);
    }
    if (req.data) free(req.data);
    netconn_close(conn);
    netbuf_delete(inbuf);
    return ESP_OK;
}

static void http_task(void *) {
    struct netconn *conn, *newconn;
    err_t err;
    conn = netconn_new(NETCONN_TCP);
    netconn_bind(conn, NULL, 80);
    netconn_listen(conn);
    do {
        err = netconn_accept(conn, &newconn);
        if (err != ERR_OK) {
            ESP_LOGE(TAG, "netconn_accept() failed.");
            continue;
        }
        if (http_serve(newconn) != ESP_OK) {
            ESP_LOGE(TAG, "http_serve() failed.");
        }
        netconn_delete(newconn);
    } while (1);
    netconn_close(conn);
    netconn_delete(conn);
}

void http_init(void) {
    xTaskCreate(http_task, "http_task", 4096, NULL, 5, NULL);
}
