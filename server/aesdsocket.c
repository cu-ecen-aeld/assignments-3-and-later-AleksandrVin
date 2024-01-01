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
#include <errno.h>

#include <pthread.h>

#include <sys/queue.h>

#define PORT 9000
#define BUFF_SIZE 100 + 1 // +1 for null character
// #define EXIT_FAILURE -1

int sk = -1;

int exit_flag = 0;

#ifndef USE_AESD_CHAR_DEVICE
#define USE_AESD_CHAR_DEVICE 1
#endif
#if USE_AESD_CHAR_DEVICE == 1
#define AESD_FILE "/dev/aesdchar"
#else
#define AESD_FILE "/var/tmp/aesdsocketdata"
#endif

// struct for linked list
struct node
{
    // thread id
    pthread_t tid;
    int client_sk;
    int fd;
    // client address
    struct sockaddr_in client_addr;
    // mutex reference
    pthread_mutex_t *mutex;
    // finished flag
    int finished; // 0 - not finished, 1 - finished
    // linked list
    TAILQ_ENTRY(node)
    nodes;
};

#define JOIN_FINISHED_THREADS(node, head, nodes)         \
    TAILQ_FOREACH(node, &head, nodes)                     \
    {                                                     \
        if (node->finished == 1)                          \
        {                                                 \
            int ret = pthread_join(node->tid, NULL);      \
            if (ret < 0)                                  \
            {                                             \
                syslog(LOG_ERR, "pthread_join() failed"); \
                exit(EXIT_FAILURE);                       \
            }                                             \
            TAILQ_REMOVE(&head, node, nodes);             \
            free(node);                                   \
        }                                                 \
    }

// thread function
static void *thread_start(void *arg)
{
    struct node *node = arg;

    char buf[BUFF_SIZE];
    buf[BUFF_SIZE - 1] = '\0';

    int new_line = 0;

    while (new_line == 0)
    {
        int bytes_read = read(node->client_sk, buf, sizeof(buf));
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

        // lock the mutex
        pthread_mutex_lock(node->mutex);

        int bytes_written = write(node->fd, buf, bytes_read);
        if (bytes_written < 0)
        {
            syslog(LOG_ERR, "write() failed");
            exit(EXIT_FAILURE);
        }

        // unlock the mutex
        pthread_mutex_unlock(node->mutex);
    }

    // save fd position
    off_t fd_pos = lseek(node->fd, 0, SEEK_CUR);

    // lock mutex
    pthread_mutex_lock(node->mutex);

    // lseek to the beginning of the file
    if(lseek(node->fd, 0, SEEK_SET) != 0)
    {
        syslog(LOG_ERR, "can't lseek to beginning");
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        int bytes_read = read(node->fd, buf, sizeof(buf));
        if (bytes_read < 0)
        {
            syslog(LOG_ERR, "read() failed");
            exit(EXIT_FAILURE);
        }

        if (bytes_read == 0)
        {
            break;
        }

        int bytes_written = write(node->client_sk, buf, bytes_read);
        if (bytes_written < 0)
        {
            syslog(LOG_ERR, "write() failed");
            exit(EXIT_FAILURE);
        }
    }

    // lseek to the saved position
    lseek(node->fd, fd_pos, SEEK_SET);

    // unlock mutex
    pthread_mutex_unlock(node->mutex);

    // syslog that connection closed
    syslog(LOG_INFO, "Closed connection from %s", inet_ntoa(node->client_addr.sin_addr));

    close(node->client_sk);

    node->finished = 1;

    return arg;
}

