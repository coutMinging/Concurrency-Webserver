struct q_item {
    int client_fd;
    int priority;
    char *path;
    int delay;
};

void create_queue(int max_queue_size);
int add_work(int fd, int p, char *path, int delay);
int get_highest_priority();
struct q_item *get_work();
struct q_item *get_work_nonblocking();
void destroy_queue();