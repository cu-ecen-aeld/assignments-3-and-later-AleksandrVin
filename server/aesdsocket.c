#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <syslog.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

#define PORT 9000
#define BUFF_SIZE 100 + 1 // +1 for null character
// #define EXIT_FAILURE -1

int sk = -1;

int exit_flag = 0;

// signal handler for SIGINT and SIGTERM
// close the socket and exit
static void signal_handler(int signo)
{
    if (signo == SIGINT || signo == SIGTERM)
    {
        syslog(LOG_INFO, "Caught signal, exiting");
        // shutdown the socket
        shutdown(sk, SHUT_RD);
        exit_flag = 1;
    }
}

int main(int argc, char *argv[])
{
    // Create a tcp socket server and bind to port 9000
    // Listen for incoming connections
    // Accept a connection
    // Read data from the socket
    // Write data to the file
    // Read data from the file
    // Write data to the socket
    // Close the connection
    // Close the socket
    // Close the file
    // Exit

    sk = socket(AF_INET, SOCK_STREAM, 0);
    if (sk < 0)
    {
        syslog(LOG_ERR, "socket() failed");
        exit(EXIT_FAILURE);
    }

    // use SO_REUSEADDR to reuse the port
    int opt = 1;
    int ret = setsockopt(sk, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (ret < 0)
    {
        syslog(LOG_ERR, "setsockopt() failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    ret = bind(sk, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (ret < 0)
    {
        syslog(LOG_ERR, "bind() failed");
        exit(EXIT_FAILURE);
    }

    // parse command line arguments
    while ((opt = getopt(argc, argv, "d")) != -1)
    {
        switch (opt)
        {
        case 'd':
            // daemonize the process
            if (daemon(0, 0) < 0)
            {
                perror("daemon() failed");
                exit(EXIT_FAILURE);
            }
            break;
        default:
            printf("Usage: %s [-d]", argv[0]);
        }
    }

    ret = listen(sk, 5);
    if (ret < 0)
    {
        syslog(LOG_ERR, "listen() failed");
        exit(EXIT_FAILURE);
    }

    // assign signal handler for SIGINT and SIGTERM using sigaction
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    int fd = open("/var/tmp/aesdsocketdata", O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
    {
        syslog(LOG_ERR, "open() failed");
        exit(EXIT_FAILURE);
    }

    while (exit_flag == 0)
    {

        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int client_sk = accept(sk, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_sk < 0)
        {
            syslog(LOG_ERR, "accept() failed");
            exit(EXIT_FAILURE);
        }

        // syslog accepted connection from client
        syslog(LOG_INFO, "Accepted connection from %s", inet_ntoa(client_addr.sin_addr));

        char buf[BUFF_SIZE];
        buf[BUFF_SIZE - 1] = '\0';

        int new_line = 0;

        while (new_line == 0)
        {
            int bytes_read = read(client_sk, buf, sizeof(buf));
            if (bytes_read < 0)
            {
                syslog(LOG_ERR, "read() failed");
                exit(EXIT_FAILURE);
            }

            if (bytes_read == 0)
            {
                break;
            }

            // find the new line character using strchr
            char *new_line_ptr = strchr(buf, '\n');
            if (new_line_ptr != NULL)
            {
                // new line character found
                bytes_read = (new_line_ptr - buf) + 1;
                new_line = 1;
            }

            int bytes_written = write(fd, buf, bytes_read);
            if (bytes_written < 0)
            {
                syslog(LOG_ERR, "write() failed");
                exit(EXIT_FAILURE);
            }
        }

        // lseek to the beginning of the file
        lseek(fd, 0, SEEK_SET);

        while (1)
        {
            int bytes_read = read(fd, buf, sizeof(buf));
            if (bytes_read < 0)
            {
                syslog(LOG_ERR, "read() failed");
                exit(EXIT_FAILURE);
            }

            if (bytes_read == 0)
            {
                break;
            }

            int bytes_written = write(client_sk, buf, bytes_read);
            if (bytes_written < 0)
            {
                syslog(LOG_ERR, "write() failed");
                exit(EXIT_FAILURE);
            }
        }

        // syslog that connection closed
        syslog(LOG_INFO, "Closed connection from %s", inet_ntoa(client_addr.sin_addr));

        close(client_sk);
    }

    close(fd);
    close(sk);
    // delete the file
    unlink("/var/tmp/aesdsocketdata");

    return 0;
}