#pragma once
#include<iostream>
#include<fstream>
#include<sstream>
#include<vector>
#include <ctime>
#include <random>
#include <chrono>

#include<process.h>
#include<windows.h>
#include<thread>
#include<pthread.h>

#include <sys/stat.h>
using namespace std;
using namespace chrono;
typedef std::chrono::_V2::system_clock::time_point TIME;
struct Next{
    int V;
    double val;
};

struct SparseMat{
    int M,N;
    vector<vector<Next> > data;
    SparseMat(int m_,int n_):M(m_),N(n_){
        data.resize(M);
    }
};

int read_int(char *_data,int &index);
double read_double(char *_data,int &index);

SparseMat loading_matrix(string Path){
    //这是一个存储文件(夹)信息的结构体，其中有文件大小和创建时间、访问时间、修改时间等
    TIME start = system_clock::now();
    struct stat statbuf;

	// 提供文件名字符串，获得文件属性结构体
	stat(Path.c_str(), &statbuf);
	
	// 获取文件大小
	size_t filesize = statbuf.st_size;
    char *_data = new char[filesize+1]{};

    //用fread快速读取
    FILE *fp = fopen(Path.c_str(),"rb");
    size_t filelen = fread(_data,sizeof(char),filesize+1,fp);
    fclose(fp);

    vector<long> index_arr; //遍历，找到所有换行符
    for(int i=0;i<filelen;i++){
        if(_data[i] == '\n') index_arr.push_back(i);
    }
    cout<<index_arr.size()<<endl;

    int M,N,i,j,len_of_file,cnt=0,tmp;
    long index=0;
    double val;
    char *ptr = _data;
    while(ptr[1]=='%'){
         ptr = _data+index_arr[index];
         index++;
    }
    //读入元信息 M,N,len_of_file
    sscanf(ptr,"%d%d%d",&M,&N,&len_of_file);
    cout<<M<<" "<<N<<" "<<len_of_file<<'\n';
    //开始读入稀疏矩阵信息，每两个换行符之间用sscanf
    SparseMat mat(M,N);
    while (cnt!=len_of_file){
        ptr = _data+index_arr[index];
        index++;
        // sscanf(ptr,"%d%d%lf",&i,&j,&val); //1
        //2
        tmp = 1;
        i = read_int(ptr,tmp);
        while(ptr[tmp]==' ') tmp++;
        j = read_int(ptr,tmp);
        while(ptr[tmp]==' ') tmp++;
        if(ptr[tmp] != '\n') val = read_double(ptr,tmp);
        else val = 1;
        mat.data[i-1].push_back(Next{j-1,val});
        mat.data[j-1].push_back(Next{i-1,val});
        cnt++;
        if(!(cnt%2000000)){
            cout<<cnt<<" passed.\n";
        }
    }
    TIME end = system_clock::now();
    auto duration = duration_cast<microseconds>(end - start);

    cout<<"done, spend "
        <<double(duration.count()) * microseconds::period::num / microseconds::period::den
        <<" second.\n";
    delete [] _data;
    return mat;
}

vector<double> loadvec(int len,string Path){
    vector<double> res(len,0);
    FILE *fd = fopen(Path.c_str(),"rb");
    size_t s = fread(&res[0],sizeof(double),len,fd);
    fclose(fd);
    return res;
}

pthread_mutex_t * mutex_; //锁，防止竞争写入
int mblk = 100; //mat中每100列数据用一个锁管理，节省空间

struct IO_args{ //用于给IO线程传递参数的数据结构
    SparseMat &mat; //要构造的稀疏矩阵
    char * _data; //字符串数据
    vector<long> &index_arr; //换行符的下标数组
    int begin,end; //任务分段的下标
    IO_args(SparseMat &a,char *b,vector<long> &c):mat(a),_data(b),index_arr(c){}
};

unsigned int __stdcall loading_matrix_single_thread(void *);

