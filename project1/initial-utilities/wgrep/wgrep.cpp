#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include <string>
#include <vector>
using namespace std;

int main(int argc, char** argv) {
    if (argc < 2) {
        string error_str = "wgrep: searchterm [file ...]\n";
        write(STDOUT_FILENO, error_str.c_str(), error_str.size() * sizeof(char));
        exit(1);
    }

    char* target = argv[1];
    if (argc == 2) {
        char ch;
        string line = "";
        vector<string> input;

        while (read(STDIN_FILENO, &ch, 1) > 0) {
            if (ch != '\n') line += ch;
            else {
                input.push_back(line);
                line = "";
            }
        }

        for (string input_line : input) {
            if (input_line.find(target) != string::npos) {
                input_line += "\n";
                write(STDOUT_FILENO, input_line.c_str(), input_line.size() * sizeof(char));
            }
        }
    }
    else {
        for (int i = 2; i < argc; i++) {
            int fd = open(argv[i], O_RDONLY);
            if (fd == -1) {
                string error_str = "wgrep: cannot open file\n";
                write(STDOUT_FILENO, error_str.c_str(), error_str.size() * sizeof(char));
                exit(1);
            }

            char ch;
            string line = "";
            vector<string> input;

            while (read(fd, &ch, 1) > 0) {
                if (ch != '\n') line += ch;
                else {
                    input.push_back(line);
                    line = "";
                }
            }

            for (string input_line : input) {
                if (input_line.find(target) != string::npos) {
                    input_line += "\n";
                    write(STDOUT_FILENO, input_line.c_str(), input_line.size() * sizeof(char));
                }
            }

            close(fd);
        }
    }

    return 0;
}