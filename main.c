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

#define die(msg) \
  { \
		perror(msg); \
		exit(EXIT_FAILURE);\
	}

#define TRUE 1
#define FALSE !TRUE
#define TABLE_SIZE 512

typedef struct   //stores the info about the client
{
	int acceptfd;
	struct sockaddr_in clientAddrs;
} clientInfo;

typedef struct   //record structure
{
	pid_t pid;
	int status;  //status is currently free or active
	int requestsHandled;
} child_info;

typedef struct
{
  pid_t pid;
	int status;
	char clientIP[64];
	unsigned int port_no;
} message;

child_info children[TABLE_SIZE];    //array that stores the info about childern

int StartServer=2, MinSpareServer, MaxSpareServer, MaxClient, MaxRequestPerChild, listenfd;
unsigned int totalChildren, currentRequests, spareChildren;
int acceptfd;
int sockfd[2]; /* sockfd[0] --> read data, sockfd[1] --> write data */
int port = 4243;

// Function prototypes
void childFunction(void);
void addChildInformation(pid_t, int, int);
int getHandledRequests(pid_t);
int getChildStatus(pid_t);
void deleteChildInformation(pid_t);
void updateChildInformation(pid_t, int);
pid_t getChildPIDtoDelete(void);
void printServerVariables(void);
void printChildrenInformation(void);
void createSocket(void);
void bindSocket(int port);
void createServer(int port);
void acceptConnection(clientInfo * c);
int recvMsg(int fd, char buffer[], int size);
int sendMsg(int fd, char buffer[], int size);
void handleClient(int acceptfd);
void createSocketpair(void);
void sendChildMessage(pid_t child_pid, int status, char * clientIP, unsigned int port_no);
void recvChildMessage(message *child_msg);

//signal handlers
void sigClearSocket(int);
void sigPrintChildrenInfo(int);
void sigIgnore(int);

