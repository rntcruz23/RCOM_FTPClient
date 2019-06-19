#include <stdio.h>

#include <sys/socket.h>

#include <errno.h>

#include <stdlib.h>

#include <string.h>

#include <netdb.h>

#include <arpa/inet.h>

#include <unistd.h>

#include <pthread.h>

#include <semaphore.h>

#include <fcntl.h>

#define error()                                                                \
  {                                                                            \
    fprintf(stderr,                                                            \
            "usage: ./ftp ftp://[<user>:<password>@]<host>/<url-path>\n");     \
    exit(1);                                                                   \
  }

#define FTP 21

char **parseInput(char *input, int *size, int *error);

int checkSubString(char *str, const char *sub, int start);

char *getWord(char *input, int start);

int stopChar(char c);

char **buildAnonymous(char **commands);

int tryLogin(char *user, int sckt, char *buf);

int openConnection(char *host, int port, char *protocol);

int closeConnection(int sckt);

int rcvConnection(int sckt, char *buff, int size, int flags);

int sendConnection(int sckt, char *buf, int size, int flags);

char *getPath(char *input, int *start);

char **order(char **args); /*order command line input to ftp commands*/

int getpsvport(char *buf);

void login(char **commands, int sckt);

char *getFileName(char *buf);

void clear(char **arg, int size) {

  int i;

  for (i = 0; i < size; i++)

    free(arg[i]);

  free(arg);
}

int main(int argc, char **argv) {

  if (argc != 2)
    error();

  int sckt;

  int error = 1;

  int path;

  char **args = parseInput(argv[1], &path, &error);

  char **commands;

  if (!error) {

    clear(args, 5);

    error();
  }

  commands = order(args);

  printf("service: %s\nuser: %s\npass: %s\nhost: %s\nfile: %s\n", args[0],
         args[1], args[2], args[3], args[4]);

  sckt = openConnection(args[3], FTP, "tcp");

  printf("Connected to: %s\n", args[3]);

  char *name = getFileName(args[4]);

  int f = open(name, O_CREAT | O_RDWR);

  char c[2];

  c[1] = c[0] = 0;

  char buf[100];

  int i = 1;

  do {

    rcvConnection(sckt, buf, 100, 0);

  } while (!strstr(buf, "220 "));

  if (tryLogin(commands[0], sckt, buf)) { /* Check for anonymous server */

    printf("Anonymous server\n");

    commands = buildAnonymous(commands); /* Build anonymous login     */

    i = 0;
  }

  while (i < 3) { /* Continue login           */

    sendConnection(sckt, commands[i], strlen(commands[i]), 0);

    rcvConnection(sckt, buf, 100, 0);

    i++;
  }

  int psvPort = getpsvport(buf);

  int psvSocket = openConnection(args[3], psvPort, "tcp");

  printf("Connected to: %s:%d\n", args[3], psvPort);

  sendConnection(sckt, commands[3], strlen(commands[3]), 0);

  while (rcvConnection(psvSocket, &c[0], 1, 0))

    write(f, &c[0], 1);

  printf("File received\n");

  close(f);

  closeConnection(psvSocket);

  rcvConnection(sckt, buf, 100, 0);

  closeConnection(sckt);

  clear(commands, 4);

  clear(args, 5);

  free(name);
}

int tryLogin(char *user, int sckt,
             char *buf) { /*return 0: normal login - return != 0: anon login*/

  sendConnection(sckt, user, strlen(user), 0);

  rcvConnection(sckt, buf, 100, 0);

  return (!!strstr(buf, "anonymous"));
}

char **buildAnonymous(char **commands) {

  if (!commands)
    return NULL;

  char **orderedInput = (char **)malloc(sizeof(char *) * 4);

  if (!orderedInput)
    return NULL;

  /*Anonymous user: user anonymous*/

  int newsize = strlen("user anonymous\n");

  orderedInput[0] = (char *)malloc(sizeof(char) * newsize);

  orderedInput[0] = strcpy(orderedInput[0], "user anonymous\n");

  /*Anonymous pass: pass anonymous@*/

  newsize = strlen("pass anonymous@\n");

  orderedInput[1] = (char *)malloc(sizeof(char) * newsize);

  orderedInput[1] = strcpy(orderedInput[1], "pass anonymous\n");

  /*change to passive: pasv*/

  newsize = strlen("pasv\n");

  orderedInput[2] = (char *)malloc(sizeof(char) * newsize);

  orderedInput[2] = strcpy(orderedInput[2], "pasv\n");

  /*get path: retr <path>*/

  newsize = strlen(commands[3]) + strlen("retr \n");

  orderedInput[3] = (char *)malloc(newsize);

  orderedInput[3] = strcpy(orderedInput[3], "retr ");

  orderedInput[3] = strcat(orderedInput[3], commands[3] + strlen("retr "));

  orderedInput[3] = strcat(orderedInput[3], "\n");

  clear(commands, 4);

  return orderedInput;
}

char *getFileName(char *buf) {

  int size = strlen(buf) - 1;

  int i = size;

  while (buf[i] != '/')
    i--;

  char *name = (char *)malloc(sizeof(char) * (size - i + 1));

  name = strcpy(name, buf + i + 1);

  return name;
}

int getpsvport(char *buf) {

  int i = 0;

  strtok(buf, "(");

  while (i < 4) {

    strtok(NULL, ",");

    i++;
  }

  int byte1 = atoi(strtok(NULL, ","));

  int byte2 = atoi(strtok(NULL, ","));

  return byte1 * 256 + byte2;
}

