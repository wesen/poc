/*
 * HTTP server routines
 *
 * (c) 2005 bl0rg.net
 */

#include "conf.h"

#include <assert.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "network.h"
#include "http.h"
#include "misc.h"

static void http_client_init(http_client_t *client);
static int http_handle_client(http_server_t *server,
                              http_client_t *client, void *data);

static void http_server_assert(http_server_t *server) {
  assert(server != NULL);
  assert(server->clients != NULL);
  assert((server->max_clients == 0) ||
         (server->count_clients <= server->max_clients));
  assert(server->count_clients <= server->num_clients);
}

void http_server_reset(http_server_t *server) {
  assert(server != NULL);
  
  server->clients     = NULL;
  server->num_clients = 0;
  server->max_clients = 0;
  server->callback    = NULL;
  server->fd        = -1;
}

int http_server_init(http_server_t *server,
                     unsigned int num_clients,
                     unsigned int max_clients,
                     http_client_callback_t *callback,
                     int fd) {
  assert(server != NULL);
  assert(num_clients > 0);
  assert(fd != -1);

  http_server_reset(server);
  server->clients = malloc(sizeof(http_client_t) * num_clients);
  if (server->clients == NULL)
    return 0;
  server->num_clients = num_clients;
  server->max_clients = max_clients;
  server->callback = callback;
  
  unsigned int i;
  for (i = 0; i < server->num_clients; i++) {
    http_client_init(server->clients + i);
  }
  server->count_clients = 0;
  server->fd = fd;

  return 1;
}

void http_server_close(http_server_t *server) {
  assert(server != NULL);

  if (server->clients != NULL) {
    unsigned int i;
    for (i = 0; i < server->num_clients; i++) {
      if (server->clients[i].fd != -1) {
        close(server->clients[i].fd);
      }
    }
    free(server->clients);
  }
  server->clients       = NULL;
  server->num_clients   = 0;
  server->count_clients = 0;
  server->max_clients   = 0;
  
  if (server->fd != -1)
    close(server->fd);
  server->fd = -1;

  server->callback = NULL;
}

int http_server_realloc(http_server_t *server,
                        unsigned int new_num_clients) {
  http_server_assert(server);
  assert(new_num_clients >= server->count_clients);

  http_client_t *new_clients = malloc(sizeof(http_client_t) * new_num_clients);
  if (new_clients == NULL)
    return 0;
  unsigned int new_count_clients = 0;
  unsigned int i;
  for (i = 0; i < server->num_clients; i++) {
    if (server->clients[i].fd != -1) {
      memcpy(new_clients + new_count_clients,
             server->clients + i,
             sizeof(http_client_t));
      new_count_clients++;
    }
  }
  for (i = new_count_clients; i < new_num_clients; i++)
    http_client_init(new_clients + i);
  
  assert(new_count_clients == server->count_clients);
  free(server->clients);
  server->clients = new_clients;
  server->num_clients = new_num_clients;
  
  return 1;
}

/*
 * Close the connection to client with an error code.
 *
 * Sends back the error code and the comment.
 */
void http_bad_request(http_client_t *client, 
                      unsigned long code,
                      const char *comment, const char *msg) {
   char buf[256];
   int len;

   len = snprintf(buf, 256,
                  "HTTP/1.0 %lu %s\r\nConnection: close\r\n\r\n%s\r\n", 
                  code, comment, msg);
   write(client->fd, buf, len);
}

/*
 * Accept a HTTP client connection.
 *
 * Accept the connection on the listening socket and fill the client
 * structure.
 */
int http_server_accept(http_server_t *server) {
  http_server_assert(server);
  
  unsigned char ip[16];
  unsigned short port;
  int fd, i;
  
  /* Accept the connection. */
  if ((fd = net_tcp4_accept_socket(server->fd, ip, &port)) < 0)
    return 0;

  if (net_tcp4_socket_nonblock(fd) == -1) {
    close(fd);
    return 0;
  }

  if ((server->max_clients == 0) ||
      (server->count_clients < server->max_clients)) {
    if (server->count_clients == server->num_clients) {
      if (!http_server_realloc(server, server->num_clients * 2)) {
        fprintf(stderr, "Could not grow the size of the server structure\n");
        goto exit;
      }
    }
    
    /* Find an empty client structure and fill it with filedescriptor
     * and timeout value
     */
    for (i = 0; i < server->num_clients; i++) {
      if (server->clients[i].fd == -1) {
        server->clients[i].fd = fd;
        server->clients[i].fini = time(NULL) + HTTP_TIMEOUT;
        server->count_clients++;
        return 1;
      }
    }
  }

 exit:
  close(fd);
  
  return 1;
}

