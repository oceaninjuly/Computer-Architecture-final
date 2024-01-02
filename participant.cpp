#include<WinSock2.h>
#include<pthread.h>
#include<vector>
#include<iostream>

#include<process.h>
#include<windows.h>
#include<thread>

#include"calculation.h"
#include"data_structure.h"

using namespace std;

// string p0 = "./CS_mat_try1/";
string p0 = "./";
string vecpath = p0+"rndvec";
string prefix = p0+"data_mtx/";

int main(){
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(1,1), &wsa_data)) {
        return -1;
    }
    SOCKET socket_client = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_client == INVALID_SOCKET) {
        WSACleanup();
        printf("socket_client invaild,-1\n");
        return -1;
    }
    string hostIP;
    cout<<"enter host ip:\n";
    cin >> hostIP;
    SOCKADDR_IN srvAddr;
    memset(&srvAddr, 0, sizeof(SOCKADDR));
    srvAddr.sin_family = AF_INET;
    srvAddr.sin_addr.s_addr = inet_addr(hostIP.c_str());
    srvAddr.sin_port = htons(6010);
    if(connect(socket_client, (struct sockaddr*)&srvAddr, sizeof(srvAddr)) < 0){ 
        int err = WSAGetLastError();
        printf("connect error:%d\n",err);
        exit(0); 
    }
    int core_num = thread::hardware_concurrency();
    int n = sendto(socket_client,(char*)&core_num,sizeof(core_num),0,(struct sockaddr*)&srvAddr,sizeof(srvAddr));
    if(n == -1){
        printf("can not recv core_num,err:%d\n",WSAGetLastError());
    }
    char filename[50];
    memset(filename,0,sizeof(filename));
    int addr_len = sizeof(srvAddr);
    n = recvfrom(socket_client,(char*)&filename,sizeof(filename),0,(struct sockaddr*)&srvAddr,&addr_len);
    if(n == -1){
        printf("can not recv core_num,err:%d\n",WSAGetLastError());
    }
    cout<<filename<<endl;
    //读取文件
    auto mat = loading_matrix_multi_thread(prefix+string(filename));
    auto vec = loadvec(mat.N,vecpath);
    int buf[2];
    memset(buf,0,sizeof(buf));
    n = recvfrom(socket_client,(char*)buf,sizeof(buf),0,(struct sockaddr*)&srvAddr,&addr_len);
    if(n == -1){
        printf("recv err:%d\n",WSAGetLastError());
    }
    //开始计算
    vector<double> res = calculation_multi_core(mat,vec,buf[0],buf[1]);
    n = send(socket_client,(char*)&res[0],res.size()*sizeof(double),0);
    if(n == -1){
        printf("send err:%d\n",WSAGetLastError());
    }
    closesocket(socket_client);
    WSACleanup();
    return 0;
}