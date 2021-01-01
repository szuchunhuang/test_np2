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

#define MAX_PIPE    10000
#define	MAX_ACCEPT 30
#define DEBUG       0

using namespace std;

char **user_env[30];
char **user_env_variable[30];
int env_size_global = 0;
int devNull = open("/dev/null", O_WRONLY);
int num_empty_index = 0;
int userpipe_num_empty_index = 0;
string user_table[MAX_ACCEPT][5] = {"0"}; // [0]  | [1] |  [2] |   [3]        |      [4] 
                                          // flag |  ID | name | address:port | belong_socket
int user_table_flag[MAX_ACCEPT] = {0}; // replace user_table[][0];
                               
int num_pipe_table[MAX_PIPE][5] = {0};  //   [0]   |   [1]   |      [2]         |     [3]    |     [4]
                                        //   fd_R  |  fd_w   |   left_counter   |    flag    |  belong_socket
                                        //   int   |   int   |      int         |   0 or 1   |     int 

int user_pipe_table[MAX_PIPE][5] = {0}; //   [0]   |   [1]   |       [2]         |         [3]         |      [4]
                                        //   fd_R  |  fd_w   |   sender_socket   |    receiver_socket  |     flag
                                        //   int   |   int   |       int         |         int         |    0 or 1 


void pipe_closing (int fd_read, int fd_write) {
    for (int i = 0; i < MAX_PIPE; i++) { //小孩要關掉別 socket 的 numpipe
        if (num_pipe_table[i][3] == 1 && num_pipe_table[i][0] != fd_read && num_pipe_table[i][1] != fd_write) {
            close(num_pipe_table[i][0]);
            close(num_pipe_table[i][1]);
        }
    }

    for (int i = 0; i < MAX_PIPE; i++) { //小孩要關掉沒有用到的 user pipe
        if (user_pipe_table[i][4] == 1 && user_pipe_table[i][0] != fd_read && user_pipe_table[i][1] != fd_write) {
            close(user_pipe_table[i][0]);
            close(user_pipe_table[i][1]);
        }
    }
}

// parsing a % command line to separate instructions
std::vector<std::string> split(const std::string& line, char delim){
    std::vector<std::string> vec;
    std::vector<std::string> general_split;
    std::vector<std::string> built_in_func;
    std::vector<std::string> empty;
    std::stringstream s(line);
    std::string temp;
    std::string buff;

    if (line.size() == 0) {
        return empty;
    }
    while (std::getline(s, temp, delim)) {
        if (temp[0] == '|' || temp[0] == '!') {
            if (temp.size() >= 2) {
                buff.append(" ");
                buff.append(temp);
                vec.push_back(buff);
                buff = ""; // instruction |number pipe => all in one
            } else {
                vec.push_back(buff); // split command by '|'
                buff = "";
            }
        } else {
            if (!buff.empty()) {
                buff.append(" ");
                buff.append(temp);
            } else {
                buff.append(temp);
            }            
        }
    }   
    if (!buff.empty()){
        vec.push_back(buff);
    }

    return vec;
}

