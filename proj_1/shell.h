#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <vector>
#include <cstring>
#include<signal.h>
using namespace std;

int my_printenv(const std::string& variable){
    char* env = new char[variable.size()];
    std::strcpy(env,variable.c_str());
    char const* temp = getenv(env);
    if (temp) {
        std::cout << std::string(temp) << std::endl;
    }
    return 0;
}

int my_setenv(const std::string& variable_name, const std::string& assign_value){
    std::string buf_1 = variable_name;
    std::string buf_2 = assign_value;
    char *env_1 = new char[buf_1.size()];
    char *env_2 = new char[buf_2.size()];
    std::strcpy(env_1,buf_1.c_str());
    std::strcpy(env_2,buf_2.c_str());
    setenv(env_1, env_2, 1); 
    return 0;
}

int my_unsetenv(const std::string& variable_name){
    std::string buf = variable_name;
    char *env = new char[buf.size()];
    std::strcpy(env,buf.c_str());
    unsetenv(env); 
    return 0;
}

int my_exit(){
    exit(1);
    return 0;
}