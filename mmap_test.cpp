#include <iostream>
#include <string>
//for mmap
#include <sys/mman.h>
//for sem_t
#include <semaphore.h>
//for write
#include <unistd.h>
//for exit
#include <stdlib.h>
//for open
#include <sys/types.h>
#include <sys/stat.h>
//for waitpid
#include <sys/wait.h>
#include <fcntl.h>
#include <pthread.h>
//for memcpy
#include <memory.h>

using namespace std;

void test_mmap_access()
{
    size_t page_size = sysconf(_SC_PAGESIZE);
    cout << "pagesize=" << page_size << endl;
    string s("");
    s.append(page_size, 'B');
    s.append(10, 'C');
    int fd = open("./test.txt", O_RDWR | O_CREAT | O_TRUNC, 00700);
    write(fd, s.c_str(), s.length());
    close(fd);

    fd = open("./test.txt", O_RDWR);
    void *addr = mmap(NULL, 15000, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    char *ptr = (char *)addr;
    cout << "value[0]=" << ptr[0] << endl;
    cout << "value[4095]=" << ptr[page_size - 1] << endl;
    cout << "value[4096]=" << ptr[page_size] << endl;
    cout << "value[4106]=" << ptr[page_size + 10] << endl;
    ptr[0] = 'X';                  //文件第一个字节，如期写入文件
    ptr[page_size - 1] = 'X';      //文件第一页的最后一个字节，如期写入文件
    ptr[page_size - 1 + 10] = 'Y'; //文件最后的一个字节，如期写入文件
    ptr[page_size + 20] = 'Z';     //超过文件大小的位置，但是在两页的范围内，如期未写入文件
    //ptr[page_size*2+1] = 'O';//超过两页大小的位置但是在映射的内存区范围内，如期bus error
    // ptr[4*page_size] = 'O'; //超过内存映射区的'页'第一个位置，centos7上居然没有任何错误，不知为何
    munmap(addr, 15000);
    close(fd);
}

typedef struct SharedObj
{
    int len;
    char str[16];
} SharedObj;

void test_anonymous_mmap()
{
    sem_t *mutex;
    SharedObj *shared_obj = (SharedObj *)mmap(NULL, sizeof(SharedObj), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    //初始化共享内存区
    shared_obj->len = 0;
    shared_obj->str[0] = '\0';

    pid_t child_pid = fork();

    if (child_pid == 0)
    {
        string hello = "hello world";
        shared_obj->len = hello.length();
        memcpy(shared_obj->str, hello.c_str(), hello.length());
        cout << "child:" << getpid() << " wrote [str:" << shared_obj->str << ",len:" << shared_obj->len << "]" << endl;
        cout << "child share_obj addr:" << ios_base::hex << shared_obj << endl;
        munmap(shared_obj, sizeof(SharedObj));
        exit(0);
    }
    else if (child_pid > 0)
    {
        sleep(1);
        waitpid(child_pid, NULL, 0);
        cout << "parent:" << getpid() << " read [str:" << shared_obj->str << ",len:" << shared_obj->len << "]" << endl;
        cout << "parent share_obj addr:" << ios_base::hex << shared_obj << endl;
        munmap(shared_obj, sizeof(SharedObj));
    }
    else
    {
        cout << "fork child fail" << endl;
        exit(-1);
    }
}

void test_share_memory_producer(const char *shmFile, off_t length)
{
    int fd = shm_open(shmFile, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd < 0)
    {
        cout << "test_share_memory_producer shm_open failed" << endl;
        exit(-1);
    }
    int res = ftruncate(fd, length);
    if (res != 0)
    {
        cout << "test_share_memory_producer ftruncate failed" << endl;
        shm_unlink(shmFile);
        exit(-1);
    }
    void *ptr = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (MAP_FAILED == ptr)
    {
        cout << "test_share_memory_producer mmap failed" << endl;
        shm_unlink(shmFile);
        exit(-1);
    }
    string data = "hello world !";
    memcpy(ptr, data.c_str(), data.length());
    msync(ptr, data.length(), MS_SYNC);
    munmap(ptr, length);
    sleep(10);
    shm_unlink(shmFile);
}

void test_share_memory_consumer(const char *shmFile)
{
    sleep(10);
    int fd = shm_open(shmFile, O_RDWR, S_IRUSR | S_IWUSR);
    if (fd < 0)
    {
        cout << "test_share_memory_consumer shm_open fail" << endl;
        exit(-1);
    }
    struct stat shmObjStat;
    memset(&shmObjStat, 0, sizeof(struct stat));
    int ret = fstat(fd, &shmObjStat);
    if (0 != ret)
    {
        cout << "test_share_memory_consumer fstat fail" << endl;
        shm_unlink(shmFile);
        exit(-1);
    }
    off_t size = shmObjStat.st_size;
    char *data = (char *)mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    cout << "shmObj size:" << size << ",client write data:" << data << endl;
    munmap(data, size);
    shm_unlink(shmFile);
}

void test_share_memory()
{
    const char *shmFile = "./test.shm.file";
    size_t length = 100;
    pid_t childPid = fork();
    if (childPid == 0)
    {
        //child
        test_share_memory_producer(shmFile, length);
        cout << "child exit" << endl;
        exit(0);
    }
    else if (childPid > 0)
    {
        //parent
        test_share_memory_consumer(shmFile);
        waitpid(childPid, NULL, 0);
    }
    else
    {
        exit(-1);
    }
}

int main()
{
    // test_mmap_access();
    // test_anonymous_mmap();
    // test_share_memory();
    const char *shmFile = "./test.shm.file";
    size_t length = 100;
    test_share_memory_producer(shmFile, length);
    // test_share_memory_consumer(shmFile);
    return 0;
}
