#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include <string>
#include <vector>
#include <ios>
#include <bits/stdc++.h>
#include <sys/wait.h>

using namespace std;

char error_message[30] = "An error has occurred\n";
string prompt = "wish> ";
vector<string> path_vars;
const string WHITESPACE = " \n\r\t\f\v";

string trim_left(string str) {
    size_t start = str.find_first_not_of(WHITESPACE);
    return (start == string::npos) ? "" : str.substr(start);
}

string trim_right(string str) {
    size_t end = str.find_last_not_of(WHITESPACE);
    return (end == string::npos) ? "" : str.substr(0, end + 1);
}

string trim(string str) {
    return trim_right(trim_left(str));
}

void run_cmd(string cmd) {
    cmd = trim(cmd);

    if (cmd == "") return;
    if (cmd == "exit") exit(0);
    if (cmd.substr(0, 3) == "cd ") {
        string path = cmd.substr(3);
        if (path == "") {
            write(STDERR_FILENO, error_message, strlen(error_message));
            return;
        }

        path = trim_left(path); // there could be many spaces between "cd" and the input path
        if (chdir(path.c_str()) == -1) { // don't check if there are 2 or more arguments because that will also cause chdir to fail
            write(STDERR_FILENO, error_message, strlen(error_message));
            return;
        }
        return;
    }

    string cmd_name;
    string args_str = "";
    vector<string> arg_vec;
    size_t space_idx = cmd.find(" ");
    if (space_idx == string::npos) {
        cmd_name = cmd;
        arg_vec.push_back(cmd_name);
    }
    else {
        cmd_name = cmd.substr(0, space_idx);
        args_str = cmd.substr(space_idx + 1);

        arg_vec.push_back(cmd_name);
        if (args_str != "") {
            stringstream args_stream(args_str);
            string arg;
            while (getline(args_stream, arg, ' ')) {
                if (!all_of(arg.begin(), arg.end(), ::isspace)) arg_vec.push_back(arg);
            }
        }
    }

    size_t num_args = arg_vec.size();
    char* arg_arr[num_args + 1];
    for (size_t i = 0; i < num_args; i++) {
        arg_arr[i] = (char*)arg_vec[i].c_str();
    }
    arg_arr[num_args] = NULL;

    string cmd_path;
    int cmd_found = 0;
    for (string var : path_vars) {
        cmd_path = var + "/" + cmd_name;
        cmd_found = access(cmd_path.c_str(), X_OK) == 0;
        if (cmd_found) break;
    }

    if (cmd_found) {
        pid_t pid = fork();
        if (pid == -1) write(STDERR_FILENO, error_message, strlen(error_message));
        else if (pid == 0) {
            execv(cmd_path.c_str(), arg_arr);
            write(STDERR_FILENO, error_message, strlen(error_message)); // should not reach here if execv successful!
        }
        else {
            int wstatus;
            wait(&wstatus);
            if (wstatus == -1) write(STDERR_FILENO, error_message, strlen(error_message));
        }
    }
    else write(STDERR_FILENO, error_message, strlen(error_message));
}

int main(int argc, char** argv) {
    path_vars.push_back("/bin");

    if (argc > 2) {
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    }
    else if (argc > 1) {
        int fd = open(argv[1], O_RDONLY);
        if (fd == -1) {
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1);
        }

        char buf[1024];
        string file_str = "";
        ssize_t status;
        do {
            status = read(fd, buf, 1024);
            if (status > 0) file_str.append(buf, status);
            else if (status == -1) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                exit(1);
            }
        } while (status > 0);

        stringstream input_stream(file_str);
        string cmd;
        while (getline(input_stream, cmd)) run_cmd(cmd);
        
        if (close(fd) == -1) {
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1);
        }
    }
    else {
        write(STDOUT_FILENO, prompt.c_str(), prompt.size());

        string cmd;
        while (getline(cin, cmd)) {
            run_cmd(cmd);
            write(STDOUT_FILENO, prompt.c_str(), prompt.size());
        }
    }

    return 0;
}