#include <sys/resource.h>

#include <QApplication>
#include <QSslConfiguration>
#include <QTranslator>

#include "selfdrive/hardware/hw.h"
#include "selfdrive/ui/qt/qt_window.h"
#include "selfdrive/ui/qt/util.h"
#include "selfdrive/ui/qt/window.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <string.h>
#include <thread>
#include <chrono>
#include "selfdrive/common/params.h"
#include <iostream>
#include <netinet/in.h>
#include "selfdrive/ui/globalmsgcom.h"

#include <stdexcept>
#include <string>

#include <ifaddrs.h>
#include <net/if.h>

/*
class UDPSocket {
    public:
        UDPSocket(const char* ip, uint16_t port) {
            fd = ::socket(AF_INET, SOCK_DGRAM, 0);
            if (fd < 0) throw std::runtime_error("socket() failed");
    
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            if (::inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
                ::close(fd);
                throw std::runtime_error("inet_pton() failed");
            }
        }
    
        ~UDPSocket() {
            if (fd >= 0) ::close(fd);
        }
    
        void sendTo(const void* data, size_t len) {
            ssize_t n = ::sendto(fd, data, len, 0,
                                 reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
            if (n < 0) throw std::runtime_error("sendto() failed");
        }
    
        void sendTo(const std::string &data) {
            sendTo(data.data(), data.size());
        }
    
    private:
        int fd{-1};
        sockaddr_in addr{};
};
*/
class UDPSocket {
public:
    UDPSocket() {
        fd = socket(AF_INET, SOCK_DGRAM, 0);
        if(fd < 0)
            throw std::runtime_error("socket");
        int yes = 1;
        setsockopt(fd,
                   SOL_SOCKET,
                   SO_BROADCAST,
                   &yes,
                   sizeof(yes));
    }
    ~UDPSocket() {
        if(fd >= 0)
            close(fd);
    }
    bool bindPort(uint16_t port) {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;
        return bind(fd,
                    (sockaddr*)&addr,
                    sizeof(addr)) == 0;
    }
    bool sendTo(const std::string &ip,
                uint16_t port,
                const void *data,
                size_t len) {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if(inet_pton(AF_INET,
                     ip.c_str(),
                     &addr.sin_addr) <= 0)
            return false;
        return sendto(fd,
                      data,
                      len,
                      0,
                      (sockaddr*)&addr,
                      sizeof(addr)) >= 0;
    }
    bool recvFrom(std::string &ip,
                  uint16_t &port,
                  char *buf,
                  int timeoutMs) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd,&fds);
        timeval tv{};
        tv.tv_sec = timeoutMs / 1000;
        tv.tv_usec = (timeoutMs % 1000) * 1000;
        if(select(fd+1,
                  &fds,
                  nullptr,
                  nullptr,
                  &tv) <= 0)
            return false;
        sockaddr_in from{};
        socklen_t len = sizeof(from);
        int n = recvfrom(fd,
                         buf,
                         256,
                         0,
                         (sockaddr*)&from,
                         &len);
        if(n <= 0)
            return false;
        buf[n] = 0;
        char ipbuf[32];
        inet_ntop(AF_INET,
                  &from.sin_addr,
                  ipbuf,
                  sizeof(ipbuf));
        ip = ipbuf;
        port = ntohs(from.sin_port);
        return true;
    }