int main(int argc, char *argv[])
{

	// REMOVE THIS COMMENT TO TAKE COMMAND LINE ARGS
	if (argc != 5)
	{
		printf("usage: %s <MinSpareServer> <MaxSpareServer> <MaxClient> <MaxRequestPerClient>\n", argv[0]);
		exit(-1);
	}

	MinSpareServer = atoi(argv[1]);
	MaxSpareServer = atoi(argv[2]);
	MaxClient = atoi(argv[3]);
	MaxRequestPerChild = atoi(argv[4]);

	signal(SIGINT, sigPrintChildrenInfo);    //ctrl-C handles the printing info of all children

	// Initialise table
	memset(children, -1, sizeof(children));

	createServer(port);  //a listening socket is created here
	createSocketpair();
	int i;
	pid_t cpid;
	printf("Parent pid = %d\n", getpid());
	for (i = 0; i < StartServer; i++)   //initially creates 2 children
	{
		if ((cpid = fork()) == -1)
			die("fork() failed, exiting");
		if (cpid == 0) // child is created here
		{
			childFunction();  //accepts a connection b/w child and client
			exit(0);
		}
		addChildInformation(cpid, 0, 0);   //Number of child server processes created at startup
		//printf("child pid = %d, child status = %d and requests = %d\n", cpid, getChildStatus(cpid), getHandledRequests(cpid));
	}

	printChildrenInformation();

	message msg;
	pid_t cp;
	totalChildren = StartServer;
	currentRequests = 0;

	spareChildren = totalChildren - currentRequests;
	printf("Initial Server Configuration !!\n");
	printf("MinSpareServer = %d\tMaxSpareServer = %d\ttotalChildren = %d\tcurrentRequests = %d\tspareChildren = %d !!\n", MinSpareServer, MaxSpareServer,
			totalChildren, currentRequests, spareChildren);
	printf("\nServer log !!! \n");
	int status;
	while (1) // Parent is now waiting for child's reply and self regulated logic
	{
		// Self regulated logic

		printf("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
		spareChildren = totalChildren - currentRequests;
		printf("\n");

		/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		printChildrenInformation();
		printf("\n");
		memset(&msg, 0, sizeof(msg));
		//recvChildMessage(&msg);
		read(sockfd[0], &msg, sizeof(message));  //read msg from child

		if (msg.status == 1)
			printf("child (%d) is handling Client @ %s on port_number = %d\n", msg.pid, msg.clientIP, msg.port_no);
		else
			printf("Child (%d) has finished client request with ReqsHandled = %d!!\n", msg.pid, getHandledRequests(msg.pid));

		// Store this child struct in table..
		updateChildInformation(msg.pid, msg.status);
		if (getChildStatus(msg.pid) != 1) // Free child
		{
			if (getHandledRequests(msg.pid) >= MaxRequestPerChild)  //find number of requests handled by the child total
			{
				printf("MaxRequestsPerChild --> Child to be killed = %d\n", msg.pid);

				deleteChildInformation(msg.pid);
				kill(msg.pid, SIGKILL);
				waitpid(msg.pid, &status, 0);
				//printf("(Killed) Child pid = %d and status = %d\n", msg.pid, status);
				cp = fork();    //creating new child
				if (cp == -1)
					die("fork() error, exiting");
				if (cp == 0) // child
				{
					childFunction();
					exit(0);
				}
				else // parent
				{
					addChildInformation(cp, 0, 0);
					printf("Recycling --> Added New Child --> child pid = %d\n\n", cp);
				}
			}
		}

		/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

		if (spareChildren < MinSpareServer) // fork child and call child function
		{
			//printf("totalChildren = %d and MaxClient = %d\n", totalChildren, MaxClient);
			if (totalChildren > MaxClient)  //??
			{
				printf("MaxClient limit exceeded, can't fork new child, wait for a while\n");
			}
			else
			{
				int i = 1;
				while(spareChildren < MinSpareServer)
				{
					totalChildren += i;
					spareChildren += i;
					//printf("Forking a new child !!\n");
					for(int j = 0; j < i; ++j)
					{
						pid_t cp = fork();
						if (cp == -1)
							die("fork() error, exiting");
						if (cp == 0) // child
						{
							childFunction();
							exit(0);
						}
						else
						{ // parent
							addChildInformation(cp, 0, 0);
							printf("Too few spare servers --> New child forked --> ");
							//printf("child pid = %d, child status = %d and requests = %d\n", cp, getChildStatus(cp), getHandledRequests(cp));
							printf("child pid = %d\n", cp);
						}
					}
					printServerVariables();
					if(i != 32)
						i *= 2;
					sleep(1);
				}
			}
		}
		else if (spareChildren > MaxSpareServer) // kill a child
		{
			pid_t p = getChildPIDtoDelete();

			if (p > 0)
			{
				printf("Too many children --> child to be killed = %d\n", p);
				//printf("Child %d is killed !!\n", p);
				totalChildren--;
				spareChildren--;
				deleteChildInformation(p);
				kill(p, SIGKILL);
				waitpid(p, &status, 0);
				printServerVariables();
				//printf("(Killed) Child pid = %d and status = %d\n", p, status);
			}
			//printChildrenInformation();
		}
	}

	while (waitpid(-1, NULL, 0) > 0)
		; // For zombie process
	return 0;
}

void childFunction()
{
	//signal(SIGTERM, sigClearSocket);
	//signal(SIGUSR1, sigIgnore);
	signal(SIGINT, SIG_IGN);
	struct sockaddr_in clientAddrsTemp;
	clientInfo c;
	char clientIP[64];
	unsigned int port_no;
	while (TRUE)
	{
		acceptConnection(&c);

		clientAddrsTemp = c.clientAddrs;
		strcpy(clientIP, (char *) inet_ntoa(clientAddrsTemp.sin_addr));
		port_no = ntohs(clientAddrsTemp.sin_port);
		printf("connection accepted ---> pid = %d, client's ip = %s, port_no = %d", getpid(), clientIP, port_no);

		sendChildMessage(getpid(), 1, clientIP, port_no); // Send Message to parent --> BUSY (1)
		handleClient(c.acceptfd);  //receives reply from client and sends dummy reply
		sendChildMessage(getpid(), 0, clientIP, port_no); // Send Message to parent --> FREE (0)
		close(acceptfd);
		//pause();
	}
	close(listenfd);
}
void sigClearSocket(int signo)
{
	printf("Child (%d) has received SIGTERM !!\n", getpid());

	close(listenfd);
	shutdown(acceptfd, SHUT_RDWR);

	exit(0);
}

void sigPrintChildrenInfo(int signo)
{
	signal(SIGINT, sigPrintChildrenInfo);
	printf("\n************** Server Status **********************\n");
	int i;
	int activeChildren = 0;
	for (i = 0; i < TABLE_SIZE; i++)
	{
		if (children[i].pid == -1)
			continue;
		else
		{
			printf("child pid = %d, child status = %d and requests = %d\n", children[i].pid, children[i].status, children[i].requestsHandled);
			if (children[i].status == 1)
			{
				activeChildren++;
			}
		}
	}
	printf("Total Children = %d and active children = %d\n", totalChildren, activeChildren);
	printf("*****************************************************\n");
}

void addChildInformation(pid_t pid, int status, int requestHandler)
{
	int i = 0;
	for (i = 0; i < TABLE_SIZE; i++)
	{
		if (children[i].pid == -1)
		{
			children[i].pid = pid;
			children[i].status = status;
			children[i].requestsHandled = requestHandler;
			break;
		}
		else
			continue;
	}
}

pid_t getChildPIDtoDelete()
{
	int i;
	pid_t cpid;
	for (i = 0; i < TABLE_SIZE; i++)
	{
		if (children[i].pid != -1) // value should not be -1
		{
			if (children[i].status != 1) // any client should not served by that child
				cpid = children[i].pid;
			break;
		}
	}
	return cpid;
}

int getHandledRequests(pid_t pid)
{
	int req = -99;
	int i;
	for (i = 0; i < TABLE_SIZE; i++)
	{
		if (children[i].pid == -1)
			continue;
		else if (children[i].pid == pid)
		{
			req = children[i].requestsHandled;
			break;
		}
	}
	return req;
}

int getChildStatus(pid_t pid)
{
	int status = -1;
	int i;
	for (i = 0; i < TABLE_SIZE; i++)
	{
		if (children[i].pid == -1)
			continue;
		else if (children[i].pid == pid)
		{
			status = children[i].status;
			break;
		}
	}
	return status;
}
void deleteChildInformation(pid_t pid)
{
	int i;
	for (i = 0; i < TABLE_SIZE; i++)
	{
		if (children[i].pid == -1)
			continue;
		else if (children[i].pid == pid)
		{
			children[i].pid = -1;
			children[i].status = -1;
			children[i].requestsHandled = -1;
			break;
		}
	}
}
void updateChildInformation(pid_t pid, int status)
{
	int i;
	for (i = 0; i < TABLE_SIZE; i++)
	{
		if (children[i].pid == -1)
		{
			continue;
		}
		if (children[i].pid == pid)
		{
			children[i].status = status;
			if (status == 1)
			{
				children[i].requestsHandled++;
				currentRequests++;
				spareChildren--;
			}
			else
			{
				currentRequests--;
				spareChildren++;
			}
			//printf("child pid = %d, child status = %d and requests = %d\n", children[i].pid, children[i].status, children[i].requestsHandled);
			break;
		}
	}
}
void printChildrenInformation()
{
	int i;
	printf("\n*********************Children Information*************************\n");
	for (i = 0; i < TABLE_SIZE; i++)
	{
		if (children[i].pid == -1)
			continue;
		else
		{
			printf("child pid = %d, child status = %d and requests = %d\n", children[i].pid, children[i].status, children[i].requestsHandled);
		}
	}
	printf("********************************************************************\n");

}
void printServerVariables(void)
{
	//printf("MinSpareServer = %d\tMaxSpareServer = %d\ttotalChildren = %d\tcurrentRequests = %d\tspareChildren = %d !!\n", MinSpareServer, MaxSpareServer,	totalChildren, currentRequests, spareChildren);
	printf("totalChildren = %d\tcurrentRequests = %d\tspareChildren = %d !!\n", totalChildren, currentRequests, spareChildren);
}
void sigIgnore(int signum)
{
	//printf("Inside signal handler --> pid = %d !!\n", getpid());
}
void createSocket()
{
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if (listenfd == -1)
		die("Error while creating Socket, exiting\n");
	printf("Socket successfully created !!\n");
}
void bindSocket(int port)
{
	int reuse = 1;
  if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0)
      die("setsockopt(SO_REUSEADDR) failed");

	struct sockaddr_in serverAddrs;
	memset(&serverAddrs, 0, sizeof(serverAddrs));
	serverAddrs.sin_family = AF_INET;
	serverAddrs.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddrs.sin_port = htons(port);

	if (bind(listenfd, (struct sockaddr *) &serverAddrs, sizeof(serverAddrs)) < 0)
		die("bind() failed, exiting\n");
	printf("bind() successful !!\n");
}
void createServer(int port)
{
	createSocket();
	bindSocket(port);
	if (listen(listenfd, 10) < 0)
		die("listen() failed, exiting");
}
void acceptConnection(clientInfo * c)
{
	int acceptfd;
	unsigned int clientLength;
	clientLength = sizeof(c->clientAddrs);

	acceptfd = accept(listenfd, (struct sockaddr *) &(c->clientAddrs), &clientLength);
	if (acceptfd == -1)
		die("accept() failed, exiting");
	//printf("child (%d) is handling Client @ %s on port_number = %d\n", getpid(), (char *) inet_ntoa(clientAddrs.sin_addr.s_addr), ntohs(clientAddrs.sin_port));
	c->acceptfd = acceptfd;
}

