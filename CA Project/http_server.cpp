#include "http_server.h"
#include "database.h"
#include "json_utils.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>

struct ClientContext {
    HttpServer* server;
    SOCKET clientSocket;
};

static std::string urlDecode(const std::string& in) {
    std::string out;
    out.reserve(in.length());
    for (size_t i = 0; i < in.length(); ++i) {
        if (in[i] == '%') {
            if (i + 2 < in.length()) {
                int hexVal = 0;
                std::istringstream hexStream(in.substr(i + 1, 2));
                if (hexStream >> std::hex >> hexVal) {
                    out += static_cast<char>(hexVal);
                    i += 2;
                } else {
                    out += '%';
                }
            } else {
                out += '%';
            }
        } else if (in[i] == '+') {
            out += ' ';
        } else {
            out += in[i];
        }
    }
    return out;
}

HttpServer::HttpServer(int p, const std::string& webDir)
    : port(p), listenSocket(INVALID_SOCKET), running(false), serverThread(NULL), webRoot(webDir) {
}

HttpServer::~HttpServer() {
    stop();
}

bool HttpServer::start() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        return false;
    }

    listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        std::cerr << "socket() failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return false;
    }

    // Set reuse address
    int opt = 1;
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in service;
    service.sin_family = AF_INET;
    service.sin_addr.s_addr = INADDR_ANY;
    service.sin_port = htons(port);

    if (bind(listenSocket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR) {
        std::cerr << "bind() failed on port " << port << ": " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return false;
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "listen() failed: " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return false;
    }

    running = true;
    serverThread = CreateThread(NULL, 0, ListenLoop, this, 0, NULL);
    if (!serverThread) {
        std::cerr << "CreateThread() failed." << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return false;
    }

    std::cout << "[HTTP] Banking Web Server running at: http://localhost:" << port << "/" << std::endl;
    return true;
}

void HttpServer::stop() {
    if (running) {
        running = false;
        if (listenSocket != INVALID_SOCKET) {
            closesocket(listenSocket);
            listenSocket = INVALID_SOCKET;
        }
        if (serverThread) {
            WaitForSingleObject(serverThread, 2000);
            CloseHandle(serverThread);
            serverThread = NULL;
        }
        WSACleanup();
        std::cout << "[HTTP] Server stopped." << std::endl;
    }
}

DWORD WINAPI HttpServer::ListenLoop(LPVOID param) {
    HttpServer* self = static_cast<HttpServer*>(param);

    while (self->running) {
        sockaddr_in clientAddr;
        int clientAddrLen = sizeof(clientAddr);
        SOCKET clientSock = accept(self->listenSocket, (SOCKADDR*)&clientAddr, &clientAddrLen);
        if (clientSock == INVALID_SOCKET) {
            if (!self->running) break;
            continue;
        }

        ClientContext* ctx = new ClientContext();
        ctx->server = self;
        ctx->clientSocket = clientSock;

        HANDLE hThread = CreateThread(NULL, 0, ClientHandler, ctx, 0, NULL);
        if (hThread) {
            CloseHandle(hThread); // Detached
        } else {
            closesocket(clientSock);
            delete ctx;
        }
    }

    return 0;
}