private:
    int fd{-1};
};
std::string getLocalIPAddress()
{
    struct ifaddrs *ifaddr;
    if(getifaddrs(&ifaddr) == -1)
        return "";
    std::string result;
    for(struct ifaddrs *ifa = ifaddr;
        ifa != nullptr;
        ifa = ifa->ifa_next)
    {
        if(ifa->ifa_addr == nullptr)
            continue;
        if(ifa->ifa_addr->sa_family != AF_INET)
            continue;
        if(ifa->ifa_flags & IFF_LOOPBACK)
            continue;
        if(strncmp(ifa->ifa_name,"wlan",4) != 0 &&
           strncmp(ifa->ifa_name,"eth",3) != 0)
            continue;
        char ip[INET_ADDRSTRLEN];
        sockaddr_in *sa =
            reinterpret_cast<sockaddr_in*>(ifa->ifa_addr);
        inet_ntop(AF_INET,
                  &sa->sin_addr,
                  ip,
                  sizeof(ip));
        result = ip;
        break;
    }
    freeifaddrs(ifaddr);
    return result;
}
std::string discoverPadIP()
{
    std::string localIP = getLocalIPAddress();
    if(localIP.empty())
    {
        std::cout << "Local IP not found" << std::endl;
        return "";
    }
    std::cout << "OpenPilot IP : "
              << localIP
              << std::endl;
    size_t pos = localIP.rfind('.');
    if(pos == std::string::npos)
        return "";
    std::string subnet = localIP.substr(0,pos+1);
    UDPSocket sock;
    if(!sock.bindPort(8081))
    {
        std::cout << "bind failed" << std::endl;
        return "";
    }
    const char *msg = "DISCOVER_PAD";
    for(int i=1;i<=254;i++)
    {
        std::string target = subnet + std::to_string(i);
        if(target == localIP)
            continue;
        sock.sendTo(target,
                    8080,
                    msg,
                    strlen(msg));
    }
    char buf[256];
    std::string ip;
    uint16_t port;
    auto start = std::chrono::steady_clock::now();
    while(true)
    {
        if(sock.recvFrom(ip,
                         port,
                         buf,
                         100))
        {
            if(strcmp(buf,"PAD_HERE") == 0 &&
               port == 8080)
            {
                std::cout << "Pad found : "
                          << ip
                          << std::endl;
                return ip;
            }
        }
        auto now = std::chrono::steady_clock::now();
        auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                now - start).count();
        if(elapsed > 3000)
            break;
    }
    std::cout << "Pad not found" << std::endl;
    return "";
}
void start_udp_loop()
{
    std::thread([]() {
        Params params;
        std::string port_str = params.get("ExternalPadPort");
        if(port_str.empty())
            port_str = "8080";
        uint16_t port = static_cast<uint16_t>(std::stoi(port_str));
        std::string ip;
        while(ip.empty())
        {
            ip = discoverPadIP();
            if(ip.empty())
            {
                std::cout << "Retry pad search..." << std::endl;
                std::this_thread::sleep_for(
                    std::chrono::seconds(5));
            }
        }
        try
        {
            UDPSocket sock;
            std::cout << "UDP target: "
                      << ip
                      << ":"
                      << port
                      << std::endl;
            auto lastSent = std::chrono::steady_clock::now();
            while(true)
            {
                auto now = std::chrono::steady_clock::now();
                auto elapsed =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - lastSent).count();
                if(elapsed >= 100)
                {
                    sock.sendTo(ip,
                                port,
                                msgcom,
                                sizeof(msgcom));
                    sock.sendTo(ip,
                                port,
                                "\n",
                                1);
                    lastSent = now;
                }
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(50));
            }
        }
        catch(const std::exception &e)
        {
            std::cerr << "UDP error: "
                      << e.what()
                      << std::endl;
        }
    }).detach();
}