// execute the command ex. removetag test.html > test.txt
void my_exec(const std::string& cmd) {
    std::stringstream s(cmd);
    std::vector<std::string> argv;
    std::string temp;
    char delim = ' ';
    int argc = 0;
    bool save_file_flag = 0;
    std::string save_file_name;
    
    while (std::getline(s, temp, delim)) {
        if (temp[0] == '|' || temp[0] == '!') {
            break;
        }
        if (temp[0] == '<') {
            continue;
        }
        if (temp[0] == '>') {
            if (temp.length() == 2) {
                continue;
            } else if (temp.length() == 1) {
                save_file_flag = 1;
            }
        } else if (save_file_flag) {        
            // string sss = temp.substr(0, temp.length() - 1);
            // save_file_name = sss;
            save_file_name = temp;
            if (DEBUG) {
                cout << "file name length: " << save_file_name.size() << endl;
                cout << "file name content: " << save_file_name << "!" << endl;
            }
        } else {
            argc = argc + 1;
            argv.push_back(temp);
        }
    }
    
    // open output file
    if (save_file_flag) {
        int fd = open(save_file_name.c_str(), O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC, 0777);
        dup2(fd, STDOUT_FILENO);
        if (DEBUG) {
            std::cout << "expected file name: " << save_file_name << std::endl;
            std::cout << "file fd: " << fd << std::endl;
        }
    }

    // create exe_argv[]
    char* exe_argv[argc+1];
    for (int i = 0; i < argc; i++) {       
        char* tmp = new char[argv[i].size()+1]; // save argument data in heap
        std::strcpy(tmp,argv[i].c_str());
        exe_argv[i] = tmp;
    }
    exe_argv[argc] = NULL;
    if (DEBUG) {
        for (int i = 0; i < argc; i++) {
            cout << "exe_argv[i]: " << exe_argv[i] << endl;
        }
    }
    execvp(exe_argv[0], exe_argv);
    
    // string ss = argv[0].substr(0, argv[0].length() - 1);

    if (DEBUG) {
        cerr << "(DEBUG:cerr) Unknown command: [" << argv[0] << "]."<< endl;
        cout << "(DEBUG:cout) Unknown command: [" << argv[0] << "]."<< endl;
    }
    // if any error happened, end the child process
    cerr << "Unknown command: [" << argv[0] << "]."<< endl;
    exit(0);
    return;
}

void childHandler(int signal) {
    int status;
    while (waitpid(-1, &status, WNOHANG) > 0);
}

