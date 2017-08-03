#ifndef HYSOCKET_H
#define HYSOCKET_H

#ifndef __WIN32
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <netdb.h>
#else
# include <winsock2.h>
# include <io.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <errno.h>
#include <pthread.h>
#include <map>


#define HY_KEY_DEAD 0x1016dead
#define HY_KEY_BEEF 0x1016beef
#define HY_KEY_FULL 0x1016fcc1

#define HY_KEY_PUT_FILE_MODE 0x1016cf01
#define HY_KEY_FREE_CMD_MODE 0x1016cd01
#define HY_KEY_FREE_CMD_LOCAL_INFO_MODE 0x1016cd0a
#define HY_KEY_CLIENT_DISCONNECT 0x1016bacc

#define HY_KEY_GET_FILE_MODE 0x1016cf02

#define HY_KEY_NONE 0x1016ff00

class HySocketBase
{
protected:
    int sfd;
    void init(struct sockaddr_in * psaddr, std::string ip, unsigned int port)
    {
        memset(psaddr, 0, sizeof(*psaddr));
        psaddr->sin_family = AF_INET;
        psaddr->sin_port = htons(port);
        psaddr->sin_addr.s_addr = inet_addr(ip.c_str());

        if (ip == "0.0.0.0")
        {
            psaddr->sin_addr.s_addr = INADDR_ANY;
        }
    }

public:
    HySocketBase(void):
        sfd(0)
    {
    }

    int getFd(void)
    {
        return sfd;
    }

#ifdef __WIN32
    static WSADATA s_was;
#endif

    static void initalize(void)
    {
#ifdef __WIN32
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2,2), &wsa) != 0)
        {
            printf("wsa error\n");
            exit(EXIT_FAILURE);
        }
#endif

    }


    static void cleanup(void)
    {
#ifdef __WIN32
        WSACleanup();
#endif
    }

    void attachSocket(int fd)
    {
        sfd = fd;
    }

    int send(const void *buf, int size)
    {
        int ret = ::send(sfd, (const char*)buf, size, 0);
        return ret;
    }

    int send_all(const void *buf, int size)
    {
        int rem = size;
        const char *cur_buf = (const char *)buf;
        int r = 0;
        while (rem > 0)
        {
            r = this->send(cur_buf, rem);
            rem -= r;
            cur_buf += r;

        }
        return rem;
    }

    int recv(void *buf, int size)
    {
        int ret = ::recv(sfd, (char*)buf, size, 0);
        return ret;

    }

    int recv_all(void *buf, int size)
    {

        fd_set fds;
        int rem = size;
        int ret = 0;
        char *pbuf = (char*)buf;

        int times = 0;
        while (rem > 0)
        {
            FD_ZERO(&fds);
            FD_SET(sfd, &fds);

            struct timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = 5000;

            errno = 0;
            int sel = select(sfd+1, &fds, NULL, NULL, &timeout);
            if (sel < 0)
            {
                perror("[recv]select fail");
                return -1;
            }

            if (sel == 0 || (FD_ISSET(sfd, &fds) == 0))
            {
                continue;
            }

            int r = ::recv(sfd, pbuf, rem, 0);
            if (r > 0)
            {
                rem -= r;
                ret += r;
                pbuf += r;
                times = 0;
            }
            else
            {
                ++times;
            }

            //printf("r = %d ts = %d\n", r, times);
            if (times > 400)
            {
                fprintf(stderr, "[recv] timeout\n");
                break;
            }

        }

        return ret;
    }

    void close(void)
    {
        shutdown(sfd, 2);
#ifdef __WIN32
        closesocket(sfd);
#else
        ::close(sfd);
#endif
    }

    virtual ~HySocketBase(void)
    {

    }


    static int ShowHostInfo(void)
    {
        char ac[80];
        if (gethostname(ac, sizeof(ac)) < 0) {
            fprintf(stderr, "get host name fail\n");
            return 1;
        }

        std::cout << "Host name is " << ac << "." << std::endl;

        struct hostent *phe = gethostbyname(ac);
        if (phe == 0) {
            std::cerr << "Yow! Bad host lookup." << std::endl;
            return 1;
        }

        for (int i = 0; phe->h_addr_list[i] != 0; ++i) {
            struct in_addr addr;
            memcpy(&addr, phe->h_addr_list[i], sizeof(struct in_addr));
            std::cout << "Address " << i << ": " << inet_ntoa(addr) << std::endl;
        }

        return 0;
    }


};

