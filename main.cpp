#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <sys/sem.h>
#include <string.h>

union semun {
    int              val;    /* Value for SETVAL */
    struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
    unsigned short  *array;  /* Array for GETALL, SETALL */
    struct seminfo  *__buf;  /* Buffer for IPC_INFO
                                           (Linux-specific) */
};

void log(const std::string& message = std::string{});
void slave(int semaphores, int pipefd);
void master(int semaphores, int pipefd);

int main() {
    log("started");
    int semaphores = semget(1253, 2, IPC_CREAT | 0666);
    if (semaphores == -1) {
        log(strerror(errno));
    }

    unsigned short vals[] ={0,0};
    semun arg;
    arg.array = vals;
    if (semctl(semaphores, 0, SETALL, arg) == -1) {
        log(strerror(errno));
        return 1;
    }

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        log(strerror(errno));
        semctl(semaphores, 0, IPC_RMID);
        return 2;
    }

    __pid_t res = fork();
    switch(res) {
        case -1:
            log(strerror(errno));
            semctl(semaphores, 0, IPC_RMID);
            return 3;
        case 0:
            slave(semaphores, pipefd[0]);
            log("slave ended");
            raise(SIGKILL);
            return 0;
    }

    master(semaphores, pipefd[1]);
    semctl(semaphores, 0, IPC_RMID);
    close(pipefd[0]);
    close(pipefd[1]);
    return 0;
}

void master(int semaphores, int pipefd) {
    sembuf wait_for_slave;
    sembuf notify_slave;

    wait_for_slave.sem_num = 0;
    notify_slave.sem_num = 1;
    wait_for_slave.sem_op = -1;
    notify_slave.sem_op = 1;
    wait_for_slave.sem_flg = SEM_UNDO;
    notify_slave.sem_flg = SEM_UNDO;
    std::string buf;

    while(true)
    {
        if (semop(semaphores, &wait_for_slave, 1) == -1) {
            log(strerror(errno));
            break;
        }
        log("can accept message");
        std::getline(std::cin, buf);
        if (buf == "exit") {
            break;
        }
        size_t size = buf.size() + 1;
        write(pipefd, &size, sizeof(size_t));
        write(pipefd, buf.c_str(), buf.size() + 1);
        if (semop(semaphores, &notify_slave, 1) == -1) {
            log(strerror(errno));
            break;
        }
    }

}

void slave(int semaphores, int pipefd) {
    sembuf wait_for_master;
    sembuf notify_master;

    wait_for_master.sem_num = 1;
    notify_master.sem_num = 0;
    wait_for_master.sem_op = -1;
    notify_master.sem_op = 1;
    wait_for_master.sem_flg = SEM_UNDO;
    notify_master.sem_flg = SEM_UNDO;
    if (semop(semaphores, &notify_master, 1) == -1) {
        log(strerror(errno));
        return;
    }
    while(true) {
        if (semop(semaphores, &wait_for_master, 1) == -1) {
            log(strerror(errno));
            break;
        }
        size_t size;
        read(pipefd, &size, sizeof(size_t));
        char* buf = new char[size];
        read(pipefd, buf, size);
        log(buf);
        delete[] buf;

        if (semop(semaphores, &notify_master, 1) == -1) {
            log(strerror(errno));
            break;
        }
    }
}

void log(const std::string& message) {
    static __pid_t main_id = getpid();
    if (getpid() == main_id) {
        std::cout << "main process: ";
    }
    else {
        std::cout << "process " << std::to_string(getpid()) << ": ";
    }
    std::cout << message;
    if (message.size())
        std::cout << '\n';
}