DWORD WINAPI HttpServer::ClientHandler(LPVOID param) {
    ClientContext* ctx = static_cast<ClientContext*>(param);
    SOCKET sock = ctx->clientSocket;
    HttpServer* self = ctx->server;
    delete ctx;

    // Set 3 second receive timeout to prevent hanging connections
    DWORD timeoutMs = 3000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeoutMs, sizeof(timeoutMs));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeoutMs, sizeof(timeoutMs));

    std::vector<char> buffer(4096);
    std::string rawData;
    size_t headerEnd = std::string::npos;
    size_t delimLen = 4;

    // Read until headers are fully received
    while (true) {
        headerEnd = rawData.find("\r\n\r\n");
        if (headerEnd != std::string::npos) {
            delimLen = 4;
            break;
        }
        headerEnd = rawData.find("\n\n");
        if (headerEnd != std::string::npos) {
            delimLen = 2;
            break;
        }

        int bytesReceived = recv(sock, buffer.data(), (int)buffer.size(), 0);
        if (bytesReceived <= 0) break;
        rawData.append(buffer.data(), bytesReceived);
    }

    if (headerEnd == std::string::npos) {
        closesocket(sock);
        return 0;
    }

    std::string headerPart = rawData.substr(0, headerEnd);
    std::string bodyPart = rawData.substr(headerEnd + delimLen);

    // Parse request line
    HttpRequest req;
    std::istringstream stream(headerPart);
    std::string requestLine;
    if (std::getline(stream, requestLine)) {
        if (!requestLine.empty() && requestLine.back() == '\r') requestLine.pop_back();
        std::istringstream lineStream(requestLine);
        lineStream >> req.method >> req.path;
    }

    // Split path and query
    size_t queryPos = req.path.find('?');
    if (queryPos != std::string::npos) {
        req.rawQuery = req.path.substr(queryPos + 1);
        req.path = req.path.substr(0, queryPos);

        // Parse query params
        std::istringstream qStream(req.rawQuery);
        std::string pair;
        while (std::getline(qStream, pair, '&')) {
            size_t eqPos = pair.find('=');
            if (eqPos != std::string::npos) {
                std::string k = urlDecode(pair.substr(0, eqPos));
                std::string v = urlDecode(pair.substr(eqPos + 1));
                req.queryParams[k] = v;
            } else if (!pair.empty()) {
                req.queryParams[urlDecode(pair)] = "";
            }
        }
    }

    // Parse headers
    std::string headerLine;
    size_t expectedContentLength = 0;
    while (std::getline(stream, headerLine)) {
        if (!headerLine.empty() && headerLine.back() == '\r') headerLine.pop_back();
        size_t colon = headerLine.find(':');
        if (colon != std::string::npos) {
            std::string key = headerLine.substr(0, colon);
            std::string val = headerLine.substr(colon + 1);
            while (!val.empty() && (val.front() == ' ' || val.front() == '\t')) val.erase(0, 1);
            while (!val.empty() && (val.back() == ' ' || val.back() == '\t' || val.back() == '\r')) val.pop_back();
            std::string lowerKey = key;
            std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);
            req.headers[lowerKey] = val;
            if (lowerKey == "content-length") {
                try {
                    expectedContentLength = std::stoul(val);
                } catch (...) {}
            }
        }
    }

    // Read remaining body if needed
    while (bodyPart.length() < expectedContentLength) {
        int needed = (int)(expectedContentLength - bodyPart.length());
        int bytesToRead = (needed < (int)buffer.size()) ? needed : (int)buffer.size();
        int more = recv(sock, buffer.data(), bytesToRead, 0);
        if (more <= 0) break;
        bodyPart.append(buffer.data(), more);
    }
    req.body = bodyPart;

    std::cout << "[HTTP] " << req.method << " " << req.path << " (Body: " << req.body.length() << "b)" << std::endl;

    // Handle OPTIONS CORS preflight
    HttpResponse res;
    if (req.method == "OPTIONS") {
        res.status = 200;
        res.statusText = "OK";
        res.contentType = "text/plain";
        res.body = "";
    } else {
        res = self->routeRequest(req);
    }

    // Build HTTP response string
    std::ostringstream responseStream;
    responseStream << "HTTP/1.1 " << res.status << " " << res.statusText << "\r\n";
    responseStream << "Content-Type: " << res.contentType << "\r\n";
    responseStream << "Content-Length: " << res.body.length() << "\r\n";
    responseStream << "Access-Control-Allow-Origin: *\r\n";
    responseStream << "Access-Control-Allow-Methods: GET, POST, OPTIONS, PUT, DELETE\r\n";
    responseStream << "Access-Control-Allow-Headers: Content-Type, Authorization\r\n";
    responseStream << "Connection: close\r\n";
    responseStream << "\r\n";
    responseStream << res.body;

    std::string fullResponse = responseStream.str();
    int totalSent = 0;
    int toSend = (int)fullResponse.length();
    const char* p = fullResponse.c_str();
    while (totalSent < toSend) {
        int s = send(sock, p + totalSent, toSend - totalSent, 0);
        if (s <= 0) break;
        totalSent += s;
    }

    shutdown(sock, SD_SEND);
    closesocket(sock);
    return 0;
}

