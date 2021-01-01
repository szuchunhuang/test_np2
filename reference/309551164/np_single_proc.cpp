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
#include <ctype.h>
#include "np_shell_single.h"

#define	MAX_ACCEPT 30
#define MAX_PIPE 10000
#define BUFSIZE 1024
#define PORT "8000"
#define DEBUG 0


using namespace std;

// char **user_env[30];
// char **user_env_variable[30];
// int num_empty_index = 0;
string user_pipe_command[MAX_ACCEPT] = {"0"};
// string user_table[MAX_ACCEPT][5] = {0}; // [0]  | [1] |  [2] |   [3]        |      [4] 
//                                         // flag |  ID | name | address:port | belong_socket
                               
// int num_pipe_table[MAX_PIPE][5] = {0};  //   [0]   |   [1]   |      [2]         |     [3]                |     [4]
//                                         //   fd_R  |  fd_w   |   left_counter   |    flag                |  belong_socket
//                                         //   int   |   int   |      int         |   0 or 1 (or sender)   |  int (or receiver)

void welcome_message() {
    cout << "****************************************" << endl;
    cout << "** Welcome to the information server. **" << endl;
    cout << "****************************************" << endl;
}

void broadcast (int client_socket, string line) { 
    int tmp_fd;
    int tmp_id;
    int client_idx;
    int expression = 0;
    int flag;
    int flag_1;
    int fds[2];
    size_t tmp_pos_1;
    size_t tmp_pos_2;
    size_t tmp_found;
    string username;
    string rec_id;
    string rec_name;
    string send_id;
    string send_name;
    string send_socket;
    string client_id;
    string new_line;
    string ip_port;
    string belong_socket;
    string temp;
    string msg;
    char delim = ' ';
    stringstream s(line);
    getline(s, temp, delim);
    for (int k = 0; k < MAX_ACCEPT; k++) {
        if (user_table_flag[k] == 1 && user_table[k][4].compare(to_string(client_socket)) == 0) {
            client_idx = k;
            client_id = user_table[k][1];
            username = user_table[k][2];
            ip_port = user_table[k][3];
            break;
        } 
    }
    
    //assign expression
    if (temp.find("login") != string::npos) {
        expression = 1;
    } else if (temp.find("logout") != string::npos) {
        expression = 2;
    } else if (temp.find("who") != string::npos) {
        expression = 3;
    } else if (temp.find("name") != string::npos) {
        expression = 4;
    } else if (temp.find("tell") != string::npos) {
        expression = 5;
    } else if (temp.find("yell") != string::npos) {
        expression = 6;
    }
    
    switch (expression) {
        case 1: //login
            for (int k = 0; k < MAX_ACCEPT; k++) {
                if (user_table_flag[k] == 1) {
                    tmp_fd = stoi(user_table[k][4]);
                    dup2(tmp_fd, STDOUT_FILENO);
                    cout << "*** User '" << username << "' entered from " << ip_port << ". ***" << endl;
                }
            }
            dup2(client_socket, STDOUT_FILENO);
            break;

        case 2: //logout
            for (int k = 0; k < MAX_ACCEPT; k++) {
                if (user_table_flag[k] == 1 && user_table[k][2].compare(username) != 0) {
                    tmp_fd = stoi(user_table[k][4]);
                    dup2(tmp_fd, STDOUT_FILENO);
                    cout << "*** User '" << username << "' left. ***" << endl;
                }            
            }
            dup2(client_socket, STDOUT_FILENO);
            break;
        
        case 3: //who
            cout << "<ID>\t<nickname>\t<IP:port>\t<indicate me>" << endl;
            for (int k = 0; k < MAX_ACCEPT; k++) {
                if (user_table_flag[k] == 1) {
                    cout << user_table[k][1] << "\t" << user_table[k][2] << "\t" << user_table[k][3];
                    if (user_table[k][4].compare(to_string(client_socket)) == 0) {
                        cout << "\t<-me";
                    }
                    cout << endl;
                }
            }
            break;

        case 4: //name
            flag = 0;
            getline(s, temp, delim);
            username = temp;
            
            for (int k = 0; k < MAX_ACCEPT; k++) {
                if (user_table_flag[k] == 1 && user_table[k][2].compare(username) == 0) {
                    flag = 1;
                    break;
                }
            }
            if (flag == 1) {
                cout << "*** User '" << username << "' already exists. ***" << endl;
            } else {
                user_table[client_idx][2] = username;
                for (int k = 0; k < MAX_ACCEPT; k++) {
                    if (user_table_flag[k] == 1) {
                        tmp_fd = stoi(user_table[k][4]);
                        dup2(tmp_fd, STDOUT_FILENO);
                        cout << "*** User from " << ip_port << " is named '" << username << "'. ***" << endl;
                    }
                }
                dup2(client_socket, STDOUT_FILENO);
            }
            break;
        
        case 5: //tell
            getline(s, temp, delim); 
            rec_id = temp;
            msg = s.str();

            tmp_found = msg.find_first_of(' ');
            msg.erase(0, tmp_found + 1);
            tmp_found = msg.find_first_of(' ');
            msg.erase(0, tmp_found + 1);

            tmp_id = stoi (rec_id);
            if (user_table_flag[tmp_id - 1] == 1) {
                tmp_fd = stoi(user_table[tmp_id - 1][4]);
                dup2(tmp_fd, STDOUT_FILENO);
                cout << "*** " << username << " told you ***: " << msg << endl;
                dup2(client_socket, STDOUT_FILENO);
            } else {
                cout << "*** Error: user #" << rec_id << " does not exist yet. ***" << endl;
            }
            break;

        case 6: //yell
            msg = s.str();
            tmp_found = msg.find_first_of(' ');
            msg.erase(0, tmp_found + 1);
            
            for (int k = 0; k < MAX_ACCEPT; k++) {
                if (user_table_flag[k] == 1) {
                    tmp_fd = stoi(user_table[k][4]);
                    dup2(tmp_fd, STDOUT_FILENO);
                    cout << "*** " << username << " yelled ***: " << msg << endl;
                }            
            }
            dup2(client_socket, STDOUT_FILENO);
            break;

        default: //userpipe
            tmp_pos_1 = line.find('>'); 
            tmp_pos_2 = line.find('<');

            flag = 0;
            flag_1 = 0;
            if (tmp_pos_2 != string::npos) { // process '<'
                // send_id = line[tmp_pos_2 + 1];
                int c = line.find_first_of(' ', tmp_pos_2 + 1); 
                if (c != string::npos) {
                    int d = c - (tmp_pos_2 + 1);
                    send_id = line.substr (tmp_pos_2 + 1, d);
                } else {
                    send_id = line.substr (tmp_pos_2 + 1, string::npos);
                }
                
                for (int k = 0; k < MAX_ACCEPT; k++) {
                    if (user_table_flag[k] == 1 && user_table[k][1].compare(send_id) == 0) {
                        send_socket = user_table[k][4];
                        send_name = user_table[k][2];
                        flag = 1;
                        break;
                    }            
                }
                if (flag == 1) { //user exists
                    for (int k = 0; k < MAX_PIPE; k++) {
                        if (num_pipe_table[k][3] == stoi (send_socket) && num_pipe_table[k][4] == client_socket) {
                            flag_1 = 1;
                            break;
                        }
                    }
                    if (flag_1 == 1) { //successfully receive message
                        new_line = user_pipe_command[stoi(send_id) - 1];
                        for (int k = 0; k < MAX_ACCEPT; k++) {
                            if (user_table_flag[k] == 1) {
                                tmp_fd = stoi(user_table[k][4]);
                                dup2(tmp_fd, STDOUT_FILENO);
                                cout << "*** " << username << " (#" << client_id << ") just received from " << send_name << " (#" << send_id << ") by '" << new_line << "' ***" << endl;
                            }
                        }
                        dup2(client_socket, STDOUT_FILENO);

                    } else {
                        cout << "*** Error: the pipe #" << send_id << "->#" << client_id << " does not exist yet. ***" << endl;
                    }
                } else { // user doesn't exist
                    cout << "*** Error: user #" << send_id <<" does not exist yet. ***" << endl;
                } 
            }

            flag = 0;
            flag_1 = 0;
            if (tmp_pos_1 != string::npos && line[tmp_pos_1 + 1] != ' ') { // process '>'
                // rec_id = line[tmp_pos_1 + 1]; 
                
                int c = line.find_first_of(' ', tmp_pos_1 + 1); 
                if (c != string::npos) {
                    int d = c - (tmp_pos_1 + 1);
                    rec_id = line.substr (tmp_pos_1 + 1, d);
                } else {
                    rec_id = line.substr (tmp_pos_1 + 1, string::npos);
                }


                for (int k = 0; k < MAX_ACCEPT; k++) {
                    if (user_table_flag[k] == 1 && user_table[k][1].compare(rec_id) == 0) {
                        belong_socket = user_table[k][4];
                        rec_name = user_table[k][2];
                        flag = 1;
                        break;
                    }            
                }
                if (flag == 1) { // user exists
                    for (int k = 0; k < MAX_PIPE; k++) {
                        if (num_pipe_table[k][3] == client_socket && num_pipe_table[k][4] == stoi (belong_socket)) {
                            flag_1 = 1;
                            break;
                        }
                    }
                    if (flag_1 == 1) {
                        cout << "*** Error: the pipe #" << client_id << "->#" << rec_id << " already exists. ***" << endl;
                    } else {
                        user_pipe_command[stoi(client_id) - 1] = line;  
                        while (pipe(fds) < 0) {
                            usleep(100);
                        }
                        num_pipe_table[num_empty_index][0] = fds[0];
                        num_pipe_table[num_empty_index][1] = fds[1];
                        num_pipe_table[num_empty_index][2] = -1; // counter not used
                        num_pipe_table[num_empty_index][3] = client_socket; // sender
                        num_pipe_table[num_empty_index][4] = stoi (belong_socket); //receiver
                        
                        // 要找下個沒有使用的空格idx
                        num_empty_index = num_empty_index + 1;
                        while (num_pipe_table[num_empty_index][3]) {
                            num_empty_index = num_empty_index + 1;
                            if (num_empty_index == MAX_PIPE) {
                                num_empty_index = 0;
                            }
                        }
                        
                        for (int k = 0; k < MAX_ACCEPT; k++) {
                            if (user_table_flag[k] == 1) {
                                tmp_fd = stoi(user_table[k][4]);
                                dup2(tmp_fd, STDOUT_FILENO);
                                cout << "*** " << username << " (#" << client_id << ") just piped '" << line << "' to " << rec_name << " (#" << rec_id << ") ***" << endl;
                            }
                        }
                        dup2(client_socket, STDOUT_FILENO);
                    }
                        
                } else { // user doesn't exist
                    cout << "*** Error: user #" << rec_id <<" does not exist yet. ***" << endl;
                }
            }           
            break;
    }
}

