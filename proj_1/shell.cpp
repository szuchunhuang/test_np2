#include "shell.h"

#define MAX_PIPE 3000
#define DEBUG 0

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
        general_split.push_back(temp);
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

    //process builtin commands
    if (general_split[0] == "printenv") {
        my_printenv(general_split[1]);
        built_in_func.push_back("builtin");
        return built_in_func;

    } else if (general_split[0] == "setenv") {
        my_setenv(general_split[1],general_split[2]);
        built_in_func.push_back("builtin");
        return built_in_func;

    } else if (general_split[0] == "exit") {
        my_exit();
        built_in_func.push_back("builtin");
        return built_in_func;
    } else if (general_split[0] == "unsetenv") {
        my_unsetenv(general_split[1]);
        built_in_func.push_back("builtin");
        return built_in_func;
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
        if (temp[0] == '>') {
            save_file_flag = 1;
        } else if (save_file_flag) {
            save_file_name = temp;
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
    execvp(exe_argv[0], exe_argv);

    // if any error happened, end the child process
    fprintf(stderr, "Unknown command: [%s].\n", exe_argv[0]);
    exit(0);
    return;
}

void childHandler(int signal) {
    int status;
    while (waitpid(-1, &status, WNOHANG) > 0);
}


int main(int argc, char *argv[]) { 
    std::string buf = "PATH=bin:.";
    char* env = new char[buf.size()];
    std::strcpy(env,buf.c_str());
    putenv(env);

    pid_t pid;
    int status;
    int num_empty_index = 0;
    int num_pipe_table[MAX_PIPE][4] = {0};  //   [0]   |   [1]   |      [2]         |  [3]     
                                            //   fd_R  |  fd_w   |   left_counter   |  flag
                                            //   int   |   int   |      int         |  0 or 1
    
    while (true) {
        signal(SIGCHLD, childHandler); // 任何 child process 結束即時回收，避免殭屍出現
        int last_R;
        int last_W;
        int temp_w_fd;
        int temp_r_fd;
        int temp_r_fd_single_end;
        int temp_w_fd_single_end;
        int expression;
        int temp_check;
        int temp_check_single_end;
        int transfer_result;
        bool error_hold_flag;
        size_t found_errpipe;
        size_t found_numpipe;
        std::vector<pid_t> pid_table_current_line;
        std::string line;
        std::cout.flush();
        std::cout << "% "; //如果輸入 enter 就會跳過 if 的指令大小檢查，重新跳回這一行等待新的輸入（hold住）
        std::getline(std::cin, line);
        std::vector<std::string> command = split(line, ' ');

        // process EOF
        if (cin.eof()) {
            exit(1);
        }

        if (command.size() > 0) {
            int fds[2];
            transfer_result = 0;
            error_hold_flag = 0;

            // process num_pipe counter table
            for (int k = 0; k < MAX_PIPE; k++) {
                if (num_pipe_table[k][3]) {
                    num_pipe_table[k][2] = num_pipe_table[k][2] - 1;
                }
            }
            if (command[0] == "builtin") continue; // count as one instruction

            // process each instruction in (vector)command
            for (int i = 0; i < command.size(); i++) {
                temp_check = 0;
                temp_check_single_end = 0;
                
                found_numpipe = command[i].find('|'); 
                found_errpipe = command[i].find('!');

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
                        for (int k = 0; k < MAX_PIPE; k++) {
                            if (num_pipe_table[k][3] && num_pipe_table[k][2] == 0) {
                                close(num_pipe_table[k][1]); // 要讀出該 numpipe 先從 parent 關掉 write 端
                                temp_r_fd = num_pipe_table[k][0];
                                num_pipe_table[k][3] = 0;
                                temp_check = 1;
                                break;
                            } 
                        }
                        
                        for (int k = 0; k < MAX_PIPE; k++){
                            if (num_pipe_table[k][3] && (num_pipe_table[k][2] == transfer_result)) { // 已有出現，寫別人
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
                            
                            // 要找下個沒有使用的空格idx
                            num_empty_index = num_empty_index + 1;
                            while (num_pipe_table[num_empty_index][3] == 1) {
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
                            if (temp_check == 1) {
                                dup2(temp_r_fd, STDIN_FILENO);
                            }
                            if (temp_check_single_end == 1) {
                                close(temp_r_fd_single_end);
                                dup2(temp_w_fd_single_end, STDOUT_FILENO);
                                if (error_hold_flag) {
                                    dup2(temp_w_fd_single_end, STDERR_FILENO);
                                }
                            } else {
                                close(fds[0]);
                                dup2(fds[1], STDOUT_FILENO);
                                if (error_hold_flag) {
                                    dup2(fds[1], STDERR_FILENO);
                                }
                            }
                            my_exec(command[i]);
                            exit(0);

                        } else { // parent process
                            if (temp_check == 1) {
                                close(temp_r_fd);
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
                            if (num_pipe_table[k][3] && (num_pipe_table[k][2] == transfer_result)) { // 已有出現，寫別人
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
                            
                            // 要找下個沒有使用的空格idx
                            num_empty_index = num_empty_index + 1;
                            while (num_pipe_table[num_empty_index][3] == 1) {
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
                            dup2(last_R, STDIN_FILENO);
                            if (temp_check == 1) {
                                close(temp_r_fd);
                                dup2(temp_w_fd, STDOUT_FILENO);
                                if (error_hold_flag) {
                                    dup2(temp_w_fd, STDERR_FILENO);
                                }
                            } else {
                                close(fds[0]);
                                dup2(fds[1], STDOUT_FILENO);
                                if (error_hold_flag) {
                                    dup2(fds[1], STDERR_FILENO);
                                }
                            }
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
                        for (int k = 0; k < MAX_PIPE; k++) {
                            if (num_pipe_table[k][3] && (num_pipe_table[k][2] == 0)) {
                                close(num_pipe_table[k][1]); // 要讀出該 numpipe 先從 parent 關掉 write 端
                                temp_r_fd = num_pipe_table[k][0];
                                num_pipe_table[k][3] = 0;
                                temp_check = 1;
                                break;
                            } 
                        }
                        while ((pid = fork()) < 0) {
                            waitpid(-1, &status, 0);
                        }
                        if (pid == 0) { // child process
                            if (temp_check == 1) {
                                dup2(temp_r_fd, STDIN_FILENO);
                            }
                            my_exec(command[i]);
                            exit(0);              
                        } else { // parent process
                            if (temp_check == 1) {
                                close(temp_r_fd);
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
                        for (int k = 0; k < MAX_PIPE; k++) {
                            if (num_pipe_table[k][3] && (num_pipe_table[k][2] == 0)) {
                                close(num_pipe_table[k][1]); // 要讀出該 numpipe 先從 parent 關掉 write 端
                                temp_r_fd = num_pipe_table[k][0];
                                num_pipe_table[k][3] = 0;
                                temp_check = 1;
                                break;
                            } 
                        }

                        while (pipe(fds) < 0) {
                            usleep(100);
                        }
                        while ((pid = fork()) < 0) {
                            waitpid(-1, &status, 0);
                        }
                        if (pid == 0) { // child process
                            if (temp_check == 1) {
                                dup2(temp_r_fd, STDIN_FILENO);
                            }  
                            close(fds[0]);
                            dup2(fds[1], STDOUT_FILENO);
                            my_exec(command[i]);
                            exit(0); 
                        } else { // parent process
                            if (temp_check == 1) {
                                close(temp_r_fd);
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
                        while ((pid = fork()) < 0) {
                            waitpid(-1, &status, 0);
                        }
                        if (pid == 0) { // child process
                            dup2(last_R, STDIN_FILENO);
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
                            close(last_W);
                            dup2(last_R, STDIN_FILENO);
                            close(fds[0]);
                            dup2(fds[1], STDOUT_FILENO);
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
    }
}