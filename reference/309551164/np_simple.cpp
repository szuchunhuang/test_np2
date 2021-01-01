#include <iostream>
#include <sstream>
#include <string>
#include <array>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <map>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <algorithm>
#include <iterator>
#include <signal.h>
#include "np_shell.h"

#define	MAX_ACCEPT	20
#define PORT 		"8000"
#define DEBUG       0 // define same value between files

using namespace std;

int server_socket_TCP(const char *port, int qlen){
    struct sockaddr_in sin;
    int server_socket;
    int opt_val = 1;

    memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons((u_short)atoi(port));

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        printf("fail to create socket: %s\n", port);
		exit(1);
    }
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val)) < 0) { // set SO_REUSEADDR to true
        printf("fail to set socket option\n");
		exit(1);
    }
    bind(server_socket, (struct sockaddr *)&sin, sizeof(sin));
    if (listen(server_socket, qlen) < 0) { // qlen is the queue of pending connections
        printf("fail to listen on port: %s\n", port);
		exit(1);
    }
    return server_socket;
}

int main(int argc, char const *argv[], char* envp[]) {
    signal(SIGCHLD, childHandler);

    string port = PORT;
    struct sockaddr_in fsin;           // client address
    socklen_t alen;                    // length of client address
    int	server_socket, client_socket;  // server & client socket
    
    if (argc == 2) {
        port = argv[1];
    }

    server_socket = server_socket_TCP(port.c_str(), MAX_ACCEPT);
    
    while (true) {
        int status;
        pid_t pid;
        alen = sizeof(fsin);

        client_socket = accept(server_socket, (struct sockaddr *)&fsin, &alen);
        if (client_socket < 0) {
            cout << "accept failed!\n" << endl;
			exit(1);
        }

        if (DEBUG) {
            char *env_str = getenv("PATH");
            if (env_str != NULL) {
                cout << "PATH now is: " << env_str << endl;
            }
        }

        while ((pid = fork()) < 0) {
            waitpid(-1, &status, 0);
        }
        if (pid == 0) { // child process
            // reset env variable
            for (char **env = envp; *env != 0; env++) {
                char *p = strchr(*env, '=');
                *p = 0;
                unsetenv(*env);
            }
            setenv("PATH", "bin:.", 1);
            if (DEBUG) {
                char *env_str = getenv("PATH");
                if (env_str != NULL) {
                    cout << "(server side)in child process PATH turn to: " << env_str << endl;
                }
            }

            dup2(client_socket, STDIN_FILENO);
            dup2(client_socket, STDOUT_FILENO);
            dup2(client_socket, STDERR_FILENO);
            (void) close(server_socket);
            np_shell();
            if (DEBUG) {
                cout << "back to child's original command" << endl;
            }
            exit(0);
        } else {       // parent process
            (void) close(client_socket);
        }
    }
}