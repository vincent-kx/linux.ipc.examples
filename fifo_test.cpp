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
//for mkfifo
#include <sys/stat.h>

using namespace std;

#define FILE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
#define BUF_SIZE 16

void print_errmsg(string errmsg)
{
    cout << errmsg << ",errno:" << errno << ",errmsg:" << strerror(errno) << endl;
    //reset
    errno = 0;
}

int set_non_block(int fd)
{
    int flags = fcntl(fd, F_GETFL);
    if (flags < 0)
    {
        print_errmsg("fcntl failed");
        return -1;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void client()
{
    const char * fifo1 = "./test.fifo1";
    const char * fifo2 = "./test.fifo2";
    const char * file = "./fifo.test";

    int fifo1_fd = open(fifo1,O_WRONLY);
    if(fifo1_fd <= 0) {
        unlink(fifo1);
        unlink(fifo2);
        exit(-1);
    }

    int fifo2_fd = open(fifo2,O_RDONLY);
    if(fifo1_fd <= 0) {
        close(fifo1_fd);
        unlink(fifo1);
        unlink(fifo2);
        exit(-1);
    }
    int fd = open(file,O_RDONLY);
    if(fd < 0) {
        close(fifo1_fd);
        close(fifo2_fd);
        unlink(fifo1);
        unlink(fifo2);
        exit(-1);
    }
    set_non_block(fd);
    set_non_block(fifo1_fd);
    char line[BUF_SIZE] = {0};
    ssize_t nbytes = 0;
    //读出文件中的内容，发送给server
    while( (nbytes = read(fd,line,BUF_SIZE)) > 0) {
        write(fifo1_fd,line,nbytes);
        memset(line,0,BUF_SIZE);//clear
        //read server response
        read(fifo2_fd,line,BUF_SIZE);
        cout << "client recv:" << string(line);
    }
    // write(fifo1_fd,"OK",2);
}

void server()
{
    const char * fifo1 = "./test.fifo1";
    const char * fifo2 = "./test.fifo2";
    int ret = mkfifo(fifo1,FILE_MODE);
    if(ret < 0) {
        print_errmsg("server mkfifo failed");
        exit(-1);
    }
    ret = mkfifo(fifo2,FILE_MODE);
    if(ret < 0) {
        unlink(fifo1);
        print_errmsg("server mkfifo failed");
        exit(-1);
    }
    int fifo1_fd = open(fifo1,O_RDONLY);
    if(fifo1_fd <= 0) {
        print_errmsg("open fifo1 failed");
        unlink(fifo1);
        unlink(fifo2);
        exit(-1);
    }

    int fifo2_fd = open(fifo2,O_WRONLY);
    if(fifo2_fd <= 0) {
        print_errmsg("open fifo2 failed");
        close(fifo1_fd);
        unlink(fifo1);
        unlink(fifo2);
        exit(-1);
    }

    set_non_block(fifo2_fd);
    ssize_t nbytes = 0;
    char line[BUF_SIZE] = {0};
    while( (nbytes = read(fifo1_fd,line,BUF_SIZE)) > 0) {
        cout << "server recv:" << string(line);
        if(strcmp(line,"\n") == 0) { //文件传输结束了
            cout << "server end" << endl;
            close(fifo1_fd);
            close(fifo2_fd);
            unlink(fifo1);
            unlink(fifo2);
            sleep(1);
            break;
        }
        write(fifo2_fd,line,nbytes); //将接收到的数据写会给client
        memset(line,0,BUF_SIZE);
    }
    cout << endl;
}

int main()
{
    // client();
    server();
}