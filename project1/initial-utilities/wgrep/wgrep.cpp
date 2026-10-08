#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include <string>
#include <bits/stdc++.h>
using namespace std;

void wgrep(int file_descriptor, char* target) {
    char buf[1024];
    string input = "";
    ssize_t status;
    
    do {
        status = read(file_descriptor, buf, 1024);
        if (status > 0) input.append(buf, status);
    } while (status > 0);

    stringstream input_stream(input);
    string line;

    while (getline(input_stream, line, '\n')) {
        if (line.find(target) != string::npos) {
            line += "\n";
            write(STDOUT_FILENO, line.c_str(), line.size() * sizeof(char));
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        string error_str = "wgrep: searchterm [file ...]\n";
        write(STDOUT_FILENO, error_str.c_str(), error_str.size() * sizeof(char));
        exit(1);
    }

    char* target = argv[1];
    if (argc == 2) {
        wgrep(STDIN_FILENO, target);
    }
    else {
        for (int i = 2; i < argc; i++) {
            int fd = open(argv[i], O_RDONLY);
            if (fd == -1) {
                string error_str = "wgrep: cannot open file\n";
                write(STDOUT_FILENO, error_str.c_str(), error_str.size() * sizeof(char));
                exit(1);
            }

            wgrep(fd, target);
            close(fd);
        }
    }

    return 0;
}