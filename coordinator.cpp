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

vector<SOCKET> sock_list;
vector<struct sockaddr_in> addr_list;
vector<int> core_list;
int total_core_num = 0;
int accept_flag = 1;
pthread_mutex_t sock_lock;

// string p0 = "./CS_mat_try1/";
string p0 = "./";
string vecpath = p0+"rndvec";
string prefix = p0+"data_mtx/";

// string data_name = "GAP-road.mtx";
string data_name = "usps_norm_5NN.mtx";
// string data_name = "bcspwr05.mtx";

string Path = prefix+data_name;

//23947347 23947347 28854312

//listen socket
SOCKET socket_listen;

string CheckIP()
{
	WSADATA wsaData;
	char name[255];
	char* ip;
	PHOSTENT hostinfo;
	string ipStr;
    
    if(WSAStartup(MAKEWORD(2,0),&wsaData) == 0){
        if(gethostname(name, sizeof(name)) == 0){
            if((hostinfo = gethostbyname(name)) != NULL){
                ip = inet_ntoa(*(struct in_addr*)*hostinfo->h_addr_list);
                ipStr = ip;
            }
        }
        WSACleanup();
    }
    return ipStr;
}

void *coordinator_listen(void *args);

int main(){
    //获取本机ip
    string MyIP = CheckIP();
    printf("my IP is %s\n",MyIP.c_str());
    pthread_mutex_init(&sock_lock,NULL);
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(1,1), &wsa_data)) {
        return -1;
    }
    //创建listen socket
    socket_listen = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_listen == INVALID_SOCKET) {
        WSACleanup();
        printf("invaild socket.\n");
        return -1;
    }else{
        printf("done.\n");
    }

    //绑定到ip地址
    SOCKADDR_IN srvAddr;
    memset(&srvAddr, 0, sizeof(SOCKADDR));
    srvAddr.sin_family = AF_INET;
    srvAddr.sin_addr.s_addr = inet_addr(MyIP.c_str());
    srvAddr.sin_port = htons(6010);
    if(bind(socket_listen, (struct sockaddr*)&srvAddr, sizeof(srvAddr)) < 0){
        int err = WSAGetLastError();
        printf("bind err:%d\n",err);
        closesocket(socket_listen);
        WSACleanup();
        system("pause");
        exit(0); 
    }
    //开始listen
    if(listen(socket_listen,SOMAXCONN) == SOCKET_ERROR){
        int err = WSAGetLastError();
        printf("listen err:%d\n",err);
        closesocket(socket_listen);
        WSACleanup();
        system("pause");
        exit(0);
    }
    //创建等待连接的线程
    pthread_t listen_thread;
    pthread_create(&listen_thread,NULL,coordinator_listen,NULL);
    string tmp;
    while(cin >> tmp){
        if(tmp == "1"){
            break;
        }
    }
    //发送读取请求
    pthread_mutex_lock(&sock_lock);
    accept_flag = 0; //不再接受新参与者
    pthread_mutex_unlock(&sock_lock);
    for(int i=0;i<sock_list.size();i++){
        int n = sendto(sock_list[i],data_name.c_str(),data_name.size(),0,(struct sockaddr*)&addr_list[i],sizeof(addr_list[i]));
        if(n == -1){
            printf("send err:%d\n",WSAGetLastError());
        }
    }
    auto mat = loading_matrix_multi_thread(Path); //读取矩阵文件
    auto vec = loadvec(mat.N,vecpath); //读取向量文件
    //分配任务
    int local_core = thread::hardware_concurrency();
    total_core_num += local_core;
    cout<<"total core:"<<total_core_num<<",start.\n";
    int M = mat.M,cut_len,cut_rest,from=0,to,i;
    cut_len = M/total_core_num;cut_rest = M%total_core_num;
    vector<int> start_index(sock_list.size(),0);
    vector<int> recv_len(sock_list.size(),0);
    for(i=0;i<sock_list.size();i++){
        to = from + core_list[i]*cut_len;
        if(cut_rest>0){
            to += core_list[i] > cut_rest ? cut_rest:core_list[i];
        }
        cut_rest -= core_list[i];
        start_index[i] = from;
        recv_len[i] = to-from;
        int buf[2] = {from,to};
        int n = sendto(sock_list[i],(char*)buf,sizeof(buf),0,(struct sockaddr*)&addr_list[i],sizeof(addr_list[i]));
        if(n == -1){
            printf("send err:%d\n",WSAGetLastError());
        }
        from = to;
    }
    vector<double> res_multi(mat.M,0);
    vector<double> tmp_res = calculation_multi_core(mat,vec,from,mat.M);
    memcpy(&res_multi[from],&tmp_res[0],(mat.M-from)*sizeof(double));
    for(i=0;i<sock_list.size();i++){
        //反复拷贝,直到拷贝完成
        char *ptr = (char*)&res_multi[start_index[i]];
        int rest_len = recv_len[i]*sizeof(double);
        RECV_FLAG:
        int n = recv(sock_list[i],ptr,rest_len,0);
        if(n == -1){
            printf("recv err:%d\n",WSAGetLastError());
        }else{
            printf("len:%d,%d\n",n,rest_len);
            if(n != rest_len){
                rest_len -= n;
                ptr += n;
                goto RECV_FLAG;
            }else{
                cout<<"socket "<<i<<" recv all.\n";
            }
        }
    }
    //本地单线程计算，校验准确性
    vector<double> res_single = calculation(mat,vec);
    //验算结果
    int flag=1;
    for(i=0;i<res_single.size();i++){
        if(abs(res_single[i]-res_multi[i])>1e-6){
            cout<<i<<" wrong,"<<res_single[i]<<","<<res_multi[i]<<"\n";
            flag=0;
            break;
        }
    }
    if(flag) cout<<"correct.\n";

    closesocket(socket_listen);
    WSACleanup();
    pthread_mutex_destroy(&sock_lock);
    return 0;
}

void *coordinator_listen(void *args){
    cout<<"listening.\n";
    //开始等待连接
    while(1){
        struct sockaddr_in clientAddr;
        int length = sizeof(clientAddr);
        SOCKET accept_fd = accept(socket_listen, (struct sockaddr*)&clientAddr, &length);
        if (accept_fd == INVALID_SOCKET) {
            if(accept_flag == 0) break;
            printf("socket error.\n");
        }
        pthread_mutex_lock(&sock_lock);
        if(accept_flag){
            int core_num = 0;
            //数据报形式接收core_num
            int n = recvfrom(accept_fd,(char*)&core_num,sizeof(core_num),0,(struct sockaddr*)&clientAddr,&length);
            if(n == -1){
                printf("can not recv core_num,err:%d\n",WSAGetLastError());
            }
            core_list.push_back(core_num);
            sock_list.push_back(accept_fd);
            addr_list.push_back(clientAddr);
            total_core_num += core_num;
            cout<<"participant accepted. current core num:"<<total_core_num<<".\n";
        }else{
            pthread_mutex_unlock(&sock_lock);
            break;
        }
        pthread_mutex_unlock(&sock_lock);
    }
    return NULL;
}