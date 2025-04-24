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

void send_tcp_test() {
  // 1) TCP용 소켓 생성
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) return;

  sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(8080);                     // 패드에서 열린 포트
  inet_pton(AF_INET, "192.168.219.100", &addr.sin_addr);  // 패드 IP

  // 2) 서버(패드)에 연결
  if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
    close(sock);
    return;
  }

  // 3) 데이터 전송 (sendto → send)
  const char* msg = "1234";
  send(sock, msg, strlen(msg), 0);

  // 4) 소켓 닫기
  close(sock);
}

void start_tcp_loop() {
  std::thread([](){
    while (true) {
      send_tcp_test();  // TCP 연결 후 전송
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));  // 50:20Hz
    }
  }).detach();  // 백그라운드 실행
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
