#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <string>
using namespace std;

int main(int argc, char** argv) {
    if (argc < 2) {
        string error_str = "wzip: file1 [file2 ...]\n";
        write(STDOUT_FILENO, error_str.c_str(), error_str.size() * sizeof(char));
        exit(1);
    }

    char ch;
    char prev_ch;
    uint32_t count = 0;
    int first_read = 1;

    for (int i = 1; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY);
        if (fd == -1) {
            perror(strerror(errno));
        }

        ssize_t status;
        do {
            status = read(fd, &ch, 1);
            if (status == -1) {
                perror(strerror(errno));
            }

            if (status > 0) {
                if (first_read) {
                    first_read = 0;
                    count++;
                }
                else {
                    if (ch == prev_ch) count++;
                    else {
                        write(STDOUT_FILENO, &count, sizeof(uint32_t));
                        write(STDOUT_FILENO, &prev_ch, sizeof(char));

                        count = 1;
                    }
                }

                prev_ch = ch;
            }
            else if (status == 0) {
                if (i == (argc - 1)) {
                    write(STDOUT_FILENO, &count, sizeof(uint32_t));
                    write(STDOUT_FILENO, &prev_ch, sizeof(char));
                }
            }
        } while (status > 0);

        if (close(fd) == -1) {
            perror(strerror(errno));
            exit(1);
        }
    }
}