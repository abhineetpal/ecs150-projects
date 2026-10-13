#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <string>
using namespace std;

int main(int argc, char** argv) {
    if (argc < 2) {
        string error_str = "wunzip: file1 [file2 ...]\n";
        write(STDOUT_FILENO, error_str.c_str(), error_str.size() * sizeof(char));
        exit(1);
    }

    for (int i = 1; i < argc; i++) {
        int fd = open(argv[1], O_RDONLY);
        if (fd == -1) {
            perror(strerror(errno));
            exit(1);
        }

        ssize_t status;
        uint32_t count;
        char ch;
        do {
            status = read(fd, &count, sizeof(uint32_t));
            if (status == -1) {
                perror(strerror(errno));
                exit(1);
            }
            else if (status == 0) break;

            status = read(fd, &ch, sizeof(char));
            if (status == -1) {
                perror(strerror(errno));
                exit(1);
            }
            else if (status == 0) break;

            for (uint32_t j = 0; j < count; j++) {
                write(STDOUT_FILENO, &ch, sizeof(char));
            }
        } while (status > 0);

        if (close(fd) == -1) {
            perror(strerror(errno));
            exit(1);
        }
    } 
}