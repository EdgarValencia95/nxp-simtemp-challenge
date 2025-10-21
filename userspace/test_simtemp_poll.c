#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <string.h>

struct simtemp_sample {
    uint64_t timestamp_ns;
    int32_t temp_mC;
    uint32_t flags;
} __attribute__((packed));

int main() {
    int fd;
    struct simtemp_sample sample;
    struct pollfd fds[1];
    int ret;
    int i;

    printf("=== Testing poll() support ===\n\n");

    fd = open("/dev/simtemp", O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        perror("Failed to open /dev/simtemp");
        return 1;
    }

    fds[0].fd = fd;
    fds[0].events = POLLIN;

    printf("Waiting for data with poll() (timeout 5 seconds)...\n\n");

    for (i = 0; i < 10; i++) {
        printf("Poll attempt %d: ", i + 1);
        fflush(stdout);

        /* Wait for data with 5 second timeout */
        ret = poll(fds, 1, 5000);
        
        if (ret < 0) {
            perror("poll failed");
            break;
        } else if (ret == 0) {
            printf("Timeout! No data available.\n");
            continue;
        }

        /* Check what events occurred */
        if (fds[0].revents & POLLIN) {
            printf("Data available! ");
            
            /* Read the data */
            ret = read(fd, &sample, sizeof(sample));
            if (ret == sizeof(sample)) {
                printf("Temp=%d.%03d°C [0x%02x]\n",
                       sample.temp_mC / 1000,
                       abs(sample.temp_mC % 1000),
                       sample.flags);
            } else if (ret < 0) {
                if (errno == EAGAIN) {
                    printf("EAGAIN (spurious wakeup)\n");
                } else {
                    perror("read failed");
                    break;
                }
            }
        }

        if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            printf("Error event: 0x%x\n", fds[0].revents);
            break;
        }

        /* Small delay between polls */
        usleep(200000); /* 200ms */
    }

    close(fd);
    printf("\nTest completed.\n");
    return 0;
}