int recvMsg(int fd, char buffer[], int size)
{
	int recvMsgSize;
	if ((recvMsgSize = recv(fd, buffer, size, 0)) < 0)
		die("recv() failed, exiting");
	return recvMsgSize;
}

int sendMsg(int fd, char buffer[], int size)
{
	int sentMsgSize;
	sentMsgSize = send(fd, buffer, size, 0);
	if (sentMsgSize <= 0)
		die("send() failed, exiting");
	return sentMsgSize;
}

void handleClient(int acceptfd)
{
	char echoBuffer[10240];
	int recvMsgSize, sentMsgSize;

	recvMsgSize = recvMsg(acceptfd, echoBuffer, 10240);
	sleep(1);
	char *reply = "HTTP/1.1 200 OK\n";
	sentMsgSize = sendMsg(acceptfd, reply, strlen(reply));
	close(acceptfd);
}
void createSocketpair(void)
{
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockfd) < 0)
	{
		perror("socketpair() failed !!!");
		exit(-1);
	}
}
void sendChildMessage(pid_t child_pid, int status, char * clientIP, unsigned int port_no)
{
	//close(sockfd[0]); // we are not reading anything from parent
	message msg;
	//memset(msg, 0, sizeof(message));
	msg.pid = child_pid;
	msg.status = status;
	strcpy(msg.clientIP, clientIP);
	msg.port_no = port_no;
	if (write(sockfd[1], &msg, sizeof(msg)) < 0)
	{
		perror("error in writing data in UDS socket, exiting");
		exit(-1);
	}
	//close(sockfd[1]);
}
void recvChildMessage(message *child_msg)
{
	memset(child_msg, 0, sizeof(message));
	//close(sockfd[1]); // we are not writing anything to child through socket
	if (read(sockfd[0], child_msg, sizeof(message)) < 0)
	{
		perror("error in reading data from UDS socket, exiting");
		exit(-1);
	}
	//close(sockfd[0]);
}
