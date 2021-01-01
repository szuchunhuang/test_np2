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
#include <unordered_set>
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
int block_user[30][30] = {0};  //index 填 idx; 裡面存 ID
// char **user_env[30];
// char **user_env_variable[30];
// int num_empty_index = 0;
string user_pipe_command[MAX_ACCEPT] = {"0"};
// string user_table[MAX_ACCEPT][5] = {0}; // [0]  | [1] |  [2] |   [3]        |      [4] 
//                                         // flag |  ID | name | address:port | belong_socket
                               
// int num_pipe_table[MAX_PIPE][5] = {0};  //   [0]   |   [1]   |      [2]         |     [3]                |     [4]
//                                         //   fd_R  |  fd_w   |   left_counter   |    flag                |  belong_socket
//                                         //   int   |   int   |      int         |   0 or 1 (or sender)   |  int (or receiver)

// int user_pipe_table[MAX_PIPE][5] = {0}; //   [0]   |   [1]   |       [2]         |         [3]         |      [4]
//                                         //   fd_R  |  fd_w   |   sender_socket   |    receiver_socket  |     flag
//                                         //   int   |   int   |       int         |         int         |    0 or 1

void process_logout_pipe(int fd_read){
    pid_t pid;
    int status;

    while ((pid = fork()) < 0) {
        waitpid(-1, &status, 0);
    }
    if (pid == 0) {
        string tmp_string;
        dup2(fd_read, STDIN_FILENO);
        dup2(devNull, STDOUT_FILENO);
        while (getline(cin, tmp_string)){
            cout << tmp_string;
        }
        exit(0);
    } else {
        close(fd_read); 
    }
}
void welcome_message() {
    cout << "****************************************" << endl;
    cout << "** Welcome to the information server. **" << endl;
    cout << "****************************************" << endl;
}

