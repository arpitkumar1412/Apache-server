# Apache-server
1. Parent process starts up the server and creates a process pool with each child calling accept() call.

2. Server should be self-regulated as per the incoming traffic. Parent should regulate the process pool according to the parameters specified: MinSpareServers, MaxSpareServers. These parameters are specified as command line arguments to the server.

3. The MaxSpareServers is the desired maximum number of idle child server processes. An idle process is one which is not handling a request. If there are more than MaxSpareServers idle, then the parent process will kill off the excess processes.

4. The MinSpareServers is the desired minimum number of idle child server processes. An idle process is one which is not handling a request. If there are fewer than MinSpareServers idle, then the parent process creates new children: It will spawn one, wait a second, then spawn two, wait a second, then spawn four, and it will continue exponentially until it is spawning 32 children per second. It will stop whenever it satisfies the MinSpareServers.

5. Server should recycle the child once it finishes handling MaxRequestsPerChild number of connections. This parameter is also taken as command line parameter.

6. Child waits over listening socket. Whenever it accepts a connection, it prints its pid, client's ip and port. Child receives the HTTP request, sleeps for 1 second, and sends a dummy reply.

7. Whenever a parent makes a change to the process-pool, it prints the number of children in process pool, number of clients being handled, action being taken, post-action status.

8. Use UNIX Domain sockets for any parent-child communication.

9. By sending Ctrl-c signal, parent process prints number of children currently active, and for each child how many clients it has handled.

10. Server takes care of zombie processes.

11. httperf or ab tool can be used to generate traffic to test web server.

12. Further design explanation and features can be found in the describe.pdf file.
