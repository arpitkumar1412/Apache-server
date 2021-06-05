#include <unistd.h> /* fork, close */
#include <stdlib.h> /* exit */
#include <string.h> /* strlen */
#include <stdio.h> /* perror, fdopen, fgets */
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h> /* waitpid */
#include <netdb.h>
#include <signal.h>

int main(){
  int pid;
  for(int i = 0; i<20; ++i){
    if((pid = fork())==0){
      int sd;
      struct sockaddr_in server;

      sd = socket(AF_INET, SOCK_STREAM, 0);

      server.sin_family = AF_INET;
      server.sin_addr.s_addr = inet_addr("127.0.0.1");
      server.sin_port = htons(4243);
      connect(sd, (struct sockaddr*)&server, sizeof(server));
      printf("sent\n");
      exit(0);
    }
    usleep(10000);
  }
  wait(NULL);
  return 0;
}
