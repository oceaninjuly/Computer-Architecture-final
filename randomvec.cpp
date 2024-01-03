#include<iostream>
#include <random>
#include <chrono>
#include<windows.h>
using namespace std;
using namespace chrono;
typedef std::chrono::_V2::system_clock::time_point TIME;
int len = 20000;
// int len = 23947347;
string Path = "./CS_mat_try1/";


int main(){
    uniform_real_distribution<double> u(-1, 1); //生成随机落在(-1,1)范围内的浮点数
    default_random_engine e(time(NULL));
    double *arr = new double[len];
    int i;
    for(i=0;i<len;i++) arr[i] = u(e);
    FILE * fd = fopen((Path+"rndvec").c_str(),"wb");
    size_t s1 = fwrite(arr,sizeof(double),len,fd);
    fclose(fd);

    fd = fopen((Path+"rndvec").c_str(),"rb");

    vector<double> rarr_(len,0);

    size_t s2 = fread(&rarr_[0],sizeof(double),len,fd);
    cout<<s1<<" "<<s2<<'\n';
    for(i=0;i<len;i++){
        if(arr[i] != rarr_[i]){
            cout<<i<<" wrong, "<<arr[i]<<" "<<rarr_[i]<<"\n";break;
        }
    }
    if(i==len) cout<<"correct.\n";
    delete[] arr;

}