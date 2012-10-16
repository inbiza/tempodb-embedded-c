#include "tempodb.h"

#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

static char accessKey[ACCESS_KEY_SIZE + 1] = {0};
static char accessSecret[ACCESS_KEY_SIZE + 1] = {0};
static int sock;
static struct sockaddr_in *addr;
static char *ip;
static void tempodb_send(const char *command);
static void tempodb_read_response(char *buffer, const int buffer_size);

static struct sockaddr_in * tempodb_addr(void);
static int tempodb_create_socket(struct sockaddr_in *addr);
static char * tempodb_getip(char *host);

void tempodb_create(const char *key, const char *secret)
{
  strncpy(accessKey, key, ACCESS_KEY_SIZE);
  strncpy(accessSecret, secret, ACCESS_KEY_SIZE);

  addr = tempodb_addr();
  sock = tempodb_create_socket(addr);
}

void tempodb_destroy(void)
{
  free(addr);
  free(ip);
}

static struct sockaddr_in * tempodb_addr(void) {
  int addr_result;
  struct sockaddr_in *remote;
  ip = tempodb_getip(DOMAIN);

  remote = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in *));
  remote->sin_family = AF_INET;
  remote->sin_port = htons(80);
  remote->sin_addr.s_addr = *ip;
  addr_result = inet_pton(AF_INET, ip, (void *)(&(remote->sin_addr.s_addr)));
  if( addr_result < 0)
  {
    perror("Can't set remote->sin_addr.s_addr");
    exit(1);
  }else if(addr_result == 0)
  {
    fprintf(stderr, "%s is not a valid IP address\n", ip);
    exit(1);
  }
  return remote;
}

void tempodb_build_query(char *buffer, const size_t buffer_size, const char *verb, const char *path, const char *payload) {
  char accessCredentials[ACCESS_KEY_SIZE*2 + 2];
  char *encodedCredentials;
  snprintf(accessCredentials, strlen(accessKey) + strlen(accessSecret) + 2, "%s:%s", accessKey, accessSecret);
  printf("accessCredentials: %s\n", accessCredentials);
  encodedCredentials = encode_base64(strlen(accessCredentials), (unsigned char *)accessCredentials);

  snprintf(buffer, buffer_size, "%s %s HTTP/1.0\r\nAuthorization: Basic %s\r\nUser-Agent: tempodb-embedded-c/1.0.0\r\nHost: %s\r\nContent-Length: %ld\r\nContent-Type: application/json\r\n\r\n%s", verb, path, encodedCredentials, DOMAIN, strlen(payload), payload);
  free(encodedCredentials);
}

static int tempodb_create_socket(struct sockaddr_in *addr) {
  int sock;
  if((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
    perror("Can't create TCP socket");
    exit(1);
  }
  if(connect(sock, (struct sockaddr *)addr, sizeof(struct sockaddr)) < 0){
    perror("Could not connect");
    exit(1);
  }
  return sock;
}

static char * tempodb_getip(char *host)
{
  struct hostent *hent;
  int iplen = 16;
  char *ip = (char *)malloc(iplen+1);
  memset(ip, 0, iplen+1);
  if((hent = gethostbyname(host)) == NULL)
  {
    herror("Can't get IP");
    exit(1);
  }
  if(inet_ntop(AF_INET, (void *)hent->h_addr_list[0], ip, iplen) == NULL)
  {
    perror("Can't resolve host");
    exit(1);
  }
  return ip;
}

static void tempodb_read_response(char *buffer, const int buffer_size) {
  size_t bytes_read = 0;
  size_t bytes_read_part;

  size_t remaining_buffer_size = buffer_size - 1;
  char *remaining_buffer = buffer;

  memset(buffer, 0, buffer_size);

  while ((bytes_read_part = recv(sock, remaining_buffer, remaining_buffer_size, 0)) > 0) {
    bytes_read += bytes_read_part;
    remaining_buffer_size -= bytes_read_part;
    remaining_buffer += bytes_read_part;
  }
}

static void tempodb_send(const char *query) {
  int sent = 0;
  int sent_part;

  while (sent < strlen(query)) {
    sent_part = send(sock, query + sent, strlen(query) - sent, 0);
    if (sent_part == -1) {
      perror("Can't sent query");
      exit(1);
    }
    sent += sent_part;
  }
}

void tempodb_write_by_id(const char *seriesName, const float value, char *response_buffer, const int response_buffer_size) {
  char *queryBuffer = (char *)malloc(512);
  char path[255];
  char bodyBuffer[255];

  snprintf(path, 255, "/v1/series/key/%s/data", seriesName);
  snprintf(bodyBuffer, 255, "[{\"v\":%f}]", value);
  tempodb_build_query(queryBuffer, 512, POST, path, bodyBuffer);
  tempodb_send(queryBuffer);
  tempodb_read_response(response_buffer, response_buffer_size);

  free(queryBuffer);
}
