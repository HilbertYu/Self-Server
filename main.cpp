#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sstream>
#include "hysocket.h"
#include <string>
#include <vector>
#include <iostream>
#include <pthread.h>

using namespace std;

pthread_mutex_t g_setup_mux;

string getFileBuf(const char * file_name)
{
    FILE *p = fopen(file_name,"r");

    if(p == 0)
    {
        printf("open file error!");
        exit(0);
    }

    char ch = 0;
    string ret;
    while (fread(&ch, 1, 1, p) > 0)
    {
        ret.push_back(ch);
    //    printf("%c", ch);
    }

    fclose(p);

    return ret;


}


void * server_proc(void*)
{
    const char * file_name = "fake_data.txt";
    string buf = getFileBuf(file_name);

    int file_size = buf.size();
    printf("[Server] file name = %s\n",  file_name);
    printf("[Server] file size = %d\n",  file_size);


    vector<int> sp;
    sp.push_back(0);
    sp.push_back(36);
    sp.push_back(88);
    sp.push_back(199);
    sp.push_back(239);
    sp.push_back(380);
    sp.push_back(file_size);

    vector<string> str_pkg;
    for (int i = 0; i < sp.size() -1; ++i)
    {
        int s = sp[i];
        int e = sp[i+1] - s;

        str_pkg.push_back(buf.substr(s, e));
    }


    HySocketServer * s = new HySocketServer;
    s->setup("0.0.0.0", 30000);

    pthread_mutex_unlock(&g_setup_mux);
    HySocketClient * cl = s->accept();

    for (size_t i = 0; i< sp.size() - 1; ++i)
    {
        int pkg_size = str_pkg[i].size();
        const char * pkg_buf = str_pkg[i].c_str();
        int ret = cl->send(pkg_buf, pkg_size);
        pthread_mutex_lock(&g_setup_mux);

        printf("[Server] send = %d\n", ret);
    }
    cl->close();

    return NULL;
}


int main(int argc, const char * argv[])
{
    if (pthread_mutex_init(&g_setup_mux, NULL) < 0)
    {
        perror("");
        exit(-1);
    }

    pthread_mutex_lock(&g_setup_mux);

    pthread_t th_id;
    pthread_create(&th_id, NULL, server_proc, NULL);

    pthread_mutex_lock(&g_setup_mux);

    {
        HySocketClient * cl = new HySocketClient;
        cl->connect("127.0.0.1", 30000);

        while (1)
        {
            char buf[102400] = {0};

            int ret = cl->recv(buf, sizeof(buf));
            pthread_mutex_unlock(&g_setup_mux);

            if (ret <= 0)
                break;

            printf("ret = %d\n", ret);
            //printf("%s", buf);

        }

        return 0;

    }




    return 0;
}
