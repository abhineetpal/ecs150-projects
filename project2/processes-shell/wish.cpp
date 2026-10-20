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

typedef struct {
    vector<string> args;
    string cmd_path;

    bool is_redirect;
    bool is_valid;

    int redirect_outfile_fd;
} Command_t;

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

// creates a Command_t struct from a string (not intended for built-in commands)
Command_t parse_ext_cmd(string cmd) {
    Command_t cmd_handle;

    cmd = trim(cmd);
    if (cmd == "") {
        cmd_handle.is_valid = false;
        return cmd_handle;
    }

    size_t redirect_op_idx = cmd.find('>');
    string redirect_outfile_str;
    int redirect_outfile_fd;
    if (redirect_op_idx != string::npos) {
        redirect_outfile_str = cmd.substr(redirect_op_idx + 1);
        redirect_outfile_str = trim_left(redirect_outfile_str); // there could be more whitespace between redirect operator and filename

        // Multiple redirection operators are illegal
        // Finding whitespace within the string after trimming leading whitespace indicates another file (illegal!)
        if ((redirect_outfile_str.find('>') != string::npos) || (redirect_outfile_str.find_first_of(WHITESPACE) != string::npos)) {
            write(STDERR_FILENO, error_message, strlen(error_message));
            cmd_handle.is_valid = false;
            return cmd_handle;
        }

        cmd = cmd.substr(0, redirect_op_idx);
        if (cmd == "") {
            write(STDERR_FILENO, error_message, strlen(error_message));
            cmd_handle.is_valid = false;
            return cmd_handle;
        }

        redirect_outfile_fd = open(redirect_outfile_str.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (redirect_outfile_fd == -1) {
            write(STDERR_FILENO, error_message, strlen(error_message));
            cmd_handle.is_valid = false;
            return cmd_handle;
        }
        cmd_handle.redirect_outfile_fd = redirect_outfile_fd;
        cmd_handle.is_redirect = true;
    }
    else cmd_handle.is_redirect = false;

    stringstream args_stream(cmd);
    vector<string> arg_vec;
    string arg;
    while (args_stream >> arg) arg_vec.push_back(arg);

    string cmd_name = arg_vec[0];
    string cmd_path;
    bool cmd_found = false;
    for (string var : path_vars) {
        cmd_path = var + "/" + cmd_name;
        cmd_found = access(cmd_path.c_str(), X_OK) == 0;
        if (cmd_found) break;
    }

    if (cmd_found) {
        cmd_handle.is_valid = true;
        cmd_handle.cmd_path = cmd_path;
        cmd_handle.args = arg_vec;
    }
    else {
        write(STDERR_FILENO, error_message, strlen(error_message));
        cmd_handle.is_valid = false;
    }

    return cmd_handle;
}

void run_cmd(string cmd) {
    cmd = trim(cmd);
    if (cmd == "") return;
    if (cmd == "exit") exit(0);
    if (cmd.substr(0, 2) == "cd") {
        if (cmd == "cd") {
            write(STDERR_FILENO, error_message, strlen(error_message));
            return;
        }
        if (isspace(cmd[2])) { // deals with the case where you call a command that starts with "cd" but is not cd
            string path = cmd.substr(3);
            path = trim_left(path); // there could be more whitespace between "cd" and the input path
            
            // Finding whitespace within the string after trimming leading whitespace indicates another argument (illegal!)
            if (path.find_first_of(WHITESPACE) != string::npos) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                return;
            }

            if (chdir(path.c_str()) == -1) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                return;
            }
            return;
        }
    }
    if (cmd.substr(0, 4) == "path") {
        // This is a path command if there is whitespace after path or the command is just "path"
        if (isspace(cmd[4]) || (cmd == "path")) {
            string paths = cmd.substr(4);
            path_vars.clear();

            stringstream path_stream(paths);
            string path_var;
            while (path_stream >> path_var) path_vars.push_back(path_var);
            return;
        }
    }

    vector<Command_t> cmd_handles;
    if (cmd.find('&') != string::npos) {
        stringstream cmd_stream(cmd);
        string individual_cmd;
        while (getline(cmd_stream, individual_cmd, '&')) cmd_handles.push_back(parse_ext_cmd(individual_cmd));
    }
    else cmd_handles.push_back(parse_ext_cmd(cmd));

    for (Command_t cmd_handle : cmd_handles) {
        if (cmd_handle.is_valid) {
            size_t num_args = cmd_handle.args.size();
            char* arg_arr[num_args + 1];
            for (size_t i = 0; i < num_args; i++) {
                arg_arr[i] = (char*)cmd_handle.args[i].c_str();
            }
            arg_arr[num_args] = NULL;

            pid_t pid = fork();
            if (pid == -1) write(STDERR_FILENO, error_message, strlen(error_message));
            else if (pid == 0) {
                if (cmd_handle.is_redirect) {
                    if ((dup2(cmd_handle.redirect_outfile_fd, STDOUT_FILENO) == -1) ||
                        (dup2(cmd_handle.redirect_outfile_fd, STDERR_FILENO) == -1)) {
                        write(STDERR_FILENO, error_message, strlen(error_message));
                        return;
                    }
                    close(cmd_handle.redirect_outfile_fd);
                }

                execv(cmd_handle.cmd_path.c_str(), arg_arr);
                write(STDERR_FILENO, error_message, strlen(error_message)); // should not reach here if execv successful!
            }
        }
    }

    int wstatus;
    while (wait(&wstatus) != -1); // wait errors out when there are no child processes to wait for
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