int rcvConnection(int sckt, char *buf, int size, int flags) {

  int read;

  bzero(buf, size);

  if ((read = recv(sckt, buf, size, flags)) < 0) {

    herror("recv");

    error();
  }

  return read;
}

int sendConnection(int sckt, char *buf, int size, int flags) {

  int read;

  if ((read = send(sckt, buf, size, flags)) < 0) {

    herror("send");

    error();
  }

  return read;
}

char **order(char **args) {

  char **orderedInput = (char **)malloc(sizeof(char *) * 4);

  if (!args || !orderedInput)
    return NULL;

  /*copy user*/

  int newsize = strlen(args[1]) + strlen("user \n");

  orderedInput[0] = (char *)malloc(sizeof(char) * newsize);

  orderedInput[0] = strcpy(orderedInput[0], "user ");

  orderedInput[0] = strcat(orderedInput[0], args[1]);

  orderedInput[0] = strcat(orderedInput[0], "\n");

  /*copy pass*/

  newsize = strlen(args[2]) + strlen("pass \n");

  orderedInput[1] = (char *)malloc(sizeof(char) * newsize);

  orderedInput[1] = strcpy(orderedInput[1], "pass ");

  orderedInput[1] = strcat(orderedInput[1], args[2]);

  orderedInput[1] = strcat(orderedInput[1], "\n");

  /*change to passive*/

  newsize = strlen("pasv\n");

  orderedInput[2] = (char *)malloc(sizeof(char) * newsize);

  orderedInput[2] = strcpy(orderedInput[2], "pasv\n");

  /*get path*/

  newsize = strlen(args[4]) + strlen("retr \n");

  orderedInput[3] = (char *)malloc(newsize);

  orderedInput[3] = strcpy(orderedInput[3], "retr ");

  orderedInput[3] = strcat(orderedInput[3], args[4] + 1);

  orderedInput[3] = strcat(orderedInput[3], "\n");

  return orderedInput;
}

int closeConnection(int sckt) {

  printf("Closing connection\n");

  sendConnection(sckt, "quit\n", 5, 0);

  char buf[100];

  rcvConnection(sckt, buf, 100, 0);

  return close(sckt);
}

int openConnection(char *hostname, int port, char *connectionProtocol) {

  struct hostent *host;

  struct protoent *protocol;

  struct sockaddr_in server_addr;

  int sckt;

  if (!(host = gethostbyname(hostname))) {

    herror("gethostbyname");

    error();
  }

  char *ip = inet_ntoa(*(struct in_addr *)host->h_addr);

  protocol = getprotobyname(connectionProtocol);

  printf("<Host:IP> %s:%s\n", host->h_name, ip);

  /*Sets memory space of server_addr to zero*/

  bzero((char *)&server_addr, sizeof(server_addr));

  server_addr.sin_family = AF_INET;

  server_addr.sin_addr.s_addr =
      inet_addr(ip); /*32 bit Internet address network byte ordered*/

  server_addr.sin_port =
      htons(port); /*server TCP port must be network byte ordered */

  /*Open socket*/

  if ((sckt = socket(server_addr.sin_family, SOCK_STREAM, protocol->p_proto)) <
      0) {

    herror("socket");

    error();
  }

  /*Connect to socket*/

  if (connect(sckt, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {

    herror("connect");

    error();
  }

  return sckt;
}

char **parseInput(char *input, int *size, int *error) {

  int length = strlen(input);

  if (length <= 8)
    return 0;

  char **args = (char **)malloc(sizeof(char *) * 5);

  int i;

  args[0] = (char *)malloc(sizeof(char) * 3);

  for (i = 0; i < 3; i++)

    args[0][i] = input[i];

  if (strcmp(args[0], "ftp"))

    *error = 0;

  if (!checkSubString(input, "://", 3))
    *error = 0;

  if (!(args[1] = getWord(input, 6)))
    *error = 0;

  int nextToken = 6 + strlen(args[1]);

  if (input[nextToken] != ':')
    *error = 0;

  if (!(args[2] = getWord(input, nextToken + 1)))
    *error = 0;

  nextToken = nextToken + strlen(args[2]) + 2;

  if (input[nextToken - 1] != '@')
    *error = 0;

  if (!(args[3] = getWord(input, nextToken)))
    *error = 0;

  nextToken = nextToken + strlen(args[3]);

  if (!(args[4] = getPath(input, &nextToken)))
    *error = 0;

  *size = nextToken;

  *error *= 1;

  return args;
}

char *getPath(char *input, int *start) {

  int pathsize = strlen(input) - *start;

  char *path = (char *)malloc(sizeof(char) * pathsize);

  if (!(path = strcpy(path, input + *start))) {

    free(path);

    return NULL;
  }

  if (path[pathsize - 1] == '/') {

    printf("%s is a directory\n", path);

    free(path);

    return NULL;
  }

  *start = pathsize;

  return path;
}

int checkSubString(char *str, const char *sub, int start) {

  int i, end = strlen(sub);

  for (i = start; i < start + end; i++)

    if (str[i] != sub[i - start])

      return 0;

  return 1;
}

char *getWord(char *input, int start) {

  int i = start;

  int size = 100;

  char *nextWord = (char *)malloc(sizeof(char) * size);

  while (!stopChar(input[i])) {

    nextWord[i - start] = input[i];

    i++;

    if (i > size) {

      size += 100;

      nextWord = (char *)realloc(nextWord, size);
    }
  }

  if (i == start) {

    free(nextWord);

    return NULL;
  }

  return nextWord;
}

int stopChar(char c) {

  return ((c == ']') || (c == ':') || (c == '/') || (c == '\n') || (c == '@') ||
          (c == ' '));
}