int np_shell(int current_socket, string line) { //np_shell() return 1 is logout!
    pid_t pid;
    int status;
    // int num_empty_index = 0;
    // int num_pipe_table[MAX_PIPE][5] = {0};  //   [0]   |   [1]   |      [2]         |     [3]    |     [4]
    //                                         //   fd_R  |  fd_w   |   left_counter   |    flag    |  belong_socket
    //                                         //   int   |   int   |      int         |   0 or 1   |     int
    
    // while (true) {
    signal(SIGCHLD, childHandler); // 任何 child process 結束即時回收，避免殭屍出現
    int last_R;
    int last_W;
    int temp_w_fd;
    int temp_r_fd;
    int temp_r_fd_single_end;
    int temp_w_fd_single_end;
    int usrpipe_r_fd;
    int expression;
    int temp_check;
    int temp_check_single_end;
    int transfer_result;
    bool error_hold_flag;
    int user_pipe_in = 0;
    int user_pipe_out = 0;
    int user_pipe_exist = 0;
    int user_pipe_exist_out = 0;
    int user_pipe_in_idx;
    int user_pipe_out_idx;
    string user_pipe_in_id;
    string user_pipe_out_id;
    size_t found_usrpipe_in_pos;
    size_t found_usrpipe_out_pos;
    size_t found_errpipe;
    size_t found_numpipe;
    std::vector<pid_t> pid_table_current_line;
    int receiver_socket;
    int sender_socket;
    // std::string line;
    // std::cout.flush();
    // std::cout << "% "; //如果輸入 enter 就會跳過 if 的指令大小檢查，重新跳回這一行等待新的輸入（hold住）
    // std::getline(std::cin, line);

    // string line_trim = line.substr(0, line.length() - 1); // trim off the last character '\0'
    // std::vector<std::string> command = split(line_trim, ' ');
    std::vector<std::string> command = split(line, ' ');

    // // process EOF
    // if (cin.eof()) {
    //     return 1;
    // }

    if (command.size() > 0) {
        int fds[2];
        transfer_result = 0;
        error_hold_flag = 0;

        // process num_pipe counter table
        for (int k = 0; k < MAX_PIPE; k++) {
            if (num_pipe_table[k][3] == 1 && num_pipe_table[k][4] == current_socket) {
                num_pipe_table[k][2] = num_pipe_table[k][2] - 1;
            }
        }
        
        // process builtin commands
        if (command[0].find("printenv") != string::npos) {
            stringstream s(command[0]);
            string tmp;
            char delim = ' ';
            getline(s, tmp, delim);
            getline(s, tmp, delim);
            char *env_str = getenv(tmp.c_str());
            if (env_str != NULL) {
                cout << env_str << endl;
            }
            return 0;
        } else if (command[0].find("setenv") != string::npos) {
            stringstream s(command[0]);
            string tmp1;
            string tmp2;
            char delim = ' ';
            getline(s, tmp1, delim);
            getline(s, tmp1, delim);
            getline(s, tmp2, delim);
            setenv(tmp1.c_str(), tmp2.c_str(), 1);
            
            
            int tmp_idx;
            for (int i = 0; i < MAX_ACCEPT; i++) {
                if (user_table_flag[i] == 1 && (user_table[i][4].compare(to_string(current_socket))) == 0) {
                    tmp_idx = i;
                    break;
                }
            }
            for (int y = 0; y < env_size_global; y++) {
                char *tmp_str = user_env[tmp_idx][y];
                char *copy_str = new char [300];
                if (strcmp(tmp_str, tmp1.c_str()) == 0) { 
                    // strncpy(copy_str, tmp2.c_str(), 300);
                    copy_str = getenv(tmp1.c_str());
                    if (copy_str != NULL) {
                        user_env_variable[tmp_idx][y] = copy_str;
                    } else {
                        user_env_variable[tmp_idx][y] = NULL;
                    }
                    if (DEBUG) {
                        cout << "copy_str: " << copy_str << endl;
                        cout << "user_env_variable[tmp_idx][y]: " << user_env_variable[tmp_idx][y] << endl;
                        cout << "tmp1: " << tmp1 << "tmp2: " << tmp2 << endl;
                        char *aa = getenv(tmp1.c_str());
                        if (aa != NULL) {
                            cout << "npshell setenv, table update by getenv(tmp1): " << aa << endl;
                        }
                    }
                }
                
            }
            return 0;
        } 
        // else if (command[0].find("exit") != string::npos) {
        //     return 1;
        // }

        // process each instruction in (vector) command
        for (int i = 0; i < command.size(); i++) {
            temp_check = 0;
            temp_check_single_end = 0;
            
            found_numpipe = command[i].find('|'); 
            found_errpipe = command[i].find('!');
            found_usrpipe_in_pos = command[i].find('<');
            found_usrpipe_out_pos = command[i].find('>');
            
            if (found_usrpipe_in_pos != string::npos) {
                user_pipe_in = 1;
                int a = command[i].find_first_of(' ',found_usrpipe_in_pos + 1); 
                if (a != string::npos) {
                    int b = a - (found_usrpipe_in_pos + 1);
                    user_pipe_in_id = command[i].substr (found_usrpipe_in_pos + 1, b);
                } else {
                    user_pipe_in_id = command[i].substr (found_usrpipe_in_pos + 1, string::npos);
                }
                // user_pipe_in_id = command[i][found_usrpipe_in_pos + 1]; // string
                user_pipe_in_idx = stoi(user_pipe_in_id);
            }
            if (found_usrpipe_out_pos != string::npos && command[i][found_usrpipe_out_pos + 1] != ' ') {
                user_pipe_out = 1;
                int c = command[i].find_first_of(' ', found_usrpipe_out_pos + 1); 
                if (c != string::npos) {
                    int d = c - (found_usrpipe_out_pos + 1);
                    user_pipe_out_id = command[i].substr (found_usrpipe_out_pos + 1, d);
                } else {
                    user_pipe_out_id = command[i].substr (found_usrpipe_out_pos + 1, string::npos);
                }
                // user_pipe_out_id = command[i][found_usrpipe_out_pos + 1]; // string
                user_pipe_out_idx = stoi(user_pipe_out_id);
            }

            if (found_numpipe != string::npos || found_errpipe != string::npos) { 
                // instruction ends with '|' or '!'（其他地方不會出現'|'，因為已經切開了）
                if (found_errpipe != string::npos) { // '!'
                    error_hold_flag = 1;
                    string ss = command[i].substr(found_errpipe + 1, command[i].length());
                    stringstream transfer(ss); 
                    transfer >> transfer_result;
                    if (i == 0) {
                        expression = -4;
                    } else {
                        expression = -3;
                    }
                } else { // '|'
                    string ss = command[i].substr(found_numpipe + 1, command[i].length());
                    stringstream transfer(ss); 
                    transfer >> transfer_result;
                    if (i == 0) {
                        expression = -4;
                    } else {
                        expression = -3;
                    }
                }
            } else {
                if (i == (command.size() - 1)) {
                    if (i == 0) {
                        expression = -2;
                    } else {
                        expression = -1;
                    }
                } else {
                    expression = i;
                }
            }

            if (DEBUG) {
                std::cout << "i: " << i << std::endl;
                std::cout << "expression: " << expression << std::endl;
                std::cout << "command line: " << command[i] << std::endl;
            }

            switch (expression) {
                case -4: // single command ends with '|' or '!'
                    if (user_pipe_in == 1) {
                        temp_check = 1;
                        user_pipe_exist = 0;
                        receiver_socket = current_socket;
                        // user_pipe_in_idx = stoi(user_pipe_in_id);
                        if ((user_pipe_in_idx <= 30) && user_table_flag[user_pipe_in_idx - 1] == 1) {
                            sender_socket = stoi(user_table[user_pipe_in_idx - 1][4]);
                            for (int k = 0; k < MAX_PIPE; k++) {
                                if (user_pipe_table[k][4] == 1 && user_pipe_table[k][2] == sender_socket && user_pipe_table[k][3] == receiver_socket) {
                                    close(user_pipe_table[k][1]);
                                    temp_r_fd = user_pipe_table[k][0];
                                    user_pipe_table[k][4] = 0;
                                    user_pipe_exist = 1;
                                    break;
                                }
                            }
                            if (user_pipe_exist == 0) {
                                temp_r_fd = devNull;
                            }
                        } else {
                            temp_r_fd = devNull;
                        }
                    } else {
                        for (int k = 0; k < MAX_PIPE; k++) {
                            if (num_pipe_table[k][3] == 1 && num_pipe_table[k][2] == 0 && num_pipe_table[k][4] == current_socket) {
                                close(num_pipe_table[k][1]); // 要讀出該 numpipe 先從 parent 關掉 write 端
                                temp_r_fd = num_pipe_table[k][0];
                                num_pipe_table[k][3] = 0;
                                temp_check = 1;
                                break;
                            } 
                        }
                    }
                    
                    for (int k = 0; k < MAX_PIPE; k++){
                        if (num_pipe_table[k][3] == 1 && (num_pipe_table[k][2] == transfer_result) && (num_pipe_table[k][4] == current_socket)) { // 已有出現此 numpipe，寫別人
                            temp_w_fd_single_end = num_pipe_table[k][1];
                            temp_r_fd_single_end = num_pipe_table[k][0]; 
                            temp_check_single_end = 1;
                            break;
                        }
                    }
                    
                    if (temp_check_single_end != 1) {
                        while (pipe(fds) < 0) {
                            usleep(100);
                        }
                        num_pipe_table[num_empty_index][0] = fds[0];
                        num_pipe_table[num_empty_index][1] = fds[1];
                        num_pipe_table[num_empty_index][2] = transfer_result; //counter
                        num_pipe_table[num_empty_index][3] = 1; // flag: in use
                        num_pipe_table[num_empty_index][4] = current_socket;
                        
                        // 要找下個沒有使用的空格idx
                        num_empty_index = num_empty_index + 1;
                        while (num_pipe_table[num_empty_index][3]) {
                            num_empty_index = num_empty_index + 1;
                            if (num_empty_index == MAX_PIPE) {
                                num_empty_index = 0;
                            }
                        }
                    }

                    while ((pid = fork()) < 0) {
                        waitpid(-1, &status, 0);
                    }
                    if (pid == 0) { // child process
                        int fd_read = -1;
                        int fd_write = -1;

                        if (temp_check == 1) {
                            dup2(temp_r_fd, STDIN_FILENO);
                            fd_read = temp_r_fd;
                        }
                        if (temp_check_single_end == 1) {
                            close(temp_r_fd_single_end);
                            dup2(temp_w_fd_single_end, STDOUT_FILENO);
                            if (error_hold_flag) {
                                dup2(temp_w_fd_single_end, STDERR_FILENO);
                            }
                            fd_write = temp_w_fd_single_end;
                        } else {
                            close(fds[0]);
                            dup2(fds[1], STDOUT_FILENO);
                            if (error_hold_flag) {
                                dup2(fds[1], STDERR_FILENO);
                            }
                            fd_write = fds[1];
                        }

                        pipe_closing (fd_read, fd_write);
                        my_exec(command[i]);
                        exit(0);

                    } else { // parent process
                        if (temp_check == 1) {
                            if (user_pipe_in == 1 && user_pipe_exist == 1) {
                                close(temp_r_fd);
                            } else if (user_pipe_in == 0) {
                                close(temp_r_fd);
                            }
                        }
                        if (DEBUG) {
                            if (temp_check == 1) {
                                std::cout << "case -4, temp_r_fd " << temp_r_fd << std::endl;
                            }
                            if (temp_check_single_end == 1) {
                                std::cout << "case -4, temp_r_fd_single_end: " << temp_r_fd_single_end << std::endl;
                                std::cout << "case -4, temp_w_fd_single_end: " << temp_w_fd_single_end << std::endl;
                            }
                            std::cout << "case -4, pid: " << pid << std::endl;
                        }
                    }
                    break;

                case -3: // last command ends with '|' or '!'
                    close(last_W);
                    for (int k = 0; k < MAX_PIPE; k++){
                        if (num_pipe_table[k][3] == 1 && (num_pipe_table[k][2] == transfer_result) && (num_pipe_table[k][4] == current_socket)) { // 已有出現，寫別人
                            temp_w_fd = num_pipe_table[k][1];
                            temp_r_fd = num_pipe_table[k][0]; 
                            temp_check = 1;
                        }
                    }
                    if (temp_check != 1) {
                        while (pipe(fds) < 0) {
                            usleep(100);
                        }
                        num_pipe_table[num_empty_index][0] = fds[0];
                        num_pipe_table[num_empty_index][1] = fds[1];
                        num_pipe_table[num_empty_index][2] = transfer_result; //counter
                        num_pipe_table[num_empty_index][3] = 1; // flag: in use
                        num_pipe_table[num_empty_index][4] = current_socket;
                        
                        // 要找下個沒有使用的空格idx
                        num_empty_index = num_empty_index + 1;
                        while (num_pipe_table[num_empty_index][3]) {
                            num_empty_index = num_empty_index + 1;
                            if (num_empty_index == MAX_PIPE) {
                                num_empty_index = 0;
                            }
                        }
                    }
                    while ((pid = fork()) < 0) {
                        waitpid(-1, &status, 0);
                    }
                    if (pid == 0) { // child process
                        int fd_read = last_R;
                        int fd_write = -1;

                        dup2(last_R, STDIN_FILENO);
                        if (temp_check == 1) {
                            close(temp_r_fd);
                            dup2(temp_w_fd, STDOUT_FILENO);
                            if (error_hold_flag) {
                                dup2(temp_w_fd, STDERR_FILENO);
                            }
                            fd_write = temp_w_fd;
                        } else {
                            close(fds[0]);
                            dup2(fds[1], STDOUT_FILENO);
                            if (error_hold_flag) {
                                dup2(fds[1], STDERR_FILENO);
                            }
                            fd_write = fds[1];
                        }

                        pipe_closing (fd_read, fd_write);

                        my_exec(command[i]);
                        exit(0);
                    } else { // parent process
                        close(last_R);
                        pid_table_current_line.push_back(pid);
                        if (DEBUG) {
                            std::cout << "case -3, last_W in last command: " << last_W << std::endl;
                            std::cout << "case -3, last_R in last command: " << last_R << std::endl;
                            std::cout << "case -3, pid: " << pid << std::endl;
                        }
                    }
                    break;

                case -2: // single command without numpipe end 
                    if (user_pipe_in == 1) {
                        temp_check = 1;
                        user_pipe_exist = 0;
                        receiver_socket = current_socket;
                        // user_pipe_in_idx = stoi(user_pipe_in_id);
                        if ((user_pipe_in_idx <= 30) && user_table_flag[user_pipe_in_idx - 1] == 1) {
                            sender_socket = stoi(user_table[user_pipe_in_idx - 1][4]);
                            for (int k = 0; k < MAX_PIPE; k++) {
                                if (user_pipe_table[k][4] == 1 && user_pipe_table[k][2] == sender_socket && user_pipe_table[k][3] == receiver_socket) {
                                    close(user_pipe_table[k][1]);
                                    temp_r_fd = user_pipe_table[k][0];
                                    user_pipe_table[k][4] = 0;
                                    user_pipe_exist = 1;
                                    break;
                                }
                            }
                            if (user_pipe_exist == 0) {
                                temp_r_fd = devNull;
                            }
                        } else {
                            temp_r_fd = devNull;
                        }
                    } else {
                        for (int k = 0; k < MAX_PIPE; k++) {
                            if (num_pipe_table[k][3] == 1 && (num_pipe_table[k][2] == 0) && (num_pipe_table[k][4] == current_socket)) {
                                close(num_pipe_table[k][1]); // 要讀出該 numpipe 先從 parent 關掉 write 端
                                temp_r_fd = num_pipe_table[k][0];
                                num_pipe_table[k][3] = 0;
                                temp_check = 1;
                                break;
                            } 
                        }
                    }

                    if (user_pipe_out == 1) {
                        sender_socket = current_socket;
                        user_pipe_exist_out = 0;
                        // user_pipe_out_idx
                        if ( user_pipe_out_idx <= 30 && user_table_flag[user_pipe_out_idx - 1] == 1) {
                            receiver_socket = stoi(user_table[user_pipe_out_idx - 1][4]);
                            for (int k = 0; k < MAX_PIPE; k++) {
                                if (user_pipe_table[k][4] == 1 && user_pipe_table[k][2] == sender_socket && user_pipe_table[k][3] == receiver_socket) {
                                    usrpipe_r_fd = user_pipe_table[k][0];
                                    temp_w_fd = user_pipe_table[k][1];
                                    user_pipe_exist_out = 1;
                                    break;
                                }
                            }
                            if (user_pipe_exist_out == 0) {
                                temp_w_fd = devNull;
                            }
                        } else {
                            temp_w_fd = devNull;
                        }
                    }

                    while ((pid = fork()) < 0) {
                        waitpid(-1, &status, 0);
                    }
                    if (pid == 0) { // child process
                        int fd_read = -1;
                        int fd_write = -1;

                        if (temp_check == 1) {
                            dup2(temp_r_fd, STDIN_FILENO);
                            fd_read = temp_r_fd;
                        }
                        if (user_pipe_out == 1) {
                            close(usrpipe_r_fd);
                            dup2(temp_w_fd, STDOUT_FILENO);
                            fd_write = temp_w_fd;
                        }

                        if (DEBUG) {
                            cout << "test forking out process output!" << endl;
                            cout << "STDIN_FILENO" << STDIN_FILENO << endl;
                            cout << "STDOUT_FILENO" << STDOUT_FILENO << endl;
                        }

                        pipe_closing (fd_read, fd_write);

                        my_exec(command[i]);
                        exit(0);              
                    } else { // parent process
                        if (temp_check == 1) {
                            if (user_pipe_in == 1 && user_pipe_exist == 1) {
                                close(temp_r_fd);
                            } else if (user_pipe_in == 0) {
                                close(temp_r_fd);
                            }
                        }
                        pid_table_current_line.push_back(pid);
                        
                        if (DEBUG) {
                            if(temp_check == 1) {
                                std::cout << "case -2, temp_r_fd: " << temp_r_fd << std::endl;
                                std::cout << "case -2, pid: " << pid << std::endl;   
                            } else {
                                std::cout << "case -2, STDIN & STDOUT" << std::endl;
                                std::cout << "case -2, pid: " << pid << std::endl;   
                            }
                            
                        }
                        int pid_table_size = pid_table_current_line.size();
                        for (int i = 0; i < pid_table_size; i++) {
                            int status_current_line;
                            waitpid(pid_table_current_line[i], &status_current_line, 0); // 等一個收完才收下一個，確定每個小孩都收到才進到下一行 %
                        }
                    }
                    break;

                case 0: // initial command
                    if (user_pipe_in == 1) {
                        temp_check = 1;
                        user_pipe_exist = 0;
                        receiver_socket = current_socket;
                        // user_pipe_in_idx = stoi(user_pipe_in_id);
                        if ((user_pipe_in_idx <= 30) && user_table_flag[user_pipe_in_idx - 1] == 1) {
                            sender_socket = stoi(user_table[user_pipe_in_idx - 1][4]);
                            for (int k = 0; k < MAX_PIPE; k++) {
                                if (user_pipe_table[k][4] == 1 && user_pipe_table[k][2] == sender_socket && user_pipe_table[k][3] == receiver_socket) {
                                    close(user_pipe_table[k][1]);
                                    temp_r_fd = user_pipe_table[k][0];
                                    user_pipe_table[k][4] = 0;
                                    user_pipe_exist = 1;
                                    break;
                                }
                            }
                            if (user_pipe_exist == 0) {
                                temp_r_fd = devNull;
                            }
                        } else {
                            temp_r_fd = devNull;
                        }
                    } else {
                        for (int k = 0; k < MAX_PIPE; k++) {
                            if (num_pipe_table[k][3] == 1 && (num_pipe_table[k][2] == 0) && (num_pipe_table[k][4] == current_socket)) {
                                close(num_pipe_table[k][1]); // 要讀出該 numpipe 先從 parent 關掉 write 端
                                temp_r_fd = num_pipe_table[k][0];
                                num_pipe_table[k][3] = 0;
                                temp_check = 1;
                                break;
                            } 
                        }
                    }

                    while (pipe(fds) < 0) {
                        usleep(100);
                    }
                    while ((pid = fork()) < 0) {
                        waitpid(-1, &status, 0);
                    }
                    if (pid == 0) { // child process
                        int fd_read = -1;
                        int fd_write = -1;

                        if (temp_check == 1) {
                            dup2(temp_r_fd, STDIN_FILENO);
                            fd_read = temp_r_fd;
                        }  
                        close(fds[0]);
                        dup2(fds[1], STDOUT_FILENO);
                        fd_write = fds[1];

                        pipe_closing (fd_read, fd_write);

                        my_exec(command[i]);
                        exit(0); 
                    } else { // parent process
                        if (temp_check == 1) {
                            if (user_pipe_in == 1 && user_pipe_exist == 1) {
                                close(temp_r_fd);
                            } else if (user_pipe_in == 0) {
                                close(temp_r_fd);
                            }
                        }
                        last_R = fds[0];
                        last_W = fds[1];
                        pid_table_current_line.push_back(pid);
                        if (DEBUG) {
                            std::cout << "case 0, fds[0]: " << fds[0] << " ,fds[1]: " << fds[1] << std::endl;
                            std::cout << "case 0, last_R in initial command: " << last_R << std::endl;
                            std::cout << "case 0, pid: " << pid << std::endl;
                        }
                    }
                    break;

                case -1: // last command without number or error pipe
                    close(last_W);
                    if (user_pipe_out == 1) {
                        sender_socket = current_socket;
                        user_pipe_exist_out = 0;
                        // user_pipe_out_idx
                        if ( user_pipe_out_idx <= 30 && user_table_flag[user_pipe_out_idx - 1] == 1) {
                            receiver_socket = stoi(user_table[user_pipe_out_idx - 1][4]);
                            for (int k = 0; k < MAX_PIPE; k++) {
                                if (user_pipe_table[k][4] == 1 && user_pipe_table[k][2] == sender_socket && user_pipe_table[k][3] == receiver_socket) {
                                    usrpipe_r_fd = user_pipe_table[k][0];
                                    temp_w_fd = user_pipe_table[k][1];
                                    user_pipe_exist_out = 1;
                                    break;
                                }
                            }
                            if (user_pipe_exist_out == 0) {
                                temp_w_fd = devNull;
                            }
                        } else {
                            temp_w_fd = devNull;
                        }
                    }
                    
                    while ((pid = fork()) < 0) {
                        waitpid(-1, &status, 0);
                    }
                    if (pid == 0) { // child process
                        int fd_read = last_R;
                        int fd_write = -1;

                        dup2(last_R, STDIN_FILENO);
                        if (user_pipe_out == 1) {
                            close(usrpipe_r_fd);
                            dup2(temp_w_fd, STDOUT_FILENO);
                            fd_write = temp_w_fd;
                        }

                        pipe_closing (fd_read, fd_write);

                        my_exec(command[i]);
                        exit(0);           
                    } else { // parent process
                        close(last_R);
                        pid_table_current_line.push_back(pid);
                        
                        if (DEBUG) {
                            std::cout << "case -1, last_R in last command: " << last_R << std::endl;
                            std::cout << "case -1, pid: " << pid << std::endl;
                        }
                        int pid_table_size = pid_table_current_line.size();
                        for (int i = 0; i < pid_table_size; i++) {
                            int status_current_line;
                            waitpid(pid_table_current_line[i], &status_current_line, 0); // 等一個收完才收下一個，確定每個小孩都收到才進到下一行 %
                        }
                    }
                    break;

                default: // middle command
                    while (pipe(fds) < 0) {
                        usleep(100);
                    }
                    while ((pid = fork()) < 0) {
                        waitpid(-1, &status, 0);
                    }
                    if (pid == 0) { // child process
                        int fd_read = -1;
                        int fd_write = -1;

                        close(last_W);
                        dup2(last_R, STDIN_FILENO);
                        close(fds[0]);
                        dup2(fds[1], STDOUT_FILENO);

                        pipe_closing (fd_read, fd_write);

                        my_exec(command[i]);
                        exit(0);              
                    } else { // parent process
                        close(last_W);
                        close(last_R);
                        last_W = fds[1];
                        last_R = fds[0];
                        pid_table_current_line.push_back(pid);
                        if (DEBUG) {
                            std::cout << "case default, fds[0]: " << fds[0] << " ,fds[1]" << fds[1] << std::endl;
                            std::cout << "case default, last_R in middle command: " << last_R << std::endl;
                            std::cout << "case default, pid: " << pid << std::endl;
                        }
                    }
                    break;  
            }
        }
    }
    if (DEBUG) {
        cout << "in stack function, just finish npshell" << endl;
    }
    return 0;
}