SparseMat loading_matrix_multi_thread(string Path,int thread_num=8){
    //这是一个存储文件(夹)信息的结构体，其中有文件大小和创建时间、访问时间、修改时间等
    thread_num = thread::hardware_concurrency();
    TIME start = system_clock::now();
    struct stat statbuf;

	// 提供文件名字符串，获得文件属性结构体
	stat(Path.c_str(), &statbuf);
	
	// 获取文件大小
	size_t filesize = statbuf.st_size;
    char *_data = new char[filesize+1]{};

    //用fread快速读取
    FILE *fp = fopen(Path.c_str(),"rb");
    size_t filelen = fread(_data,sizeof(char),filesize+1,fp);
    fclose(fp);

    vector<long> index_arr; //遍历，找到所有换行符
    for(int i=0;i<filelen;i++){
        if(_data[i] == '\n') index_arr.push_back(i);
    }
    cout<<index_arr.size()<<endl;

    int M,N,i,j,len_of_file,cnt=0;
    long index=0;
    double val;
    char *ptr = _data;
    while(ptr[1]=='%'){
         ptr = _data+index_arr[index];
         index++;
    }
    //读入元信息 M,N,len_of_file
    sscanf(ptr,"%d%d%d",&M,&N,&len_of_file);
    cout<<M<<" "<<N<<" "<<len_of_file<<'\n';
    //开始读入稀疏矩阵信息
    SparseMat mat(M,N);
    //创建锁
    int mutex_len = M/mblk+1,mission_len=len_of_file;
    mutex_ = new pthread_mutex_t[mutex_len];
    for(i=0;i<mutex_len;i++) pthread_mutex_init(&mutex_[i],NULL);
    //分配任务
    vector<IO_args> arg_(thread_num,IO_args(mat,_data,index_arr));
    int cut_len=mission_len/thread_num,cut_rest=mission_len%thread_num,from=index,to;
    HANDLE t_a[thread_num];
    for(i=0;i<thread_num;i++){
        to = from+cut_len+(cut_rest>0);
        arg_[i].begin = from;arg_[i].end = to;
        t_a[i] = (HANDLE)_beginthreadex(NULL, 256,loading_matrix_single_thread,(void*)&arg_[i], CREATE_SUSPENDED, NULL);
        // pthread_create(&t_a[i],NULL,loading_matrix_single_thread,(void*)&arg_[i]);
        DWORD_PTR mask=SetThreadAffinityMask(t_a[i],1<<i);
        if(mask==0)
            printf("%d,ERROR\n",i);
        from = to;
        cut_rest --;
    }
    for(int i=0;i<thread_num;i++){
        ResumeThread(t_a[i]);
    }
    
    WaitForMultipleObjects(thread_num, t_a, TRUE, INFINITE);

    for(i=0;i<mutex_len;i++) pthread_mutex_destroy(&mutex_[i]);
    delete[] mutex_;

    TIME end = system_clock::now();
    auto duration = duration_cast<microseconds>(end - start);

    cout<<"done, spend "
        <<double(duration.count()) * microseconds::period::num / microseconds::period::den
        <<" second.\n";
    delete [] _data;
    return mat;
}

unsigned int __stdcall loading_matrix_single_thread(void *IOarg){
    //解包参数
    IO_args * arg_ = static_cast<IO_args*>(IOarg);
    SparseMat &mat = arg_->mat;
    char * _data = arg_->_data,*ptr;
    vector<long> &index_arr = arg_->index_arr;
    int begin = arg_->begin,end = arg_->end,i,j,mi,mj,tmp;
    double val;
    //开始计算
    for(int index=begin;index<end;index++){
        ptr = _data+index_arr[index];
        // sscanf(ptr,"%d%d%lf",&i,&j,&val); //1
        //2
        tmp = 1;
        i = read_int(ptr,tmp);
        while(ptr[tmp]==' ') tmp++;
        j = read_int(ptr,tmp);
        while(ptr[tmp]==' ') tmp++;
        if(ptr[tmp] != '\n') val = read_double(ptr,tmp);
        else val = 1;
        mi = (i-1)/mblk;mj=(j-1)/mblk;
        pthread_mutex_lock(&mutex_[mi]);
        mat.data[i-1].push_back(Next{j-1,val});
        pthread_mutex_unlock(&mutex_[mi]);
        pthread_mutex_lock(&mutex_[mj]);
        mat.data[j-1].push_back(Next{i-1,val});
        pthread_mutex_unlock(&mutex_[mj]);
        if(!((index-begin)%2500000)) cout<<index<<" to "<<end<<", "<<(index-begin)<<" passed.\n";
    }
    return 0;
}

int read_int(char *_data,int &index){
    int ret=0;
    while(_data[index]>='0' && _data[index]<='9'){
        ret = _data[index]-'0'+10*ret;
        index++;
    }
    return ret;
}

//while(_data[index]>='0' && _data[index]<='9') index++;

double read_double(char *_data,int &index){
    double ret=0;
    double frac = 1;
    if(_data[index] !='.') ret = (double)read_int(_data,index);
    index++; //跳过浮点
    while(_data[index]>='0' && _data[index]<='9'){
        frac /=10;
        ret += (_data[index]-'0')*frac;
        index++;
    }
    return ret;
}

