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

class TCPSocket {
  public:
    TCPSocket(const char* ip, uint16_t port) {
      fd = socket(AF_INET, SOCK_STREAM, 0);
      if (fd < 0) throw std::runtime_error("socket() failed");
  
      sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_port   = htons(port);
      if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        ::close(fd);
        throw std::runtime_error("inet_pton() failed");
      }
  
      if (connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        ::close(fd);
        throw std::runtime_error(std::string("connect() failed: ") + strerror(errno));
      }
    }
  
    ~TCPSocket() {
      if (fd >= 0) ::close(fd);
    }
  
    // send all bytes safely
    void sendAll(const std::string &data) {
      size_t total = 0, len = data.size();
      while (total < len) {
        ssize_t n = ::send(fd, data.data() + total, len - total, MSG_NOSIGNAL);
        if (n <= 0) throw std::runtime_error("send() failed");
        total += n;
      }
    }
  
  private:
    int fd{-1};
};

void start_tcp_loop() {
  std::thread([](){
    try {
      // Params 객체 생성
      Params params;
      // IP 문자열, 포트 문자열 읽어오기
      std::string ip       = params.get("ExternalPadIP");
      std::string port_str = params.get("ExternalPadPort");
      // 문자열을 정수로 변환
      uint16_t port = static_cast<uint16_t>(std::stoi(port_str));

      // 한 번 열어서 쓰레드가 끝날 때까지 재사용
      TCPSocket sock(ip.c_str(), port);
      //TCPSocket sock("192.168.219.100", 8080);
      while (true) {
        std::cout << "Sending data..." << std::endl;  // 로그 추가
        sock.sendAll("1234\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));  // 1Hz (필요시 50ms로 변경해서 20Hz)
      }
    } catch (const std::exception &e) {
      std::cerr << "TCP loop error: " << e.what() << std::endl;
      std::this_thread::sleep_for(std::chrono::seconds(1));
      // (필요하다면 재시도 로직 추가)
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