/* Check all clients for timeouts. */
void http_server_check(http_server_t *server) {
  http_server_assert(server);
  
  int i;

   for (i = 0; i < server->num_clients; i++) {
      if ((server->clients[i].fd != -1) && 
          (server->clients[i].found < 2) &&
          (time(NULL) >= server->clients[i].fini)) {
        http_client_close(server, server->clients);
      }
   }
}

/* Destroy a client structure. */
int http_client_close(http_server_t *server,
                      http_client_t *client) {
  int retval = 0;
  
  if (client->fd != -1) {
    retval = close(client->fd);
  }
  
  http_client_init(client);
  server->count_clients--;
  
  /* trim down memory size */
  if ((server->count_clients <= (server->num_clients / 4)) &&
      (server->num_clients > HTTP_MIN_CLIENTS)) {
    if (!http_server_realloc(server, server->num_clients / 2)) {
      /* not really critical */
      fprintf(stderr,
              "Could not cut down the size of the server structure\n");
    }
  }

  http_server_assert(server);
  
  return retval;
}

/* Initialise a client structure. */
void http_client_init(http_client_t *client) {
  client->fd    = -1;
  client->found = 0;
  client->in    = 0;
  client->len   = 0;
}

/*
 * Main HTTP server routine.
 *
 * Select on the listening socket and all opened client
 * sockets. Accept incoming connections and call the
 * http_client function on active clients.
 */
int http_server_main(http_server_t *server, void *data) {
  http_server_assert(server);

  int i;
  
  fd_set fds;
  struct timeval tout;
  
  tout.tv_usec = 0;
  tout.tv_sec = 0;
  
  FD_ZERO(&fds);
  
  /* Select the listening HTTP socket. */
  FD_SET(server->fd, &fds);
  
  /* Select all active client sockets. */
  for (i = 0; i < server->num_clients; i++) {
    if (server->clients[i].fd != -1) {
      FD_SET(server->clients[i].fd, &fds);
    }
  }
  
  if (select(FD_SETSIZE, &fds, NULL, NULL, &tout) < 0) {
    return 0;
  }
  
  /* Accept incoming connections. */
  if (FD_ISSET(server->fd, &fds)) {
    if (!http_server_accept(server))
      return 0;
  }
  
  /* Read incoming client data. */
  for (i = 0; i < server->num_clients; i++) {
    if ((server->clients[i].fd != -1) &&
        (FD_ISSET(server->clients[i].fd, &fds))) {
      if (http_handle_client(server, server->clients + i, data) < 0) {
        http_client_close(server, server->clients + i);
      }
    }
  }
  
  /* Check for client timeouts. */
  http_server_check(server);
  
  return 1;
}

/* Read data from client connection. */
static int http_handle_client(http_server_t *server,
                              http_client_t *client, void *data) {
  http_server_assert(server);
  
  int  tmp;
  
  /* Read header data. */
  tmp = read(client->fd, client->buf + client->len, 
	     HTTP_MAX_HDR_LEN - client->len - 5);
  
  if (tmp <= 0)
    return -1;
  
  client->in += tmp;
  
  /* A header was already found. */
  if (client->found >= 2)
    return 0;
  
  now = time(0);
  
  /* Check if the end of header is in the read data. */
  for (; (client->found < 2) && (client->len < client->in);
       ++client->len) {
    if (client->buf[client->len] == '\r')
      continue;
    if (client->buf[client->len] == '\n')
      ++client->found;
    else
      client->found = 0;
  }
  
  /* The client request was too short. */
  if (client->len < 10) {
    http_bad_request(client, 400, "Bad Request", "Not HTTP");
    return -1;
  }
  
  client->buf[client->len] = '\0';
  
  /* Check if the request is a ``GET /'', else discard the request. */
  if (!strncasecmp(client->buf, "GET /", 5)) {
    if (write(client->fd, "HTTP/1.0 200 OK\r\n\r\n", 19) != 19)
      return -1;

    if (server->callback != NULL) {
      if (server->callback(client, data) < 0) {
        http_bad_request(client, 500, "Server Internal Error", "Callback returned error");
        return -1;
      }
    }
    
    return 1;
  } else {
    http_bad_request(client, 400, "Bad Request", "Unsupported HTTP Method");
    return -1;
  }
  
  return 0;
}

