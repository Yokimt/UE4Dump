#pragma once
#ifndef _KERNEL_H_
#define _KERNEL_H_
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <string.h>
#include <errno.h>
#include <string>
#include <iostream>

#include <regex.h>
#include <sys/wait.h>
#define KERNEL_SU_OPTION 0xDEADBEEF
#define KSU_OPTIONS 0xdeadbeef

// KPM控制代码
#define CMD_KPM_CONTROL 28
#define CMD_KPM_CONTROL_MAX 7

// 控制代码
// prctl(xxx, 28, "PATH", "ARGS")
// success return 0, error return -N
#define SUKISU_KPM_LOAD 28

// prctl(xxx, 29, "NAME")
// success return 0, error return -N
#define SUKISU_KPM_UNLOAD 29

// num = prctl(xxx, 30)
// error return -N
// success return +num or 0
#define SUKISU_KPM_NUM 30

// prctl(xxx, 31, Buffer, BufferSize)
// success return +out, error return -N
#define SUKISU_KPM_LIST 31

// prctl(xxx, 32, "NAME", Buffer[256])
// success return +out, error return -N
#define SUKISU_KPM_INFO 32

// prctl(xxx, 33, "NAME", "ARGS")
// success return KPM's result value
// error return -N
#define SUKISU_KPM_CONTROL 33

// prctl(xxx, 34, buffer, bufferSize)
// success return KPM's result value
// error return -N
#define SUKISU_KPM_VERSION 34

#define CONTROL_CODE(n) (n)

int load_kpm(std::string path)
{
    int ret = -1;
    int out = -1;
    ret = prctl(KSU_OPTIONS, SUKISU_KPM_LOAD, path.c_str(), NULL, &out);
    if (out > 0)
    {
        std::cout << "load_kpm Success" << std::endl;
    }
    return ret;
}
int unload_kpm(std::string name)
{
    int ret = -1;
    int out = -1;
    ret = prctl(KSU_OPTIONS, SUKISU_KPM_UNLOAD, name.c_str(), NULL, &out);
    if (out > 0)
    {
        std::cout << "unload_kpm Success" << std::endl;
    }
    return ret;
}
int control_kpm(std::string name, std::string args)
{
    int ret = -1;
    int out = -1;
    ret = prctl(KSU_OPTIONS, CONTROL_CODE(SUKISU_KPM_CONTROL), name.c_str(), args.c_str(), &out);
    if (out > 0)
    {
        std::cout << "control_kpm Success" << std::endl;
    }
    return out;
}

enum
{
    HW_BREAKPOINT_LEN_1 = 1,
    HW_BREAKPOINT_LEN_2 = 2,
    HW_BREAKPOINT_LEN_4 = 4,
    HW_BREAKPOINT_LEN_8 = 8,
};

enum
{
    HW_BREAKPOINT_EMPTY = 0,
    HW_BREAKPOINT_R = 1,
    HW_BREAKPOINT_W = 2,
    HW_BREAKPOINT_RW = HW_BREAKPOINT_R | HW_BREAKPOINT_W,
    HW_BREAKPOINT_X = 4,
    HW_BREAKPOINT_INVALID = HW_BREAKPOINT_RW | HW_BREAKPOINT_X,
};

class kernel
{
private:
    struct kpm_read
    {
        uint16_t key;
        int pid;
        int size;
        uint64_t addr;
        void *buffer;
        void *ret;
    };
    struct kpm_mod
    {
        uint16_t key;
        int pid;
        char SoName[256];
        uintptr_t base;
        char pkg_name[256];
    };
    struct kpm_bkp
    {
        uint16_t key;
        int pid;
        uint64_t bp_addr;
        uint64_t bp_len;
        uint32_t bp_type;
        void *user_sample_hbp;
    };
    uint16_t key_vertify;
    uint16_t cmd_read;
    uint16_t cmd_write;
    uint16_t cmd_mod;
    uint16_t cmd_pid;
    uint16_t cmd_resgister_bkp;
    uint16_t cmd_unresgister_bkp;
    struct kpm_read kread;
    struct kpm_mod kmod;
    struct kpm_bkp bkp;
    int success = 0;
public:
    int cmd_ctl()
    {
        std::string key_cmd = "get_key";
        std::string kpm_name = "kernel-mem";
        int ret = control_kpm(kpm_name, key_cmd);
        if (ret == -1 || ret == -2 || ret == -3)
        {
            return ret;
        }

        key_vertify = ret & 0xFFFF;
        cmd_read = (ret >> 16) & 0xFFFF;
        std::cout << "cmd_read: " << cmd_read << std::endl;
        std::cout << "key_vertify: " << key_vertify << std::endl;
        init(cmd_read, key_vertify);
        return 0;
    }

    kernel() {};

    void init(uint16_t cmd, uint16_t key)
    {
        cmd_read = cmd; // 十六进制
        cmd_write = cmd + 1;
        cmd_mod = cmd + 2;
        cmd_pid = cmd + 3;
        cmd_resgister_bkp = cmd + 4;
        cmd_unresgister_bkp = cmd + 5;
        kread.key = key; // 十六进制
        kmod.key = key;
        bkp.key = key;
    };