// thread function for printing timestamp every 10 seconds
static void *thread_timestamp(void *arg)
{
    struct node * node = arg;
    // enable thread cancellation
    int ret = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    if (ret < 0)
    {
        syslog(LOG_ERR, "pthread_setcancelstate() failed");
        exit(EXIT_FAILURE);
    }

    while (exit_flag == 0)
    {
        sleep(10);
#if USE_AESD_CHAR_DEVICE == 1
        // no timestamp for aesd-char-driver
#else
        //syslog(LOG_INFO, "Timestamp");
        // print RFC 2822 timestamp to fd
        time_t t = time(NULL);
        struct tm *tm = localtime(&t);
        char buf[100];
        strftime(buf, sizeof(buf), "timestamp:%a, %d %b %Y %T %z\n", tm);
        buf[sizeof(buf) - 1] = '\0';
        syslog(LOG_INFO, "%s", buf);
        // disable thread cancellation
        ret = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        if (ret < 0)
        {
            syslog(LOG_ERR, "pthread_setcancelstate() failed");
            exit(EXIT_FAILURE);
        }

        // lock the mutex
        pthread_mutex_lock(node->mutex);
        int bytes_written = write(node->fd, buf, strlen(buf));
        if (bytes_written < 0)
        {
            syslog(LOG_ERR, "write() failed");
            exit(EXIT_FAILURE);
        }
        // unlock the mutex
        pthread_mutex_unlock(node->mutex);
        // enable thread cancellation
        ret = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        if (ret < 0)
        {
            syslog(LOG_ERR, "pthread_setcancelstate() failed");
            exit(EXIT_FAILURE);
        }
#endif
    }

    node->finished = 1;

    return arg;
}

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

    syslog(LOG_INFO, "Server listening on port %d", PORT);

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

    int fd = open(AESD_FILE, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
    {
        syslog(LOG_ERR, "open() failed");
        exit(EXIT_FAILURE);
    }

    // create a mutex for looking fd
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);

    struct node node_timestamp;
    node_timestamp.fd = fd;
    node_timestamp.finished = 0;
    node_timestamp.mutex = &mutex;

    // start timestamp thread
    pthread_t timestamp_thread;
    ret = pthread_create(&timestamp_thread, NULL, thread_timestamp, &node_timestamp);
    if (ret < 0)
    {
        syslog(LOG_ERR, "pthread_create() failed");
        exit(EXIT_FAILURE);
    }

    // create a linked list
    TAILQ_HEAD(tailhead, node)
    head;
    TAILQ_INIT(&head);

    while (exit_flag == 0)
    {

        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int client_sk = accept(sk, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_sk < 0)
        {
            if (errno == EINTR)
            {
                syslog(LOG_INFO, "Caught signal, exiting");
                break;
            }
            syslog(LOG_ERR, "accept() failed %s", strerror(errno));
            exit(EXIT_FAILURE);
        }

        // syslog accepted connection from client
        syslog(LOG_INFO, "Accepted connection from %s", inet_ntoa(client_addr.sin_addr));

        struct node *node = malloc(sizeof(struct node));

        node->client_sk = client_sk;
        node->fd = fd;
        node->mutex = &mutex;
        node->finished = 0;
        node->client_addr = client_addr;

        TAILQ_INSERT_TAIL(&head, node, nodes);

        // create a thread
        pthread_t thread;
        ret = pthread_create(&thread, NULL, thread_start, node);
        if (ret < 0)
        {
            syslog(LOG_ERR, "pthread_create() failed");
            exit(EXIT_FAILURE);
        }

        node->tid = thread;

        // loop through the linked list and check if any thread has finished
        JOIN_FINISHED_THREADS(node, head, nodes)
    }

    // wait for all threads to finish
    while (!TAILQ_EMPTY(&head))
    {
        struct node *node;
        TAILQ_FOREACH(node, &head, nodes)
        {
            if (node->finished == 1)
            {
                ret = pthread_join(node->tid, NULL);
                if (ret < 0)
                {
                    syslog(LOG_ERR, "pthread_join() failed");
                    exit(EXIT_FAILURE);
                }
                TAILQ_REMOVE(&head, node, nodes);
                free(node);
            }
        }
    }

    // cancel timestamp thread
    ret = pthread_cancel(timestamp_thread);
    if (ret < 0)
    {
        syslog(LOG_ERR, "pthread_cancel() failed");
        exit(EXIT_FAILURE);
    }

    // join timestamp thread
    ret = pthread_join(timestamp_thread, NULL);
    if (ret < 0)
    {
        syslog(LOG_ERR, "pthread_join() failed");
        exit(EXIT_FAILURE);
    }

    close(fd);
    close(sk);
    // delete the file
#if USE_AESD_CHAR_DEVICE != 1
    unlink(AESD_FILE);
#endif

    return 0;
}