class HySocketClient: public HySocketBase
{
    struct sockaddr_in m_sockaddr;
    struct sockaddr_in m_sockaddr_dest;

public:
    HySocketClient(void):
        m_sockaddr(),
        m_sockaddr_dest()
    {
        memset(&m_sockaddr, 0, sizeof(m_sockaddr));
        memset(&m_sockaddr_dest, 0, sizeof(m_sockaddr_dest));

    }



    void attachSocketAddr(const struct sockaddr_in & saddr)
    {
        m_sockaddr = saddr;
    }

    int connect(std::string ip, unsigned int port)
    {
        init(&m_sockaddr_dest, ip, port);
        sfd = socket(AF_INET, SOCK_STREAM, 0);

        if (sfd <= 0)
        {
            fprintf(stderr, "create socket fail, fd = %d\n", sfd);
        }

        int ret = 
            ::connect(sfd, 
                    (struct sockaddr*)&m_sockaddr_dest, 
                    sizeof(m_sockaddr_dest));

        if (ret < 0)
        {
            fprintf(stderr, "connect fail, ret = %d\n", ret);
            perror("[strerr]");
            exit(EXIT_FAILURE);
        }
        return ret;
    }

    std::string getIPv4(void)
    {
        std::string ret;
        const struct sockaddr_in * paddr = (const struct sockaddr_in*)&m_sockaddr;
        ret = std::string(inet_ntoa(paddr->sin_addr));
        return ret;
    }

};

class HySocketServer: public HySocketBase
{
    struct sockaddr_in m_sockaddr;
public:

        int setup(std::string ip, unsigned int port)
        {
            init(&m_sockaddr, ip, port);
            sfd = socket(AF_INET, SOCK_STREAM, 0);

            if (sfd <= 0)
            {
                fprintf(stderr, "create socket fail, fd = %d\n", sfd);
            }

            setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, 
                    (const char*)&m_sockaddr, sizeof(m_sockaddr));


            int ret = 0;
            ret = bind(sfd, (struct sockaddr*)&m_sockaddr, sizeof(m_sockaddr));
            if (ret < 0)
            {
                perror("bind ");
                exit(EXIT_FAILURE);
            }

            ret = listen(sfd, 1);
            if (ret < 0)
            {
                perror("listen ");
                exit(EXIT_FAILURE);

            }

            return 0;
        }

        HySocketClient *  accept(void)
        {
            struct sockaddr_in caddr;

#ifdef __WIN32
            int addrlen = sizeof(caddr);
#else
            socklen_t addrlen = sizeof(caddr);
#endif


            printf("wait accept...\n");
            int cfd = ::accept(sfd, (struct sockaddr*)&caddr, &addrlen);
            printf("[Server] accept fd = %d\n", cfd);

            if (sfd <= 0)
            {
                perror("[Server] accpt error :");
                return NULL;

            }

            HySocketClient * sock_client = new HySocketClient;
            sock_client->attachSocket(cfd);
            sock_client->attachSocketAddr(caddr);
            return sock_client;

        }

        int isReadable(int sec, int usec)
        {
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(sfd, &rfds);
            struct timeval timeout;
            timeout.tv_usec = 0;
            timeout.tv_sec = 1;

            int sel = select(sfd+1, &rfds, NULL, NULL,&timeout);

            if (sel > 0 && FD_ISSET(sfd, &rfds))
            {
                printf("[Server] select accept\n");
                return 1;
            }

            if (sel < 0)
            {
                perror("[Server]select :");
                exit(EXIT_FAILURE);
            }

            return 0;
        }
};

inline void WriteBuffToFile(const std::string & file_name, const char * buf, int size)
{

    using namespace std;
    errno = 0;
    FILE * fp = fopen(file_name.c_str(), "wb");

    if (!fp)
    {
        fprintf(stderr, "open file err\n");
        perror("[errno]");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < size; ++i)
    {
        fwrite(buf++, 1, 1, fp);
    }

    fclose(fp);
}

inline char * Vector2Buf(const std::vector<char> &v)
{
    char  * sendbuf = new char[v.size() + 1];
    char *pbuf = sendbuf;
    for (size_t i = 0 ; i < v.size(); ++i)
    {
        *pbuf++ = v[i];
    }
    *pbuf = 0;

    return sendbuf;

}

inline int GetFileBufferVector(std::vector<char> &file_buf, const std::string & file_name)
{
    errno = 0;
    FILE * fp = fopen(file_name.c_str(), "rb");

    if (!fp)
    {
        perror("[file-copy-mode]");
        return -1;
        exit(EXIT_FAILURE);
    }

    char ch = 0;
    while (fread(&ch, 1, 1, fp) > 0)
    {
        file_buf.push_back(ch);
    }

    fclose(fp);
    return 0;

}
#endif /* end of include guard: HYSOCKET_H */