#include <iostream>
//for pipe
#include <unistd.h>
//for exit
#include <stdlib.h>
//for fcntl
#include <fcntl.h>
// for strerror
#include <string.h>
//for errno
#include <errno.h>
//for waitpid
#include <sys/wait.h>

using namespace std;

void print_errmsg(string errmsg) {
    cout << errmsg << ",errno:" << errno << ",errmsg:" << strerror(errno) << endl;
    //reset
    errno = 0;
}

int set_non_block(int fd) {
    int flags = fcntl(fd,F_GETFL);
    if(flags < 0) {
        print_errmsg("fcntl failed");
        return -1;
    }
    return fcntl(fd,F_SETFL, flags | O_NONBLOCK);
}

void test_pipe_mode1()
{
    int pipeFd[2] = {0};
    int ret = pipe(pipeFd);
    if(0 != ret) {
        print_errmsg("failed to make pipe");
        exit(-1);
    }

    pid_t cpid = fork();
    if(cpid == 0) { //child
        string child_say = "老爸，我要吃糖果";
        write(pipeFd[1],child_say.c_str(),child_say.length());
        close(pipeFd[1]);
        exit(0);
    } else if (cpid > 0) { //parent
        sleep(1);
        char farther_listen[64];
        int nbytes = 0;
        write(STDOUT_FILENO,"my son said:",strlen("my son said:"));
        if(0 != set_non_block(pipeFd[0])) exit(-1);
        while((nbytes = read(pipeFd[0],farther_listen,64)) > 0) {
            write(STDOUT_FILENO,farther_listen,nbytes);
        }
        
        write(STDOUT_FILENO,"\n",1);
        close(pipeFd[0]);
        waitpid(cpid,NULL,0);
        exit(0);
    } else {
        print_errmsg("failed to fork");
    }
}

void producer(int readFd,int writeFd) {
    if(0 != set_non_block(writeFd)) exit(-1);
    string request = "AAAAAAAAAAAABBBBBBBBBBBBBBBBCCCCCCCCCCCCCCCCCDDDDDDDDDDDDDDDDDDD\n";
    char response[64];
    ssize_t nbytes = write(writeFd,request.c_str(),request.length());
    close(writeFd);
    while( (nbytes = read(readFd,response,64)) > 0) {
        if(strcmp(response,"OK")) {
            cout << "product recv ok response" << endl;
            close(readFd);
            return;
        }
    }
}

void consumer(int readFd,int writeFd) {
    if(0 != set_non_block(writeFd)) exit(-1);
    sleep(1);
    char request[64];
    string response = "OK";
    ssize_t nbytes = 0;
    write(STDOUT_FILENO,"consumer recv:",strlen("consumer recv:"));
    while((nbytes = read(readFd,request,64)) > 0) {
        write(STDOUT_FILENO,request,nbytes);
    }
    close(readFd);
    write(writeFd,"OK",2);
    close(writeFd);
}

void test_pipe_mode2() {
    int pipeFd1[2] = {0};
    int pipeFd2[2] = {0};

    int ret = pipe(pipeFd1);
    if(0 != ret) {
        print_errmsg("failed to make pipe");
        exit(-1);
    }

    ret = pipe(pipeFd2);
    if(0 != ret) {
        close(pipeFd1[0]);
        close(pipeFd1[1]);
        print_errmsg("failed to make pipe");
        exit(-1);
    }
    pid_t cpid = fork();
    if(cpid == 0) { //child
        close(pipeFd1[1]);
        close(pipeFd2[0]);
        producer(pipeFd1[0],pipeFd2[1]);
        exit(0);
    }else if (cpid > 0) { //parent
        close(pipeFd1[0]);
        close(pipeFd2[1]);
        consumer(pipeFd2[0],pipeFd1[1]);
        waitpid(cpid,NULL,0);
        exit(0);
    } else {
        print_errmsg("failed to fork");
    }
}

int main() {
    test_pipe_mode2();
}