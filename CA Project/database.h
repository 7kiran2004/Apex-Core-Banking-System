#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <vector>
#include <windows.h>
#include "sqlite3.h"
#include "models.h"

class Database {
private:
    sqlite3* db;
    CRITICAL_SECTION lock;
    std::string dbPath;

    void lockDB() { EnterCriticalSection(&lock); }
    void unlockDB() { LeaveCriticalSection(&lock); }

    bool executeSimple(const std::string& sql);

public:
    Database();
    ~Database();

    bool init(const std::string& path = "bank.db");
    void close();

    // Core Banking Operations
    bool createAccount(Account& acc, double initialDeposit, std::string& err);
    bool deposit(int account_no, double amount, const std::string& remarks, Transaction& outTx, std::string& err);
    bool withdraw(int account_no, double amount, const std::string& remarks, Transaction& outTx, std::string& err);
    bool transfer(int from_acc, int to_acc, double amount, const std::string& remarks, Transaction& outTxFrom, Transaction& outTxTo, std::string& err);
    bool setAccountStatus(int account_no, const std::string& newStatus, const std::string& reason, std::string& err);
    bool updateKyc(int account_no, const std::string& kycStatus, const std::string& idType, const std::string& idNumber, const std::string& address, std::string& err);
    bool deleteOrCloseAccount(int account_no, bool permanent, std::string& err);

    // Queries
    bool getAccount(int account_no, Account& acc);
    std::vector<Account> getAllAccounts(const std::string& search = "", const std::string& statusFilter = "", const std::string& kycFilter = "");
    std::vector<Transaction> getTransactions(int account_no = 0, int limit = 100);
    std::vector<AuditLog> getAuditLogs(int account_no = 0, int limit = 50);
    BankStats getStats();

    // Seed initial demo data if empty
    void seedSampleDataIfEmpty();
};

extern Database g_db;

#endif // DATABASE_H

