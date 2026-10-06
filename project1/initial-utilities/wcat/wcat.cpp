#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <string>
using namespace std;

int main(int argc, char** argv) {
    if (argc < 2) {
        exit(0);
    }

    for (int i = 1; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY);
        if (fd == -1) {
            string error_str = "wcat: cannot open file\n";
            write(STDOUT_FILENO, error_str.c_str(), error_str.size() * sizeof(char));
            exit(1);
        }

        char buf[1024];
        ssize_t status;
        do {
            status = read(fd, buf, 1024);
            if (status == -1) {
                perror(strerror(errno));
                exit(1);
            }
            write(STDOUT_FILENO, buf, status * sizeof(char));
        } while (status != 0);

        if (close(fd) == -1) {
            perror(strerror(errno));
            exit(1);
        }
    }

    return 0;
}