/*
 * HTTP server routines
 *
 * (c) 2005 bl0rg.net
 */

#ifndef HTTP_H__
#define HTTP_H__

/* Maximal size of HTTP header. */
#define HTTP_MAX_HDR_LEN 8192

/* Seconds before HTTP timeout. */
#define HTTP_TIMEOUT     20

/* Structure used to save information about HTTP clients. */
typedef struct http_client_s {
   int fd, found, in, len;
   char buf[HTTP_MAX_HDR_LEN];
   time_t fini;
} http_client_t;

#define HTTP_MIN_CLIENTS 2

typedef int http_client_callback_t(http_client_t *client, void *data);

typedef struct http_server_s {
  http_client_t *clients;
  unsigned int num_clients;
  unsigned int count_clients;
  unsigned int max_clients;
  http_client_callback_t *callback;
  int fd;
} http_server_t;

void http_server_reset(http_server_t *server);
int  http_server_init(http_server_t *server,
                      unsigned int num_clients, unsigned int max_clients,
                      http_client_callback_t *callback,
                      int fd);
int  http_server_main(http_server_t *server, void *data);
void http_server_close(http_server_t *server);

int http_client_close(http_server_t *server,
                      http_client_t *client);

#endif /* HTTP_H__ */
