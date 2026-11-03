#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <string>
#include <map>
#include <winsock2.h>
#include <windows.h>

struct HttpRequest {
    std::string method;
    std::string path;
    std::string rawQuery;
    std::map<std::string, std::string> queryParams;
    std::map<std::string, std::string> headers;
    std::string body;
};

struct HttpResponse {
    int status;
    std::string statusText;
    std::string contentType;
    std::string body;

    HttpResponse() : status(200), statusText("OK"), contentType("text/html; charset=utf-8") {}
};

class HttpServer {
private:
    int port;
    SOCKET listenSocket;
    bool running;
    HANDLE serverThread;
    std::string webRoot;

    static DWORD WINAPI ListenLoop(LPVOID param);
    static DWORD WINAPI ClientHandler(LPVOID param);

    HttpResponse routeRequest(const HttpRequest& req);
    HttpResponse handleStaticFile(const std::string& path);
    
    // API handlers
    HttpResponse apiGetStats();
    HttpResponse apiGetAccounts(const HttpRequest& req);
    HttpResponse apiGetAccountById(int accountNo);
    HttpResponse apiCreateAccount(const HttpRequest& req);
    HttpResponse apiDeposit(const HttpRequest& req);
    HttpResponse apiWithdraw(const HttpRequest& req);
    HttpResponse apiTransfer(const HttpRequest& req);
    HttpResponse apiFreeze(const HttpRequest& req);
    HttpResponse apiKyc(const HttpRequest& req);
    HttpResponse apiDeleteAccount(const HttpRequest& req);
    HttpResponse apiGetTransactions(const HttpRequest& req);
    HttpResponse apiGetAuditLogs(const HttpRequest& req);

public:
    HttpServer(int port = 8080, const std::string& webDir = "web");
    ~HttpServer();

    bool start();
    void stop();
    bool isRunning() const { return running; }
    int getPort() const { return port; }
};

#endif // HTTP_SERVER_H