/*
void start_udp_loop() {
    std::thread([]() {
        Params params;
        std::string ip = params.get("ExternalPadIP");
        std::string port_str = params.get("ExternalPadPort");
        if (port_str.empty()) port_str = "8080";   
        uint16_t port = static_cast<uint16_t>(std::stoi(port_str)); 
        // 기본값 설정

        // 안올패드ip서칭
        std::string ip = discoverPadIP();


        if (ip.empty()) ip = "192.168.41.127";
        
        

        try {
            UDPSocket sock(ip.c_str(), port);
            std::cout << "UDP target: " << ip << ":" << port << std::endl;
            auto lastSent = std::chrono::steady_clock::now();

            
            
            while (true) {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSent).count();
            
                if (elapsed >= 100) {
                    sock.sendTo(msgcom, sizeof(msgcom));
                    sock.sendTo("\n", 1);
                    lastSent = now;
                }
            
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

        } catch (const std::exception &e) {
            std::cerr << "UDP error: " << e.what() << std::endl;
        }
    }).detach();
}
*/
/*
class TCPSocket {
  public:
      TCPSocket(const char* ip, uint16_t port) {
          fd = ::socket(AF_INET, SOCK_STREAM, 0);
          if (fd < 0) throw std::runtime_error("socket() failed");
  
          sockaddr_in addr{};
          addr.sin_family = AF_INET;
          addr.sin_port   = htons(port);
          if (::inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
              ::close(fd);
              throw std::runtime_error("inet_pton() failed");
          }
  
          if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
              ::close(fd);
              throw std::runtime_error(std::string("connect() failed: ") + std::strerror(errno));
          }
      }
  
      ~TCPSocket() {
          if (fd >= 0) ::close(fd);
      }
  
      // 바이너리 데이터 전송용
      void sendAll(const void* data, size_t len) {
          size_t total = 0;
          const char* ptr = reinterpret_cast<const char*>(data);
          while (total < len) {
              ssize_t n = ::send(fd, ptr + total, len - total, MSG_NOSIGNAL);
              if (n <= 0) throw std::runtime_error("send() failed");
              total += n;
          }
      }
  
      // 문자열 전송용 (internally uses binary send)
      void sendAll(const std::string &data) {
          sendAll(data.data(), data.size());
      }
  
  private:
      int fd{-1};
};
  
void start_tcp_loop() {
  std::thread([](){
          Params params;
          std::string ip = params.get("ExternalPadIP");
          std::string port_str = params.get("ExternalPadPort");
          // 기본값 설정
          if (ip.empty()) ip = "192.168.41.127";
          if (port_str.empty()) port_str = "8080";
          uint16_t port = static_cast<uint16_t>(std::stoi(port_str));
  
          while (true) {  // 재연결 로직
              try {
                  TCPSocket sock(ip.c_str(), port);
                  std::cout << "Connected to " << ip << ":" << port << std::endl;
  
                  while (true) {  // 소켓 연결 유지 중 데이터 전송
                      std::cout << "Sending data..." << std::endl;
  
                      // float 배열을 바이너리로 한 덩어리 전송
                      sock.sendAll(msgcom, sizeof(msgcom));
                      // 끝 표시로 "\n" 전송
                      sock.sendAll("\n", 1);
  
                      std::this_thread::sleep_for(std::chrono::milliseconds(1));
                  }
  
              } catch (const std::exception &e) {
                  std::cerr << "TCP loop error: " << e.what() << std::endl;
                  std::this_thread::sleep_for(std::chrono::seconds(1));
              }
          }
   }).detach();
}
*/



int main(int argc, char *argv[]) {
  setpriority(PRIO_PROCESS, 0, -20);

  qInstallMessageHandler(swagLogMessageHandler);
  initApp();

  if (Hardware::EON()) {
    QSslConfiguration ssl = QSslConfiguration::defaultConfiguration();
    ssl.setCaCertificates(QSslCertificate::fromPath("/usr/etc/tls/cert.pem"));
    QSslConfiguration::setDefaultConfiguration(ssl);
  }

  QTranslator translator;
  QString translation_file = QString::fromStdString(Params().get("LanguageSetting"));
  if (!translator.load(translation_file, "translations") && translation_file.length()) {
    qCritical() << "Failed to load translation file:" << translation_file;
  }

  QApplication a(argc, argv);
  a.installTranslator(&translator);

  //send_tcp_test();
  start_udp_loop();

  MainWindow w;
  setMainWindow(&w);
  a.installEventFilter(&w);
  return a.exec();
}
