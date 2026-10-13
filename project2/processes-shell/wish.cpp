#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include <string>
#include <vector>
#include <bits/stdc++.h>
#include <sys/wait.h>
using namespace std;

int main(int argc, char** argv) {
    char error_message[30] = "An error has occurred\n";
    vector<string> path_vars;
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

        while (getline(input_stream, cmd)) {
            string cmd_path;
            int cmd_found = 0;
            for (string var : path_vars) {
                cmd_path = var + "/" + cmd;
                cmd_found = access(cmd_path.c_str(), X_OK) == 0;
                if (cmd_found) break;
            }

            if (cmd_found) {
                pid_t pid = fork();
                if (pid == -1) write(STDERR_FILENO, error_message, strlen(error_message));
                else if (pid == 0) {
                    char arg = '\0';
                    execv(cmd_path.c_str(), (char* const*)&arg);
                }
                else {
                    int wstatus;
                    wait(&wstatus);
                    if (wstatus == -1) write(STDERR_FILENO, error_message, strlen(error_message));
                }
            }
            else write(STDERR_FILENO, error_message, strlen(error_message));
        }
    }
}