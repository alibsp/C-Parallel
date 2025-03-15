#define _GNU_SOURCE
#include <stdio.h>
#include <math.h>
#include <pthread.h>
#include <iostream>
#include <fstream>
#include <thread>
#include <mutex>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

double waste_time(long n)
{

    double res = 0;
    long i = 0;
    while (i <n * 200000)
    {
        i++;
        res += sqrt(i);
    }
    return res;
}

void *thread_func(void *param)
{

    unsigned long mask = 1; /* processor 0 */

    fprintf(stderr, "start in core: %d\n", sched_getcpu()+1);

    /* bind process to processor 0 */
    if (pthread_setaffinity_np(pthread_self(), sizeof(mask), (cpu_set_t*)&mask) <0)
    {
        perror("pthread_setaffinity_np");
    }

    fprintf(stderr, "core: %d\n", sched_getcpu()+1);
    /* waste some time so the work is visible with "top" */
    fprintf(stderr, "result1: %f\n", waste_time(20000));

    mask = 2;   /* process switches to processor 1 now */
    if (pthread_setaffinity_np(pthread_self(), sizeof(mask),(cpu_set_t*)&mask) <0)
    {
        perror("pthread_setaffinity_np");
    }

    fprintf(stderr, "core: %d\n", sched_getcpu()+1);
    /* waste some more time to see the processor switch */
    fprintf(stderr, "result2: %f\n", waste_time(20000));
    return NULL;
}


void *thread_func2(void *param)
{
    int mainCore= *((int*)param);

    cpu_set_t cpu_set;
    pthread_getaffinity_np(pthread_self(), sizeof(cpu_set), &cpu_set);

    CPU_CLR(mainCore, &cpu_set);

    //unsigned long mask = 0; /* processor 0 */
    //unsigned long mask = 2; /* processor 1 */
    //unsigned long mask = 3; /* processor 0 or 1 (randomly) */
    //unsigned long mask = 4; /* processor 2 */


    if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set), &cpu_set) <0)
    {
        perror("pthread_setaffinity_np");
    }
    fprintf(stderr, "Main Core:%d, Thread start on core: %d Mask:%d \n", *((int*)param)+1, sched_getcpu()+1, (int)cpu_set.__bits[0]);

    /* waste some time so the work is visible with "top" */
    fprintf(stderr, "result1: %f\n", waste_time(20000));
    return NULL;
}


std::mutex fileMutex;  // mutex برای همگام‌سازی دسترسی به فایل


#define CHUNK_SIZE 1000000
void readFromFileInThread(const std::string& inputFilename, const std::string& outputFilename, uint64_t start, uint64_t count, unsigned long mask)
{
    //std::lock_guard<std::mutex> lock(fileMutex);  // قفل گیری بر روی mutex

    /* bind process to core */
    if (pthread_setaffinity_np(pthread_self(), sizeof(mask), (cpu_set_t*)&mask) <0)
    {
        perror("pthread_setaffinity_np");
    }
    fprintf(stderr, "start in core: %d\n", sched_getcpu());

    int inputFile = open(inputFilename.c_str(), O_RDONLY);
    remove(outputFilename.c_str());
    FILE* outputFile = fopen(outputFilename.c_str(), "w");  // باز کردن فایل برای نوشتن
    if (inputFile && outputFile)
    {
        // جابجایی به موقعیت شروع
        lseek(inputFile, start, SEEK_SET);

        // خواندن بخشی از فایل
        char buffer[CHUNK_SIZE+1]={0};
        uint64_t bytes=1;
        uint64_t i=0;
        uint64_t s=count/CHUNK_SIZE;
        uint64_t r=count%CHUNK_SIZE;
        for (; i < s && bytes>0; i++)
        {
            bytes=read(inputFile, buffer, CHUNK_SIZE);
            //if(bytes>0)
            //    fwrite(buffer, 1, CHUNK_SIZE, outputFile);
        }
        if(r)//بایقیمانده بایتهای خوانده نشده ی احتمالی که کمتر از اندازه چانک است
        {
            bytes=read(inputFile, buffer, r);
            //if(bytes>0)
            //    fwrite(buffer, 1, r, outputFile);
        }
        //fwrite("hello", 1, 5, outputFile);
        close(inputFile);
        fclose(outputFile);
    } else {
        std::cerr << "Unable to open the file: " << inputFilename << std::endl;
    }
}

int multipleRead()
{
    std::string filename  = "/home/aliakbaraledaghi/Programming/Delta/deltaPlugins/files/rand_agg_100M";
    std::string inputFilename = filename  +".txt";

    FILE* fp = fopen(inputFilename.c_str(), "r");
    fseek(fp, 0L, SEEK_END);
    uint64_t fileSize = ftell(fp);

    int threadCount=6; //تعداد ترد و کور مورد نیاز

    // تعیین موقعیت شروع و پایان برای هر ترد
    uint64_t sizeOfBytes = fileSize/threadCount;  //تعداد بایت برای هر ترد
    uint64_t mask=1;
    std::thread *threads[threadCount];
    for(int i=0; i<threadCount; i++)
    {
        uint64_t pos=sizeOfBytes*i;
        if(i==threadCount-1)
            pos += fileSize%threadCount; //محاسبه باقیمانده بایتها برای ترد آخر
        threads[i] = new std::thread(readFromFileInThread, inputFilename, filename+"_"+std::to_string(i)+".txt", pos, sizeOfBytes, mask);
        mask<<=1;
    }

    // انتظار برای پایان اجرای تردها
    for(int i=0; i<threadCount; i++)
    {
        threads[i]->join();
        delete threads[i];
    }
    return 0;
}

using namespace std::chrono;
uint64_t getMilliEpoch()
{
    return  duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

int main(int argc, char *argv[])
{
    signed short int a=10, b=0;
    b=!a;
    std::cout <<  b <<std::endl;
    return 0;
    uint64_t startTime=getMilliEpoch();
    multipleRead();
    uint64_t t2 = getMilliEpoch();
    std::cerr<<"Finish. time:"<<t2-startTime<<" ms"<<std::endl;
    pthread_exit(NULL);
    return 0;
}


int main2(int argc, char *argv[])
{

    unsigned long mask = 0; /* processor 0 */

    int mainCore=sched_getcpu();
    pthread_getaffinity_np(pthread_self(), sizeof(mask),(cpu_set_t*)&mask);
    fprintf(stderr, "Main thread core number: %d %lu \n", sched_getcpu()+1, mask);
    pthread_t my_thread;

    if (pthread_create(&my_thread, NULL, thread_func2, &mainCore) != 0)
    {
        perror("pthread_create");
    }

    fprintf(stderr, "result in main: %f\n", waste_time(30000));
    fprintf(stderr, "Finish Created threads: %d\n", sched_getcpu()+1);
    pthread_exit(NULL);
}