HttpResponse HttpServer::routeRequest(const HttpRequest& req) {
    // API endpoints
    if (req.path == "/api/stats" && req.method == "GET") {
        return apiGetStats();
    }
    if (req.path == "/api/accounts" && req.method == "GET") {
        return apiGetAccounts(req);
    }
    if (req.path.find("/api/accounts/") == 0 && req.method == "GET") {
        std::string idStr = req.path.substr(14);
        try {
            int id = std::stoi(idStr);
            return apiGetAccountById(id);
        } catch (...) {
            HttpResponse res;
            res.status = 400;
            res.statusText = "Bad Request";
            res.contentType = "application/json";
            res.body = JsonUtils::errorResponse("Invalid account number");
            return res;
        }
    }
    if (req.path == "/api/accounts" && req.method == "POST") {
        return apiCreateAccount(req);
    }
    if (req.path == "/api/deposit" && req.method == "POST") {
        return apiDeposit(req);
    }
    if (req.path == "/api/withdraw" && req.method == "POST") {
        return apiWithdraw(req);
    }
    if (req.path == "/api/transfer" && req.method == "POST") {
        return apiTransfer(req);
    }
    if (req.path == "/api/freeze" && req.method == "POST") {
        return apiFreeze(req);
    }
    if (req.path == "/api/kyc" && req.method == "POST") {
        return apiKyc(req);
    }
    if (req.path == "/api/delete" && req.method == "POST") {
        return apiDeleteAccount(req);
    }
    if (req.path == "/api/transactions" && req.method == "GET") {
        return apiGetTransactions(req);
    }
    if (req.path == "/api/audit" && req.method == "GET") {
        return apiGetAuditLogs(req);
    }

    // Static Web File serving
    return handleStaticFile(req.path);
}

