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
  start_tcp_loop();

  MainWindow w;
  setMainWindow(&w);
  a.installEventFilter(&w);
  return a.exec();
}