void broadcast (int client_socket, string line) { 
    int tmp_fd;
    int tmp_id;
    int client_idx;
    int target_idx;
    int expression = 0;
    int flag;
    int flag_1;
    int block_flag;
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
    string output_string;
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
    } else if (temp.find("block") != string::npos) {
        expression = 7;
    }
    
    switch (expression) {
        case 1: //login
            for (int k = 0; k < MAX_ACCEPT; k++) {
                if (user_table_flag[k] == 1) {
                    tmp_fd = stoi(user_table[k][4]);
                    output_string = "*** User '" + username + "' entered from " + ip_port + ". ***";
                    write(tmp_fd, &output_string, output_string.size());
                }
            }
            break;

        case 2: //logout
            for (int k = 0; k < MAX_ACCEPT; k++) {
                if (user_table_flag[k] == 1 && user_table[k][2].compare(username) != 0) {
                    tmp_fd = stoi(user_table[k][4]);
                    output_string = "*** User '" + username + "' left. ***";
                    write(tmp_fd, &output_string, output_string.size());
                }            
            }
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
                        output_string = "*** User from " + ip_port + " is named '" + username + "'. ***";
                        write(tmp_fd, &output_string, output_string.size());
                    }
                }
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
            block_flag = 0;

            tmp_id = stoi (rec_id);
            if (user_table_flag[tmp_id - 1] == 1) {
                for (int i = 0; i < 30; i++) {
                    if (block_user[client_idx][i] == tmp_id) {
                        block_flag = 1;
                        break;
                    } 
                }
                if (block_flag == 0) {
                    tmp_fd = stoi(user_table[tmp_id - 1][4]);
                    output_string = "*** " + username + " told you ***: " + msg;
                    write(tmp_fd, &output_string, output_string.size());
                }

            } else {
                cout << "*** Error: user #" << rec_id << " does not exist yet. ***" << endl;
            }
            break;

        case 6: //yell 
            msg = s.str();
            tmp_found = msg.find_first_of(' ');
            msg.erase(0, tmp_found + 1);
            block_flag = 0;
            
            for (int k = 0; k < MAX_ACCEPT; k++) {
                block_flag = 0;
                if (user_table_flag[k] == 1) {
                    tmp_fd = stoi(user_table[k][4]);
                    target_idx = stoi(user_table[k][1]);
                    for (int i = 0; i < 30; i++) {
                        if (block_user[client_idx][i] == target_idx) {
                            block_flag = 1;
                            break;
                        } 
                    }

                    if (block_flag == 0) {
                        output_string =  "*** " + username + " yelled ***: " + msg;
                        write(tmp_fd, &output_string, output_string.size());
                    }
                }      
            }
            break;

        case 7: //block 
            // int block_user[30][30] = {0};     
            getline(s, temp, delim);
            target_idx = stoi (temp);
            
            for (int i = 0; i < 30; i++) {
                if (block_user[client_idx][i] == 0) {
                    block_user[client_idx][i] = target_idx;
                    break;
                }
            }

            for (int i = 0; i < 30; i++) {
                if (block_user[target_idx - 1][i] == 0) {
                    block_user[target_idx - 1][i] = client_idx + 1;
                    break;
                }
            }
            break;

        default: //userpipe
            // int user_pipe_table[MAX_PIPE][5] = {0}; //   [0]   |   [1]   |       [2]         |         [3]         |      [4]
            //                                         //   fd_R  |  fd_w   |   sender_socket   |    receiver_socket  |     flag
            //                                         //   int   |   int   |       int         |         int         |    0 or 1
            
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
                        if (user_pipe_table[k][4] == 1 && user_pipe_table[k][2] == stoi (send_socket) && user_pipe_table[k][3] == client_socket) {
                            flag_1 = 1;
                            break;
                        }
                    }
                    if (flag_1 == 1) { //successfully receive message
                        new_line = user_pipe_command[stoi(send_id) - 1];
                        for (int k = 0; k < MAX_ACCEPT; k++) {
                            if (user_table_flag[k] == 1) {
                                tmp_fd = stoi(user_table[k][4]);
                                output_string = "*** " + username + " (#" + client_id + ") just received from " + send_name + " (#" + send_id + ") by '" + new_line + "' ***";
                                write(tmp_fd, &output_string, output_string.size());
                            }
                        }

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
                        if (user_pipe_table[k][4] == 1 && user_pipe_table[k][2] == client_socket && user_pipe_table[k][3] == stoi (belong_socket)) {
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
                        user_pipe_table[userpipe_num_empty_index][0] = fds[0];
                        user_pipe_table[userpipe_num_empty_index][1] = fds[1];
                        user_pipe_table[userpipe_num_empty_index][2] = client_socket; // sender
                        user_pipe_table[userpipe_num_empty_index][3] = stoi (belong_socket); //receiver
                        user_pipe_table[userpipe_num_empty_index][4] = 1; // counter not used
                        
                        // 要找下個沒有使用的空格idx
                        userpipe_num_empty_index = userpipe_num_empty_index + 1;
                        while (user_pipe_table[userpipe_num_empty_index][4]) {
                            userpipe_num_empty_index = userpipe_num_empty_index + 1;
                            if (userpipe_num_empty_index == MAX_PIPE) {
                                userpipe_num_empty_index = 0;
                            }
                        }
                        
                        for (int k = 0; k < MAX_ACCEPT; k++) {
                            if (user_table_flag[k] == 1) {
                                tmp_fd = stoi(user_table[k][4]);
                                output_string = "*** " + username + " (#" + client_id + ") just piped '" + line + "' to " + rec_name + " (#" + rec_id + ") ***";
                                write(tmp_fd, &output_string, output_string.size());
                            }
                        }
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

    if (command.find("who") != string::npos || command.find("tell") != string::npos || command.find("yell") != string::npos || command.find("name") != string::npos || command.find("block") != string::npos ) {
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

void update_env(char **des_env_k, char **des_env_v,char **src_env_k){
    unordered_set<string> env_set;
    int i = 0;
    for(char **env = des_env_k; *env != 0; env++){
        env_set.insert(*env);
        setenv(*env, des_env_v[i], 1);
        i++;
    }
    i = 0;
    for(char **env = src_env_k; *env != 0; env++){
        if (env_set.find(*env) == env_set.end()){
            unsetenv(*env);
        }
    }
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
        char *env_str_tmp = getenv(str1);
        ori_envp_variable[w] = env_str_tmp;
        w = w + 1;
    }

    server_socket = server_socket_TCP(port.c_str(), MAX_ACCEPT);
    // int number_of_fdset = getdtablesize(); 
    FD_ZERO(&activate_fdset);
    FD_SET(server_socket, &activate_fdset);
    while (true) {
        memcpy(&read_fdset, &activate_fdset, sizeof(read_fdset));
        if (select(FD_SETSIZE, &read_fdset, NULL, NULL, NULL) < 0) {
            continue;
        }
        if (DEBUG) {
            cout << "finish select" << endl;
        }
        // Handle connect
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
                        char *str3 = new char [300];
                        strncpy (str2, *env, 300);
                        strncpy (str3, *ori_envp_variable, 300);
                        user_env[i][z] = str2;
                        user_env_variable[i][z] = str3;
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
        // Handle read
        for (int fd = 0; fd < FD_SETSIZE; fd++) {
            // if (DEBUG) {
            //     cout << "input signal comes from client!" << endl;
            // }  
            int user_idx;
            if (fd != server_socket && FD_ISSET(fd, &read_fdset)) {
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
                // update_env(user_env[user_idx], user_env_variable[user_idx], ori_envp);
                for (int y = 0; y < size_envp; y++) {
                    // setenv(user_env[user_idx][y], user_env_variable[user_idx][y], 1);

                    char *env_str_tmp_tmp = getenv(user_env[user_idx][y]);
                    if (user_env_variable[user_idx][y] != NULL) {
                        setenv(user_env[user_idx][y], user_env_variable[user_idx][y], 1);
                    } else if (env_str_tmp_tmp != NULL) {
                        unsetenv(user_env[user_idx][y]);
                    }
                }
                
                dup2(fd, STDIN_FILENO);
                dup2(fd, STDOUT_FILENO);
                dup2(fd, STDERR_FILENO);
 
                if (echo(fd) == -1) { // exec np_shell and handle logout
                    broadcast(fd, "logout");
                    close(fd);
                    FD_CLR(fd, &activate_fdset);
                    
                    for (int k = 0; k < MAX_PIPE; k++) { //user pipe 多餘的資料尚未輸入完畢
                        if (user_pipe_table[k][4] == 1) {
                            if (user_pipe_table[k][2] == fd || user_pipe_table[k][3] == fd) { 
                                close(user_pipe_table[k][1]);
                                process_logout_pipe(user_pipe_table[k][0]);
                                user_pipe_table[k][4] = 0;
                            }
                        }
                    
                    }

                    for (int k = 0; k < MAX_PIPE; k++) { //num pipe 多餘的資料尚未輸入完畢
                        if (num_pipe_table[k][3] == 1 && num_pipe_table[k][4] == fd) {
                            close(num_pipe_table[k][1]);
                            process_logout_pipe(num_pipe_table[k][0]);
                            num_pipe_table[k][3] = 0;
                        }
                    }

                    for (int i = 0; i < MAX_ACCEPT; i++) {
                        if ((user_table[i][4].compare(to_string(fd))) == 0) {
                            user_table_flag[i] = 0;
                            int delete_idx = i;
                            for (int j = 0; j < 30 ; j++) {
                                block_user[delete_idx][j] = 0;
                            }                            
                            break;
                        }
                    }
                } 
                //改成 server 的環境變數
                // update_env(ori_envp, ori_envp_variable, user_env[user_idx]);
            }
        }
        dup2(ori_stdin, STDIN_FILENO);
        dup2(ori_stdout, STDOUT_FILENO);
        dup2(ori_stderr, STDERR_FILENO);

        // //改成 server 的環境變數
        for (int y = 0; y < size_envp; y++) {
            // setenv(ori_envp[y], ori_envp_variable[y], 1);
            
            char *env_str_tmp_tmp_1 = getenv(ori_envp[y]);
            if (ori_envp_variable[y] != NULL) {
                setenv(ori_envp[y], ori_envp_variable[y], 1);
            } else if (env_str_tmp_tmp_1 != NULL) {
                unsetenv(ori_envp[y]);
            }
        }
        
    }
    return 0;
}