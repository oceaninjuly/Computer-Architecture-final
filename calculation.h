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

#include<cmath>
#include"data_structure.h"
using namespace std;
using namespace chrono;

vector<double> *retvec =nullptr;

void show_time(TIME& start,TIME& end);

struct Myargs{
    SparseMat &mat;
    vector<double> &vec;
    int begin,end,offset;
    Myargs(SparseMat &a,vector<double> &b):mat(a),vec(b){}
};

vector<double> calculation(SparseMat& mat,vector<double>& vec){
    auto start = system_clock::now();
    int M=mat.M,N=mat.N,S,i,j,s,val;
    vector<double> res(N,0);
    for(i=0;i<M;i++){
        S = mat.data[i].size();
        for(j=0;j<S;j++){
            s = mat.data[i][j].V;val = mat.data[i][j].val;
            res[i] += val * vec[s];
        }
    }
    auto end = system_clock::now();
    cout<<"done.\n";
    show_time(start,end);
    return res;
}

unsigned int __stdcall calculation_single_core(void *p){
    Myargs *arg = static_cast<Myargs*>(p); 
    //解包参数
    SparseMat &mat = arg->mat;
    vector<double> &vec = arg->vec;
    int M=mat.M,N=mat.N,S,i,j,s,val,from=arg->begin,to=arg->end,offset=arg->offset;
    //开始计算
    vector<double> &res = *retvec;
    for(i=from;i<to;i++){
        S = mat.data[i].size();
        for(j=0;j<S;j++){
            s = mat.data[i][j].V;val = mat.data[i][j].val;
            res[i-offset] += val * vec[s];
        }
    }
    return 0;
}

vector<double> calculation_multi_core(SparseMat& mat,vector<double>& vec,int lower,int upper){
    auto start = system_clock::now();
    //获取核数量
    int core_num = thread::hardware_concurrency();
    if(retvec != nullptr) delete[] retvec;
    vector<double> vec_res(upper-lower,0);
    retvec = &vec_res;
    HANDLE t_a[core_num];
    vector<Myargs> arg_(core_num,Myargs(mat,vec));
    int M = upper-lower,cut_len,cut_rest,from=lower,to,i;
    cut_len=M/core_num; cut_rest=M%core_num;
    //计算每个线程负责的任务(长度均分)
    //依次创建线程
    for(i=0;i<core_num;i++){
        to = from+cut_len+(cut_rest>0);
        arg_[i].begin = from;arg_[i].end = to;arg_[i].offset = lower;
        t_a[i] = (HANDLE)_beginthreadex(NULL, 256,calculation_single_core,(void*)&arg_[i], CREATE_SUSPENDED, NULL);
        DWORD_PTR mask=SetThreadAffinityMask(t_a[i],1<<i);
        if(mask==0)
            printf("%d ERROR\n",i);
        from = to;
        cut_rest --;
    }
    for(int i=0;i<core_num;i++){
        ResumeThread(t_a[i]);
    }

    WaitForMultipleObjects(core_num, t_a, TRUE, INFINITE);
    auto end = system_clock::now();
    show_time(start,end);
    cout<<"done.\n";
    return vec_res;
}

void show_time(TIME& start,TIME& end){
    auto duration = duration_cast<microseconds>(end - start);
    cout << "spend " << double(duration.count()) * microseconds::period::num / microseconds::period::den << " sec." << endl;
}

// int main(){
//     TIME start,end;
//     // SparseMat mat = loading_matrix(Path);
//     SparseMat mat = loading_matrix_multi_thread(Path,8);
//     vector<double> vec = loadvec(mat.N,vecpath); //randvec1.txt记录了一段足够长的随机数组
//     cout<<"calculation, start computing:\n";
//     start = system_clock::now(); //计时
//     vector<double> res_single = calculation(mat,vec);
//     end = system_clock::now(); //停止计时并计算用时
//     cout<<"the cost of single thread:\n";
//     show_time(start,end);

//     cout<<"calculation_multi_core, start computing:\n";
//     start = system_clock::now(); //计时
//     vector<double>& res_multi = calculation_multi_core(mat,vec);
//     end   = system_clock::now(); //停止计时并计算用时
//     cout<<"the cost of multi core:\n";
//     show_time(start,end);

//     cout<<"the first value is "<<res_single[0]<<" and "<<res_multi[0]<<".\n";
//     //验算结果
//     int flag=1,i=0;
//     for(i=0;i<res_single.size();i++){
//         if(abs(res_single[i]-res_multi[i])>1e-6){
//             cout<<i<<" wrong.\n";
//             flag=0;
//         }
//     }
//     if(flag) cout<<"correct.\n";

//     if(retvec != nullptr){
//         delete retvec;
//         retvec = nullptr;
//     }
//     return 0;
// }