    void set_pid(int pid)
    {
        kread.pid = pid;
        kmod.pid = pid;
        bkp.pid = pid;
    }

    template <typename T>
    T read(uint64_t addr)
    {
        T data;
        kread.ret=&success;
        kread.addr = addr & 0xffffffffffff;
        kread.size = sizeof(T);
        kread.buffer = &data;
        int ret = ioctl(-114, cmd_read, &kread);
        return data;
    }

    void read(uint64_t addr, void *buffer, int size)
    {
        kread.addr = addr & 0xffffffffffff;
        kread.size = size;
        kread.ret=&success;
        kread.buffer = buffer;
        int ret = ioctl(-114, cmd_read, &kread);
        // sycall(entry,-1, cmd_read,&kread);
        // ioctl(-1,cmd_read,&kread);
        //  if(ret<0){
        //      //std::cout<<"read error"<<std::endl;
        //  }
    }

    void write(uint64_t addr, void *buffer, int size)
    {
        kread.addr = addr & 0xffffffffffff;
        kread.size = size;
        kread.ret=&success;
        kread.buffer = buffer;
        int ret = ioctl(-114, cmd_write, &kread);
        // syscall(entry,-1, cmd_write,&kread);
        // ioctl(-1,cmd_write,&kread);
        //  if(ret<0){
        //      //std::cout<<"write error"<<std::endl;
        //  }
    }

    template <typename T>
    bool write(uint64_t addr, T data)
    {
        kread.addr = addr & 0xffffffffffff;
        kread.size = sizeof(T);
        kread.buffer = &data;
        kread.ret=&success;
        int ret = ioctl(-114, cmd_write, &kread);

        if (ret < 0)
        {
            return false;
        }
        return ret == 0;
    }

    uint64_t get_mod_base(std::string name)
    {
        strncpy(kmod.SoName, name.c_str(), sizeof(kmod.SoName) - 1);
        kmod.SoName[sizeof(kmod.SoName) - 1] = '\0';
        kmod.base = 0;
        int ret = ioctl(-114, cmd_mod, &kmod);
        std::cout << "get_mod_base " << std::hex << kmod.base << ", pid:" << std::dec << kmod.pid << std::endl;
        return kmod.base;
    }
    int get_pid(std::string name)
    {
        strncpy(kmod.pkg_name, name.c_str(), sizeof(kmod.pkg_name) - 1);
        kmod.pkg_name[sizeof(kmod.pkg_name) - 1] = '\0';
        kmod.pid = 0;
        int ret = ioctl(-114, cmd_pid, &kmod);
        std::cout << "get_pid:" << std::dec << kmod.pid << std::endl;
        set_pid(kmod.pid);
        return kmod.pid;
    }
    uint64_t set_bkp(uint64_t addr, uint64_t len, int type, uint64_t user_sample_hbp)
    {
        bkp.bp_addr = addr & 0xfffffffffff;
        bkp.bp_len = len;
        bkp.bp_type = type;
        bkp.user_sample_hbp = &user_sample_hbp;
        int ret = ioctl(-114, cmd_resgister_bkp, &bkp);
        std::cout << "set_bkp " << std::hex << bkp.bp_addr << ", pid:" << std::dec << bkp.pid << std::endl;
        return user_sample_hbp;
    }
    void unresgister_bkp(void *user_sample_hbp)
    {
    }
    bool is_unvalid(uint64_t addr)
    {
        if (addr == 0 ||*(int*)this->kread.ret!=0||addr>=0x00007fffffffffff)
        {
            // printf("读取失败 ret %d\n",*(int*)this->kread.ret);
            return 1;
        }
        return 0;
    }
    bool is_unvalid()
    {
        if (*(int*)this->kread.ret!=0)
        {
            // printf("读取失败 ret %d\n",*(int*)this->kread.ret);
            return 1;
        }
        return 0;
    }
};

kernel *driver = new kernel();

typedef char PACKAGENAME; // 包名

void run_as_root(const char *cmd)
{
    int pipefd[2]; // 管道描述符：[0]读端, [1]写端
    pid_t pid;
    char buffer[1024];
    ssize_t bytes_read;

    // 1. 创建管道
    if (pipe(pipefd) == -1)
    {
        perror("pipe failed");
        exit(EXIT_FAILURE);
    }

    // 2. 创建子进程
    pid = fork();
    if (pid < 0)
    {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }
    else if (pid == 0)
    {                     // 子进程
        close(pipefd[0]); // 关闭读端

        // 重定向标准输出到管道写端
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]); // 关闭原写端

        // 执行su命令（输出被重定向到管道）
        execlp("su", "su", "-c", cmd, NULL);

        // 若执行失败
        perror("execlp failed");
        _exit(EXIT_FAILURE);
    }
    else
    {                          // 父进程
        close(pipefd[1]);      // 关闭写端
        waitpid(pid, NULL, 0); // 等待子进程结束

        // 3. 从管道读取输出
        printf("Android ID:");
        while ((bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0)
        {
            buffer[bytes_read] = '\0'; // 确保字符串终止
            printf("%s", buffer);
        }
        close(pipefd[0]); // 关闭读端
    }
}



#endif /* _KERNEL_H_ */