int echo(int current_socket) {
    char buf[BUFSIZE];
	int	char_count;
    int char_sum = 0;
    string command;
    string temp;
    char delim = ' ';

    char_count = read(current_socket, buf, BUFSIZE);
    while (char_count == 1024) {
        command.append(buf);
        char_sum += char_count;
        char_count = read(current_socket, buf, BUFSIZE);
    }
    command.append(buf);
    char_sum += char_count;

    if (DEBUG) { 
        cout << "before trim: " << command << endl;
    }

    // trim command
    string whitespace ("\n\r");
    size_t found = command.find_last_of(whitespace);
    while (found != string::npos) {
        command.erase(found, string::npos);
        found = command.find_last_of(whitespace);
    }

    while (command.back() == ' ') { // 若字串尾巴有空白就會刪掉
        command.pop_back();
    }
    if (DEBUG) { 
        cout << "trimmed command: " << command << ": <- test next line" << endl;
    }


    // handle exit and empty input
    if (command.find("exit") != string::npos) { 
        return -1;
    } else if (command.length() == 0) {
        cout << "% " << flush;
	    return char_sum;
    }

    if (command.find("who") != string::npos || command.find("tell") != string::npos || command.find("yell") != string::npos || command.find("name") != string::npos ) {
        if (DEBUG) {
            cout << "exec who, name, tell, or yell" << endl;
        }
        broadcast(current_socket, command);
        
    } else { 
        size_t tmp_pos_1;
        size_t tmp_pos_2;
        tmp_pos_1 = command.find('>'); 
        tmp_pos_2 = command.find('<');
        if (tmp_pos_1 != string::npos && command[tmp_pos_1 + 1] != ' ') { // separate from output to file
            broadcast(current_socket, command);
        } else if (tmp_pos_2 != string::npos) {
            broadcast(current_socket, command);
        }
        if (DEBUG) {
            cout << "about to enter npshell" << endl;
        }
        np_shell(current_socket, command);
        if (DEBUG) {
            cout << "leave npshell" << endl;
        }
    }
    cout << "% " << flush;
	return char_sum;
}

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
    
    pid_t pid;
    int status;
    string port = PORT;
    struct sockaddr_in fsin;                // client address
    socklen_t alen;                         // length of client address
    int	server_socket, client_socket;       // server & client socket
    fd_set read_fdset, activate_fdset;      // file descriptor set
    
    
    int ori_stdin  = dup(STDIN_FILENO);
    int ori_stdout = dup(STDOUT_FILENO);
    int ori_stderr = dup(STDERR_FILENO);

    if (argc == 2) {
        port = argv[1];
    }

    //clear env variable
    int count_env = 0;
    for (char **env = envp; *env != 0; env++) {
        count_env = count_env + 1;
        char *p = strchr(*env, '=');
        *p = 0;
        unsetenv(*env);
    }
    setenv("PATH", "bin:.", 1);
    // if (DEBUG) {
    //     char *env_str = getenv("PATH");
    //     if (env_str != NULL) {
    //         cout << "(server side) PATH turn to: " << env_str << endl;
    //     }
    // } 

    env_size_global = count_env;
    // 複製一份原本的環境變數
    int size_envp = count_env;
    char **ori_envp = new char *[size_envp]; // user_env[id] = new char *[size_envp];
    char **ori_envp_variable = new char *[size_envp];
    int w = 0;
    for (char **env = envp; *env != 0; env++) {
        char *str1 = new char [300];
        strncpy (str1, *env, 300);
        ori_envp[w] = str1;
        // if (DEBUG) {
        //     cout << "(ori_envp) ori_envp[w]: " << ori_envp[w] << endl;
        //     cout << "(ori_envp) str1: " << str1 << endl;
        // }
        char *env_str_tmp = getenv(str1);
        if (env_str_tmp != NULL) {
            ori_envp_variable[w] = env_str_tmp;
            if (DEBUG) {
                cout << "(ori_envp) env_str_tmp: " << env_str_tmp<< endl;
                cout << "(ori_envp) ori_envp_variable[w]: " << ori_envp_variable[w] << endl;
            }
        } else {
            ori_envp_variable[w] = NULL;
        }
        w = w + 1;
    }

    server_socket = server_socket_TCP(port.c_str(), MAX_ACCEPT);
    // int number_of_fdset = getdtablesize(); 
    FD_ZERO(&activate_fdset);
    FD_SET(server_socket, &activate_fdset);
    while (true) {
        // if (DEBUG) {
        //     cout << "enter while head" << endl;
        // }
        memcpy(&read_fdset, &activate_fdset, sizeof(read_fdset));
        // if (DEBUG) {
        //     cout << "finish memcpy" << endl;
        // }
        if (select(FD_SETSIZE, &read_fdset, NULL, NULL, NULL) < 0) {
            continue;
        }
        if (DEBUG) {
            cout << "finish select" << endl;
        }
        if (FD_ISSET(server_socket, &read_fdset)) { // check if it is the server socket receiving message (means: someone wants to connect)
                                                    // skip processing user out of range: user_count < MAX_USER
            alen = sizeof(fsin);
            client_socket = accept(server_socket, (struct sockaddr *)&fsin, &alen);
            if (client_socket < 0) {
                cerr << "accept failed!" << endl;
                continue;
            }
            char client_ip[INET_ADDRSTRLEN];
            int client_port;
            inet_ntop(AF_INET, &(fsin.sin_addr), client_ip, INET_ADDRSTRLEN);
            FD_SET(client_socket, &activate_fdset);
            client_port = ntohs(fsin.sin_port);
            string tmp_address;
            tmp_address.append(client_ip);
            tmp_address.append(":");
            tmp_address.append(to_string(client_port));

            // if (DEBUG) {
            //     cout << "client_ip: " << client_ip << endl;
            //     cout << "client_port: " << client_port << endl;
            //     cout << "tmp_address: " << tmp_address << endl;
            // }
            
            for (int i = 0; i < MAX_ACCEPT; i++) {
                if (user_table_flag[i] == 0) {
                    user_table_flag[i] = 1;
                    user_table[i][1] = to_string(i + 1); //ID
                    user_table[i][2] = "(no name)";
                    user_table[i][3] = tmp_address;
                    user_table[i][4] = to_string(client_socket);
                    if (DEBUG) {
                        cout << "ID: " << user_table[i][1] << endl;
                        cout << "user_table[i][3] (address+port): " << user_table[i][3] << endl;
                        cout << "user_table[i][4] (client_socket): " << user_table[i][4] << endl;
                    }

                    // 要複製 ori_envp 的資料
                    user_env[i] = new char *[size_envp]; // user_env[id] = new char *[size_envp];
                    user_env_variable[i] = new char *[size_envp];
                    int z = 0;
                    for (char **env = ori_envp; *env != 0; env++) {
                        char *str2 = new char [300];
                        strncpy (str2, *env, 300);
                        user_env[i][z] = str2;
                        // if (DEBUG) {
                        //     cout << "(client_envp) user_env[i][z]: " << user_env[i][z] << endl;
                        //     cout << "(client_envp) str1: " << str2 << endl;
                        // }
                        char *env_str_tmp_1 = getenv(str2);
                        if (env_str_tmp_1 != NULL) {
                            user_env_variable[i][z] = env_str_tmp_1;
                            if (DEBUG) {
                                cout << "(client_envp) env_str_tmp: " << env_str_tmp_1 << endl;
                                cout << "(client_envp) user_env_variable[i][z]: " << user_env_variable[i][z] << endl;
                            }
                        } else {
                            user_env_variable[i][z] = NULL;
                        }
                        z = z + 1;
                    }
                    break;
                }
            }          
            
            dup2(client_socket, STDIN_FILENO);
            dup2(client_socket, STDOUT_FILENO);
            dup2(client_socket, STDERR_FILENO);
            welcome_message();
            broadcast(client_socket, "login");
            cout << "% " << flush;
        }
        for (int fd = 0; fd < FD_SETSIZE; fd++) {
            // if (DEBUG) {
            //     cout << "input signal comes from client!" << endl;
            // }  
            if (fd != server_socket && FD_ISSET(fd, &read_fdset)) {
                int user_idx;
                for (int i = 0; i < MAX_ACCEPT; i++) {
                    if (user_table_flag[i] == 1 && (user_table[i][4].compare(to_string(fd))) == 0) {
                        if (DEBUG) {
                            cout << "client address: " << user_table[i][3] << endl;
                        }
                        user_idx = i;
                        break;
                    }
                }
                
                //改成這個 client 的環境變數
                for (int y = 0; y < size_envp; y++) {
                    char *env_str_tmp_tmp = getenv(user_env[user_idx][y]);

                    if (user_env_variable[user_idx][y] != NULL) {
                        setenv(user_env[user_idx][y], user_env_variable[user_idx][y], 1);
                        // if (DEBUG) {
                        //     cout << "user_env[user_idx][y]: " << user_env[user_idx][y] << endl;
                        //     cout << "user_env_variable[user_idx][y]: " << user_env_variable[user_idx][y] << endl;
                        // }
                    } else if (env_str_tmp_tmp != NULL) {
                        unsetenv(user_env[user_idx][y]);
                    }
                }
                
                dup2(fd, STDIN_FILENO);
                dup2(fd, STDOUT_FILENO);
                dup2(fd, STDERR_FILENO);

                // if (DEBUG) {
                //     char *env_str_1 = getenv("PATH");
                //     if (env_str_1 != NULL) {
                //         cout << "switch to client, PATH is: " << env_str_1 << endl;
                //     }
                // } 
                if (echo(fd) == -1) { // exec np_shell and handle logout
                    broadcast(fd, "logout");
                    close(fd);
                    FD_CLR(fd, &activate_fdset);
                    
                    for (int k = 0; k < MAX_PIPE; k++) {
                        if ((num_pipe_table[k][3] == 1 && num_pipe_table[k][4] == fd) || 
                            (num_pipe_table[k][3] > 1 && num_pipe_table[k][4] == fd) || 
                            (num_pipe_table[k][3] == fd)) {
                            close(num_pipe_table[k][1]);
                            num_pipe_table[k][3] = 0;

                            while ((pid = fork()) < 0) {
                                waitpid(-1, &status, 0);
                            }
                            if (pid == 0) {
                                string tmp_string;
                                dup2(num_pipe_table[k][0], STDIN_FILENO);
                                dup2(devNull, STDOUT_FILENO);
                                while (getline(cin, tmp_string)){
                                    cout << tmp_string;
                                }
                                exit(0);
                            } else {
                                close(num_pipe_table[k][0]); 
                            }
                        }
                    }

                    for (int i = 0; i < MAX_ACCEPT; i++) {
                        if ((user_table[i][4].compare(to_string(fd))) == 0) {
                            user_table_flag[i] = 0;                           
                            break;
                        }
                    }
                }
                // if (DEBUG) {
                //     cout << "finish this turn of fd echo" << endl;
                // }   
            }
        }
        dup2(ori_stdin, STDIN_FILENO);
        dup2(ori_stdout, STDOUT_FILENO);
        dup2(ori_stderr, STDERR_FILENO);

        //改成 server 的環境變數
        for (int y = 0; y < size_envp; y++) {
            char *env_str_tmp_tmp_1 = getenv(ori_envp[y]);

            if (ori_envp_variable[y] != NULL) {
                setenv(ori_envp[y], ori_envp_variable[y], 1);
                // if (DEBUG) {
                //     cout << "ori_envp[y]: " << ori_envp[y] << endl;
                //     cout << "ori_envp_variable[y]: " << ori_envp_variable[y] << endl;
                // }
            } else if (env_str_tmp_tmp_1 != NULL) {
                unsetenv(ori_envp[y]);
            }
        }

        // if (DEBUG) {
        //     char *env_str = getenv("PATH");
        //     if (env_str != NULL) {
        //         cout << "back to server PATH is: " << env_str << endl;
        //     }
        // } 
        if (DEBUG) {
            cout << "(server) finish checking client select, back to while head" << endl;
        } 
    }
    return 0;
}