HttpResponse HttpServer::handleStaticFile(const std::string& reqPath) {
    HttpResponse res;
    std::string safePath = reqPath;
    if (safePath == "/" || safePath.empty()) {
        safePath = "/index.html";
    }

    // Prevent directory traversal
    if (safePath.find("..") != std::string::npos) {
        res.status = 403;
        res.statusText = "Forbidden";
        res.contentType = "text/plain";
        res.body = "Forbidden";
        return res;
    }

    std::string fullPath = webRoot + safePath;
    std::ifstream file(fullPath.c_str(), std::ios::binary);
    if (!file.is_open()) {
        res.status = 404;
        res.statusText = "Not Found";
        res.contentType = "text/html; charset=utf-8";
        res.body = "<h1>404 Not Found</h1><p>Requested file " + safePath + " was not found on this server.</p>";
        return res;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    res.body = ss.str();
    res.status = 200;
    res.statusText = "OK";

    // Detect MIME type
    if (safePath.rfind(".html") != std::string::npos || safePath.rfind(".htm") != std::string::npos) {
        res.contentType = "text/html; charset=utf-8";
    } else if (safePath.rfind(".css") != std::string::npos) {
        res.contentType = "text/css; charset=utf-8";
    } else if (safePath.rfind(".js") != std::string::npos) {
        res.contentType = "application/javascript; charset=utf-8";
    } else if (safePath.rfind(".json") != std::string::npos) {
        res.contentType = "application/json";
    } else if (safePath.rfind(".svg") != std::string::npos) {
        res.contentType = "image/svg+xml";
    } else if (safePath.rfind(".png") != std::string::npos) {
        res.contentType = "image/png";
    } else if (safePath.rfind(".jpg") != std::string::npos || safePath.rfind(".jpeg") != std::string::npos) {
        res.contentType = "image/jpeg";
    } else if (safePath.rfind(".ico") != std::string::npos) {
        res.contentType = "image/x-icon";
    } else {
        res.contentType = "text/plain";
    }

    return res;
}

HttpResponse HttpServer::apiGetStats() {
    HttpResponse res;
    res.contentType = "application/json";
    BankStats s = g_db.getStats();
    res.body = JsonUtils::successResponse("Stats retrieved", JsonUtils::statsToJson(s));
    return res;
}

HttpResponse HttpServer::apiGetAccounts(const HttpRequest& req) {
    HttpResponse res;
    res.contentType = "application/json";

    std::string search = "";
    std::string status = "";
    std::string kyc = "";

    auto it = req.queryParams.find("q");
    if (it != req.queryParams.end()) search = it->second;

    it = req.queryParams.find("status");
    if (it != req.queryParams.end()) status = it->second;

    it = req.queryParams.find("kyc");
    if (it != req.queryParams.end()) kyc = it->second;

    std::vector<Account> list = g_db.getAllAccounts(search, status, kyc);

    std::ostringstream jsonArr;
    jsonArr << "[";
    for (size_t i = 0; i < list.size(); ++i) {
        if (i > 0) jsonArr << ",";
        jsonArr << JsonUtils::accountToJson(list[i]);
    }
    jsonArr << "]";

    res.body = JsonUtils::successResponse("Accounts list", jsonArr.str());
    return res;
}

HttpResponse HttpServer::apiGetAccountById(int accountNo) {
    HttpResponse res;
    res.contentType = "application/json";
    Account acc;
    if (g_db.getAccount(accountNo, acc)) {
        res.body = JsonUtils::successResponse("Account details", JsonUtils::accountToJson(acc));
    } else {
        res.status = 404;
        res.statusText = "Not Found";
        res.body = JsonUtils::errorResponse("Account not found");
    }
    return res;
}

HttpResponse HttpServer::apiCreateAccount(const HttpRequest& req) {
    HttpResponse res;
    res.contentType = "application/json";

    Account acc;
    acc.holder_name = JsonUtils::getString(req.body, "holder_name");
    acc.account_type = JsonUtils::getString(req.body, "account_type", "Savings");
    acc.email = JsonUtils::getString(req.body, "email");
    acc.phone = JsonUtils::getString(req.body, "phone");
    acc.id_type = JsonUtils::getString(req.body, "id_type", "National ID");
    acc.id_number = JsonUtils::getString(req.body, "id_number");
    acc.address = JsonUtils::getString(req.body, "address");
    acc.status = "ACTIVE";
    acc.kyc_status = JsonUtils::getString(req.body, "kyc_status", "VERIFIED");

    double initialDeposit = JsonUtils::getDouble(req.body, "initial_deposit", 0.0);

    if (acc.holder_name.empty()) {
        res.status = 400;
        res.body = JsonUtils::errorResponse("Full holder name is required.");
        return res;
    }

    std::string err;
    if (g_db.createAccount(acc, initialDeposit, err)) {
        res.body = JsonUtils::successResponse("Account created successfully with Account Number #" + std::to_string(acc.account_no), JsonUtils::accountToJson(acc));
    } else {
        res.status = 400;
        res.body = JsonUtils::errorResponse(err);
    }
    return res;
}

HttpResponse HttpServer::apiDeposit(const HttpRequest& req) {
    HttpResponse res;
    res.contentType = "application/json";

    int accountNo = JsonUtils::getInt(req.body, "account_no");
    double amount = JsonUtils::getDouble(req.body, "amount");
    std::string remarks = JsonUtils::getString(req.body, "remarks");

    if (accountNo <= 0) {
        res.status = 400;
        res.body = JsonUtils::errorResponse("Valid account number is required.");
        return res;
    }

    Transaction tx;
    std::string err;
    if (g_db.deposit(accountNo, amount, remarks, tx, err)) {
        res.body = JsonUtils::successResponse("Deposit successful! New balance: ₹" + std::to_string(tx.balance_after), JsonUtils::transactionToJson(tx));
    } else {
        res.status = 400;
        res.body = JsonUtils::errorResponse(err);
    }
    return res;
}

HttpResponse HttpServer::apiWithdraw(const HttpRequest& req) {
    HttpResponse res;
    res.contentType = "application/json";

    int accountNo = JsonUtils::getInt(req.body, "account_no");
    double amount = JsonUtils::getDouble(req.body, "amount");
    std::string remarks = JsonUtils::getString(req.body, "remarks");

    if (accountNo <= 0) {
        res.status = 400;
        res.body = JsonUtils::errorResponse("Valid account number is required.");
        return res;
    }

    Transaction tx;
    std::string err;
    if (g_db.withdraw(accountNo, amount, remarks, tx, err)) {
        res.body = JsonUtils::successResponse("Withdrawal successful! Remaining balance: ₹" + std::to_string(tx.balance_after), JsonUtils::transactionToJson(tx));
    } else {
        res.status = 400;
        res.body = JsonUtils::errorResponse(err);
    }
    return res;
}

HttpResponse HttpServer::apiTransfer(const HttpRequest& req) {
    HttpResponse res;
    res.contentType = "application/json";

    int fromAcc = JsonUtils::getInt(req.body, "from_account");
    int toAcc = JsonUtils::getInt(req.body, "to_account");
    double amount = JsonUtils::getDouble(req.body, "amount");
    std::string remarks = JsonUtils::getString(req.body, "remarks");

    if (fromAcc <= 0 || toAcc <= 0) {
        res.status = 400;
        res.body = JsonUtils::errorResponse("Both sender and receiver account numbers are required.");
        return res;
    }

    Transaction txFrom, txTo;
    std::string err;
    if (g_db.transfer(fromAcc, toAcc, amount, remarks, txFrom, txTo, err)) {
        std::ostringstream ss;
        ss << "Transfer of ₹" << amount << " from #" << fromAcc << " to #" << toAcc << " completed successfully!";
        res.body = JsonUtils::successResponse(ss.str(), JsonUtils::transactionToJson(txFrom));
    } else {
        res.status = 400;
        res.body = JsonUtils::errorResponse(err);
    }
    return res;
}

HttpResponse HttpServer::apiFreeze(const HttpRequest& req) {
    HttpResponse res;
    res.contentType = "application/json";

    int accountNo = JsonUtils::getInt(req.body, "account_no");
    std::string status = JsonUtils::getString(req.body, "status"); // "FROZEN" or "ACTIVE"
    std::string reason = JsonUtils::getString(req.body, "reason");

    if (accountNo <= 0 || status.empty()) {
        res.status = 400;
        res.body = JsonUtils::errorResponse("Account number and target status ('FROZEN' or 'ACTIVE') are required.");
        return res;
    }

    std::string err;
    if (g_db.setAccountStatus(accountNo, status, reason, err)) {
        Account acc;
        g_db.getAccount(accountNo, acc);
        std::string actionVerb = (status == "FROZEN") ? "frozen" : "activated";
        res.body = JsonUtils::successResponse("Account #" + std::to_string(accountNo) + " is now " + actionVerb + ".", JsonUtils::accountToJson(acc));
    } else {
        res.status = 400;
        res.body = JsonUtils::errorResponse(err);
    }
    return res;
}

HttpResponse HttpServer::apiKyc(const HttpRequest& req) {
    HttpResponse res;
    res.contentType = "application/json";

    int accountNo = JsonUtils::getInt(req.body, "account_no");
    std::string kycStatus = JsonUtils::getString(req.body, "kyc_status", "VERIFIED");
    std::string idType = JsonUtils::getString(req.body, "id_type");
    std::string idNumber = JsonUtils::getString(req.body, "id_number");
    std::string address = JsonUtils::getString(req.body, "address");

    if (accountNo <= 0) {
        res.status = 400;
        res.body = JsonUtils::errorResponse("Account number is required.");
        return res;
    }

    std::string err;
    if (g_db.updateKyc(accountNo, kycStatus, idType, idNumber, address, err)) {
        Account acc;
        g_db.getAccount(accountNo, acc);
        res.body = JsonUtils::successResponse("KYC record updated for Account #" + std::to_string(accountNo), JsonUtils::accountToJson(acc));
    } else {
        res.status = 400;
        res.body = JsonUtils::errorResponse(err);
    }
    return res;
}

HttpResponse HttpServer::apiDeleteAccount(const HttpRequest& req) {
    HttpResponse res;
    res.contentType = "application/json";

    int accountNo = JsonUtils::getInt(req.body, "account_no");
    bool permanent = JsonUtils::getBool(req.body, "permanent", false);

    if (accountNo <= 0) {
        res.status = 400;
        res.body = JsonUtils::errorResponse("Account number is required.");
        return res;
    }

    std::string err;
    if (g_db.deleteOrCloseAccount(accountNo, permanent, err)) {
        std::string actionStr = permanent ? "permanently deleted" : "closed and archived";
        res.body = JsonUtils::successResponse("Account #" + std::to_string(accountNo) + " has been " + actionStr + ".");
    } else {
        res.status = 400;
        res.body = JsonUtils::errorResponse(err);
    }
    return res;
}

HttpResponse HttpServer::apiGetTransactions(const HttpRequest& req) {
    HttpResponse res;
    res.contentType = "application/json";

    int accountNo = 0;
    int limit = 100;

    auto it = req.queryParams.find("account_no");
    if (it != req.queryParams.end()) {
        try { accountNo = std::stoi(it->second); } catch (...) {}
    }

    it = req.queryParams.find("limit");
    if (it != req.queryParams.end()) {
        try { limit = std::stoi(it->second); } catch (...) {}
    }

    std::vector<Transaction> list = g_db.getTransactions(accountNo, limit);

    std::ostringstream jsonArr;
    jsonArr << "[";
    for (size_t i = 0; i < list.size(); ++i) {
        if (i > 0) jsonArr << ",";
        jsonArr << JsonUtils::transactionToJson(list[i]);
    }
    jsonArr << "]";

    res.body = JsonUtils::successResponse("Transactions list", jsonArr.str());
    return res;
}

HttpResponse HttpServer::apiGetAuditLogs(const HttpRequest& req) {
    HttpResponse res;
    res.contentType = "application/json";

    int accountNo = 0;
    int limit = 50;

    auto it = req.queryParams.find("account_no");
    if (it != req.queryParams.end()) {
        try { accountNo = std::stoi(it->second); } catch (...) {}
    }

    it = req.queryParams.find("limit");
    if (it != req.queryParams.end()) {
        try { limit = std::stoi(it->second); } catch (...) {}
    }

    std::vector<AuditLog> list = g_db.getAuditLogs(accountNo, limit);

    std::ostringstream jsonArr;
    jsonArr << "[";
    for (size_t i = 0; i < list.size(); ++i) {
        if (i > 0) jsonArr << ",";
        jsonArr << JsonUtils::auditLogToJson(list[i]);
    }
    jsonArr << "]";

    res.body = JsonUtils::successResponse("Audit logs list", jsonArr.str());
    return res;
}
