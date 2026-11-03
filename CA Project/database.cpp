#include "database.h"
#include <iostream>
#include <sstream>
#include <iomanip>

Database g_db;

Database::Database() : db(nullptr) {
    InitializeCriticalSection(&lock);
}

Database::~Database() {
    close();
    DeleteCriticalSection(&lock);
}

void Database::close() {
    lockDB();
    if (db) {
        sqlite3_close(db);
        db = nullptr;
    }
    unlockDB();
}

bool Database::executeSimple(const std::string& sql) {
    char* zErrMsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &zErrMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL Error: " << (zErrMsg ? zErrMsg : "unknown") << " in query: " << sql << std::endl;
        if (zErrMsg) sqlite3_free(zErrMsg);
        return false;
    }
    return true;
}

bool Database::init(const std::string& path) {
    lockDB();
    dbPath = path;
    int rc = sqlite3_open(dbPath.c_str(), &db);
    if (rc != SQLITE_OK) {
        std::cerr << "Cannot open database: " << sqlite3_errmsg(db) << std::endl;
        unlockDB();
        return false;
    }

    // Enable foreign keys and WAL mode for better concurrency
    executeSimple("PRAGMA foreign_keys = ON;");
    executeSimple("PRAGMA journal_mode = WAL;");

    // Accounts table
    std::string createAccountsTable = 
        "CREATE TABLE IF NOT EXISTS accounts ("
        "  account_no INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  holder_name TEXT NOT NULL,"
        "  account_type TEXT NOT NULL DEFAULT 'Savings',"
        "  email TEXT,"
        "  phone TEXT,"
        "  balance REAL NOT NULL DEFAULT 0.0,"
        "  status TEXT NOT NULL DEFAULT 'ACTIVE'," // ACTIVE, FROZEN, CLOSED
        "  kyc_status TEXT NOT NULL DEFAULT 'VERIFIED'," // PENDING, VERIFIED, REJECTED
        "  id_type TEXT,"
        "  id_number TEXT,"
        "  address TEXT,"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  updated_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");";

    // Transactions table
    std::string createTxTable = 
        "CREATE TABLE IF NOT EXISTS transactions ("
        "  tx_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  account_no INTEGER NOT NULL,"
        "  type TEXT NOT NULL," // DEPOSIT, WITHDRAWAL, TRANSFER_IN, TRANSFER_OUT
        "  amount REAL NOT NULL,"
        "  balance_after REAL NOT NULL,"
        "  target_account INTEGER DEFAULT 0,"
        "  remarks TEXT,"
        "  timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  FOREIGN KEY (account_no) REFERENCES accounts(account_no)"
        ");";

    // Audit logs table
    std::string createAuditTable = 
        "CREATE TABLE IF NOT EXISTS audit_logs ("
        "  log_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  account_no INTEGER NOT NULL,"
        "  action TEXT NOT NULL,"
        "  details TEXT,"
        "  timestamp DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");";

    bool ok = executeSimple(createAccountsTable) && 
              executeSimple(createTxTable) && 
              executeSimple(createAuditTable);

    unlockDB();

    if (ok) {
        seedSampleDataIfEmpty();
    }

    return ok;
}

void Database::seedSampleDataIfEmpty() {
    lockDB();
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT COUNT(*) FROM accounts;";
    int count = 0;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    if (count == 0) {
        std::cout << "[DB] Seeding initial sample banking data..." << std::endl;
        // Insert sample accounts starting with realistic 8-digit numbers
        executeSimple("INSERT INTO sqlite_sequence (name, seq) VALUES ('accounts', 10010000);");

        std::string seedSql = 
            "BEGIN TRANSACTION;"
            "INSERT INTO accounts (account_no, holder_name, account_type, email, phone, balance, status, kyc_status, id_type, id_number, address) "
            "VALUES (10010001, 'Alex Morgan', 'Savings', 'alex.morgan@fintech.io', '+1-555-0199', 5420.50, 'ACTIVE', 'VERIFIED', 'Passport', 'P9823411', '742 Evergreen Terrace, Springfield');"
            "INSERT INTO transactions (account_no, type, amount, balance_after, target_account, remarks) "
            "VALUES (10010001, 'DEPOSIT', 5420.50, 5420.50, 0, 'Initial Account Opening Deposit');"
            "INSERT INTO audit_logs (account_no, action, details) "
            "VALUES (10010001, 'CREATE', 'Account opened with Savings plan. KYC Verified with Passport.');"

            "INSERT INTO accounts (account_no, holder_name, account_type, email, phone, balance, status, kyc_status, id_type, id_number, address) "
            "VALUES (10010002, 'Sophia Chen', 'Current', 'sophia.c@techventures.com', '+1-555-0288', 18950.00, 'ACTIVE', 'VERIFIED', 'National ID', 'NID-882194', '452 Innovation Blvd, Silicon Valley');"
            "INSERT INTO transactions (account_no, type, amount, balance_after, target_account, remarks) "
            "VALUES (10010002, 'DEPOSIT', 18950.00, 18950.00, 0, 'Initial Corporate Account Opening Deposit');"
            "INSERT INTO audit_logs (account_no, action, details) "
            "VALUES (10010002, 'CREATE', 'Corporate Current account opened.');"

            "INSERT INTO accounts (account_no, holder_name, account_type, email, phone, balance, status, kyc_status, id_type, id_number, address) "
            "VALUES (10010003, 'Marcus Vance', 'Savings', 'marcus.v@webmail.org', '+1-555-0377', 850.00, 'FROZEN', 'PENDING', 'Driving License', 'DL-2023-991', '12 Ocean Drive, Miami');"
            "INSERT INTO transactions (account_no, type, amount, balance_after, target_account, remarks) "
            "VALUES (10010003, 'DEPOSIT', 850.00, 850.00, 0, 'Opening deposit');"
            "INSERT INTO audit_logs (account_no, action, details) "
            "VALUES (10010003, 'CREATE', 'Account created.');"
            "INSERT INTO audit_logs (account_no, action, details) "
            "VALUES (10010003, 'FREEZE', 'Account temporarily frozen pending address verification document.');"

            "COMMIT;";

        executeSimple(seedSql);
        std::cout << "[DB] Sample accounts and transactions ready." << std::endl;
    }
    unlockDB();
}

bool Database::createAccount(Account& acc, double initialDeposit, std::string& err) {
    lockDB();
    if (acc.holder_name.empty()) {
        err = "Account holder name is required.";
        unlockDB();
        return false;
    }
    if (acc.account_type.empty()) {
        acc.account_type = "Savings";
    }
    if (acc.status.empty()) {
        acc.status = "ACTIVE";
    }
    if (acc.kyc_status.empty()) {
        acc.kyc_status = "VERIFIED";
    }
    if (initialDeposit < 0.0) {
        err = "Initial deposit cannot be negative.";
        unlockDB();
        return false;
    }

    acc.balance = initialDeposit;

    executeSimple("BEGIN TRANSACTION;");

    const char* sql = "INSERT INTO accounts (holder_name, account_type, email, phone, balance, status, kyc_status, id_type, id_number, address) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        err = sqlite3_errmsg(db);
        executeSimple("ROLLBACK;");
        unlockDB();
        return false;
    }

    sqlite3_bind_text(stmt, 1, acc.holder_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, acc.account_type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, acc.email.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, acc.phone.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 5, acc.balance);
    sqlite3_bind_text(stmt, 6, acc.status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, acc.kyc_status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, acc.id_type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 9, acc.id_number.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 10, acc.address.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        err = sqlite3_errmsg(db);
        sqlite3_finalize(stmt);
        executeSimple("ROLLBACK;");
        unlockDB();
        return false;
    }
    sqlite3_finalize(stmt);

    acc.account_no = (int)sqlite3_last_insert_rowid(db);

    // Record initial deposit transaction if amount > 0
    if (initialDeposit > 0.0) {
        const char* txSql = "INSERT INTO transactions (account_no, type, amount, balance_after, target_account, remarks) "
                            "VALUES (?, 'DEPOSIT', ?, ?, 0, 'Account Opening Initial Deposit');";
        sqlite3_stmt* txStmt = nullptr;
        if (sqlite3_prepare_v2(db, txSql, -1, &txStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(txStmt, 1, acc.account_no);
            sqlite3_bind_double(txStmt, 2, initialDeposit);
            sqlite3_bind_double(txStmt, 3, acc.balance);
            sqlite3_step(txStmt);
            sqlite3_finalize(txStmt);
        }
    }

    // Record audit log
    const char* logSql = "INSERT INTO audit_logs (account_no, action, details) VALUES (?, 'CREATE', ?);";
    sqlite3_stmt* logStmt = nullptr;
    if (sqlite3_prepare_v2(db, logSql, -1, &logStmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(logStmt, 1, acc.account_no);
        std::string det = "Account opened with " + acc.account_type + " plan. Initial deposit: ₹" + std::to_string(initialDeposit);
        sqlite3_bind_text(logStmt, 2, det.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(logStmt);
        sqlite3_finalize(logStmt);
    }

    executeSimple("COMMIT;");
    unlockDB();
    return true;
}

bool Database::deposit(int account_no, double amount, const std::string& remarks, Transaction& outTx, std::string& err) {
    lockDB();
    if (amount <= 0.0) {
        err = "Deposit amount must be strictly greater than zero.";
        unlockDB();
        return false;
    }

    Account acc;
    // We unlock briefly or read directly
    sqlite3_stmt* stmt = nullptr;
    const char* qSql = "SELECT account_no, holder_name, balance, status FROM accounts WHERE account_no = ?;";
    if (sqlite3_prepare_v2(db, qSql, -1, &stmt, nullptr) != SQLITE_OK) {
        err = sqlite3_errmsg(db);
        unlockDB();
        return false;
    }
    sqlite3_bind_int(stmt, 1, account_no);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        err = "Account number #" + std::to_string(account_no) + " not found.";
        sqlite3_finalize(stmt);
        unlockDB();
        return false;
    }

    std::string status = (const char*)sqlite3_column_text(stmt, 3);
    double curBal = sqlite3_column_double(stmt, 2);
    sqlite3_finalize(stmt);

    if (status == "CLOSED") {
        err = "Cannot deposit funds: Account #" + std::to_string(account_no) + " is CLOSED.";
        unlockDB();
        return false;
    }
    if (status == "FROZEN") {
        err = "Cannot deposit funds: Account #" + std::to_string(account_no) + " is FROZEN. Please activate the account first.";
        unlockDB();
        return false;
    }

    double newBal = curBal + amount;
    executeSimple("BEGIN TRANSACTION;");

    const char* upSql = "UPDATE accounts SET balance = ?, updated_at = CURRENT_TIMESTAMP WHERE account_no = ?;";
    if (sqlite3_prepare_v2(db, upSql, -1, &stmt, nullptr) != SQLITE_OK) {
        err = sqlite3_errmsg(db);
        executeSimple("ROLLBACK;");
        unlockDB();
        return false;
    }
    sqlite3_bind_double(stmt, 1, newBal);
    sqlite3_bind_int(stmt, 2, account_no);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    // Insert transaction
    std::string note = remarks.empty() ? "Cash / Electronic Deposit" : remarks;
    const char* txSql = "INSERT INTO transactions (account_no, type, amount, balance_after, target_account, remarks) VALUES (?, 'DEPOSIT', ?, ?, 0, ?);";
    if (sqlite3_prepare_v2(db, txSql, -1, &stmt, nullptr) != SQLITE_OK) {
        err = sqlite3_errmsg(db);
        executeSimple("ROLLBACK;");
        unlockDB();
        return false;
    }
    sqlite3_bind_int(stmt, 1, account_no);
    sqlite3_bind_double(stmt, 2, amount);
    sqlite3_bind_double(stmt, 3, newBal);
    sqlite3_bind_text(stmt, 4, note.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    int txId = (int)sqlite3_last_insert_rowid(db);

    // Audit log
    const char* logSql = "INSERT INTO audit_logs (account_no, action, details) VALUES (?, 'DEPOSIT', ?);";
    if (sqlite3_prepare_v2(db, logSql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, account_no);
        std::ostringstream ss;
        ss << "Deposited ₹" << std::fixed << std::setprecision(2) << amount << ". New Balance: ₹" << newBal;
        std::string d = ss.str();
        sqlite3_bind_text(stmt, 2, d.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    executeSimple("COMMIT;");

    outTx.tx_id = txId;
    outTx.account_no = account_no;
    outTx.type = "DEPOSIT";
    outTx.amount = amount;
    outTx.balance_after = newBal;
    outTx.target_account = 0;
    outTx.remarks = note;

    unlockDB();
    return true;
}

bool Database::withdraw(int account_no, double amount, const std::string& remarks, Transaction& outTx, std::string& err) {
    lockDB();
    if (amount <= 0.0) {
        err = "Withdrawal amount must be strictly greater than zero.";
        unlockDB();
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    const char* qSql = "SELECT account_no, holder_name, balance, status FROM accounts WHERE account_no = ?;";
    if (sqlite3_prepare_v2(db, qSql, -1, &stmt, nullptr) != SQLITE_OK) {
        err = sqlite3_errmsg(db);
        unlockDB();
        return false;
    }
    sqlite3_bind_int(stmt, 1, account_no);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        err = "Account number #" + std::to_string(account_no) + " not found.";
        sqlite3_finalize(stmt);
        unlockDB();
        return false;
    }

    std::string status = (const char*)sqlite3_column_text(stmt, 3);
    double curBal = sqlite3_column_double(stmt, 2);
    sqlite3_finalize(stmt);

    if (status == "CLOSED") {
        err = "Account #" + std::to_string(account_no) + " is CLOSED. Transactions are prohibited.";
        unlockDB();
        return false;
    }
    if (status == "FROZEN") {
        err = "Account #" + std::to_string(account_no) + " is FROZEN. Withdrawals are blocked until account reactivation.";
        unlockDB();
        return false;
    }

    if (curBal < amount) {
        std::ostringstream ss;
        ss << "Insufficient funds! Current balance is ₹" << std::fixed << std::setprecision(2) << curBal 
           << ", cannot withdraw ₹" << amount << ".";
        err = ss.str();
        unlockDB();
        return false;
    }

    double newBal = curBal - amount;
    executeSimple("BEGIN TRANSACTION;");

    const char* upSql = "UPDATE accounts SET balance = ?, updated_at = CURRENT_TIMESTAMP WHERE account_no = ?;";
    if (sqlite3_prepare_v2(db, upSql, -1, &stmt, nullptr) != SQLITE_OK) {
        err = sqlite3_errmsg(db);
        executeSimple("ROLLBACK;");
        unlockDB();
        return false;
    }
    sqlite3_bind_double(stmt, 1, newBal);
    sqlite3_bind_int(stmt, 2, account_no);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    // Insert transaction
    std::string note = remarks.empty() ? "ATM / Cash Withdrawal" : remarks;
    const char* txSql = "INSERT INTO transactions (account_no, type, amount, balance_after, target_account, remarks) VALUES (?, 'WITHDRAWAL', ?, ?, 0, ?);";
    if (sqlite3_prepare_v2(db, txSql, -1, &stmt, nullptr) != SQLITE_OK) {
        err = sqlite3_errmsg(db);
        executeSimple("ROLLBACK;");
        unlockDB();
        return false;
    }
    sqlite3_bind_int(stmt, 1, account_no);
    sqlite3_bind_double(stmt, 2, amount);
    sqlite3_bind_double(stmt, 3, newBal);
    sqlite3_bind_text(stmt, 4, note.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    int txId = (int)sqlite3_last_insert_rowid(db);

    // Audit log
    const char* logSql = "INSERT INTO audit_logs (account_no, action, details) VALUES (?, 'WITHDRAWAL', ?);";
    if (sqlite3_prepare_v2(db, logSql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, account_no);
        std::ostringstream ss;
        ss << "Withdrew ₹" << std::fixed << std::setprecision(2) << amount << ". New Balance: ₹" << newBal;
        std::string d = ss.str();
        sqlite3_bind_text(stmt, 2, d.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    executeSimple("COMMIT;");

    outTx.tx_id = txId;
    outTx.account_no = account_no;
    outTx.type = "WITHDRAWAL";
    outTx.amount = amount;
    outTx.balance_after = newBal;
    outTx.target_account = 0;
    outTx.remarks = note;

    unlockDB();
    return true;
}

bool Database::transfer(int from_acc, int to_acc, double amount, const std::string& remarks, Transaction& outTxFrom, Transaction& outTxTo, std::string& err) {
    if (from_acc == to_acc) {
        err = "Source account and Destination account cannot be the same.";
        return false;
    }
    if (amount <= 0.0) {
        err = "Transfer amount must be strictly greater than zero.";
        return false;
    }

    lockDB();

    // Check sender
    sqlite3_stmt* stmt = nullptr;
    const char* qSql = "SELECT balance, status, holder_name FROM accounts WHERE account_no = ?;";
    if (sqlite3_prepare_v2(db, qSql, -1, &stmt, nullptr) != SQLITE_OK) {
        err = sqlite3_errmsg(db);
        unlockDB();
        return false;
    }
    sqlite3_bind_int(stmt, 1, from_acc);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        err = "Source account #" + std::to_string(from_acc) + " not found.";
        sqlite3_finalize(stmt);
        unlockDB();
        return false;
    }
    double fromBal = sqlite3_column_double(stmt, 0);
    std::string fromStatus = (const char*)sqlite3_column_text(stmt, 1);
    std::string fromHolder = (const char*)sqlite3_column_text(stmt, 2);
    sqlite3_finalize(stmt);

    if (fromStatus == "FROZEN" || fromStatus == "CLOSED") {
        err = "Transfer rejected: Source account #" + std::to_string(from_acc) + " is " + fromStatus + ".";
        unlockDB();
        return false;
    }

    if (fromBal < amount) {
        std::ostringstream ss;
        ss << "Insufficient balance! Source account #" << from_acc << " balance is ₹" 
           << std::fixed << std::setprecision(2) << fromBal << ", cannot transfer ₹" << amount << ".";
        err = ss.str();
        unlockDB();
        return false;
    }

    // Check recipient
    if (sqlite3_prepare_v2(db, qSql, -1, &stmt, nullptr) != SQLITE_OK) {
        err = sqlite3_errmsg(db);
        unlockDB();
        return false;
    }
    sqlite3_bind_int(stmt, 1, to_acc);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        err = "Destination account #" + std::to_string(to_acc) + " not found.";
        sqlite3_finalize(stmt);
        unlockDB();
        return false;
    }
    double toBal = sqlite3_column_double(stmt, 0);
    std::string toStatus = (const char*)sqlite3_column_text(stmt, 1);
    std::string toHolder = (const char*)sqlite3_column_text(stmt, 2);
    sqlite3_finalize(stmt);

    if (toStatus == "CLOSED") {
        err = "Cannot transfer funds to CLOSED account #" + std::to_string(to_acc) + ".";
        unlockDB();
        return false;
    }

    double newFromBal = fromBal - amount;
    double newToBal = toBal + amount;

    executeSimple("BEGIN TRANSACTION;");

    // Deduct from sender
    const char* upSender = "UPDATE accounts SET balance = ?, updated_at = CURRENT_TIMESTAMP WHERE account_no = ?;";
    if (sqlite3_prepare_v2(db, upSender, -1, &stmt, nullptr) != SQLITE_OK) {
        err = sqlite3_errmsg(db);
        executeSimple("ROLLBACK;");
        unlockDB();
        return false;
    }
    sqlite3_bind_double(stmt, 1, newFromBal);
    sqlite3_bind_int(stmt, 2, from_acc);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    // Add to recipient
    if (sqlite3_prepare_v2(db, upSender, -1, &stmt, nullptr) != SQLITE_OK) {
        err = sqlite3_errmsg(db);
        executeSimple("ROLLBACK;");
        unlockDB();
        return false;
    }
    sqlite3_bind_double(stmt, 1, newToBal);
    sqlite3_bind_int(stmt, 2, to_acc);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    // Insert Sender TX (TRANSFER_OUT)
    std::string senderRemark = remarks.empty() ? ("Transfer to #" + std::to_string(to_acc) + " (" + toHolder + ")") : remarks;
    const char* txSql = "INSERT INTO transactions (account_no, type, amount, balance_after, target_account, remarks) VALUES (?, ?, ?, ?, ?, ?);";
    if (sqlite3_prepare_v2(db, txSql, -1, &stmt, nullptr) != SQLITE_OK) {
        err = sqlite3_errmsg(db);
        executeSimple("ROLLBACK;");
        unlockDB();
        return false;
    }
    sqlite3_bind_int(stmt, 1, from_acc);
    sqlite3_bind_text(stmt, 2, "TRANSFER_OUT", -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 3, amount);
    sqlite3_bind_double(stmt, 4, newFromBal);
    sqlite3_bind_int(stmt, 5, to_acc);
    sqlite3_bind_text(stmt, 6, senderRemark.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    int fromTxId = (int)sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);

    // Insert Recipient TX (TRANSFER_IN)
    std::string recvRemark = remarks.empty() ? ("Transfer from #" + std::to_string(from_acc) + " (" + fromHolder + ")") : remarks;
    if (sqlite3_prepare_v2(db, txSql, -1, &stmt, nullptr) != SQLITE_OK) {
        err = sqlite3_errmsg(db);
        executeSimple("ROLLBACK;");
        unlockDB();
        return false;
    }
    sqlite3_bind_int(stmt, 1, to_acc);
    sqlite3_bind_text(stmt, 2, "TRANSFER_IN", -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 3, amount);
    sqlite3_bind_double(stmt, 4, newToBal);
    sqlite3_bind_int(stmt, 5, from_acc);
    sqlite3_bind_text(stmt, 6, recvRemark.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    int toTxId = (int)sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);

    // Audit logs
    const char* logSql = "INSERT INTO audit_logs (account_no, action, details) VALUES (?, 'TRANSFER', ?);";
    if (sqlite3_prepare_v2(db, logSql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, from_acc);
        std::string d = "Sent ₹" + std::to_string(amount) + " to #" + std::to_string(to_acc);
        sqlite3_bind_text(stmt, 2, d.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    if (sqlite3_prepare_v2(db, logSql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, to_acc);
        std::string d = "Received ₹" + std::to_string(amount) + " from #" + std::to_string(from_acc);
        sqlite3_bind_text(stmt, 2, d.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    executeSimple("COMMIT;");

    outTxFrom.tx_id = fromTxId;
    outTxFrom.account_no = from_acc;
    outTxFrom.type = "TRANSFER_OUT";
    outTxFrom.amount = amount;
    outTxFrom.balance_after = newFromBal;
    outTxFrom.target_account = to_acc;
    outTxFrom.remarks = senderRemark;

    outTxTo.tx_id = toTxId;
    outTxTo.account_no = to_acc;
    outTxTo.type = "TRANSFER_IN";
    outTxTo.amount = amount;
    outTxTo.balance_after = newToBal;
    outTxTo.target_account = from_acc;
    outTxTo.remarks = recvRemark;

    unlockDB();
    return true;
}

bool Database::setAccountStatus(int account_no, const std::string& newStatus, const std::string& reason, std::string& err) {
    if (newStatus != "ACTIVE" && newStatus != "FROZEN" && newStatus != "CLOSED") {
        err = "Invalid status. Must be ACTIVE, FROZEN, or CLOSED.";
        return false;
    }

    lockDB();
    sqlite3_stmt* stmt = nullptr;
    const char* checkSql = "SELECT status FROM accounts WHERE account_no = ?;";
    if (sqlite3_prepare_v2(db, checkSql, -1, &stmt, nullptr) != SQLITE_OK) {
        err = sqlite3_errmsg(db);
        unlockDB();
        return false;
    }
    sqlite3_bind_int(stmt, 1, account_no);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        err = "Account #" + std::to_string(account_no) + " not found.";
        sqlite3_finalize(stmt);
        unlockDB();
        return false;
    }
    std::string oldStatus = (const char*)sqlite3_column_text(stmt, 0);
    sqlite3_finalize(stmt);

    const char* upSql = "UPDATE accounts SET status = ?, updated_at = CURRENT_TIMESTAMP WHERE account_no = ?;";
    if (sqlite3_prepare_v2(db, upSql, -1, &stmt, nullptr) != SQLITE_OK) {
        err = sqlite3_errmsg(db);
        unlockDB();
        return false;
    }
    sqlite3_bind_text(stmt, 1, newStatus.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, account_no);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    // Audit log
    std::string actionName = (newStatus == "FROZEN") ? "FREEZE" : ((newStatus == "ACTIVE") ? "ACTIVATE" : "CLOSE");
    std::string detail = "Status changed from " + oldStatus + " to " + newStatus;
    if (!reason.empty()) {
        detail += ". Reason: " + reason;
    }

    const char* logSql = "INSERT INTO audit_logs (account_no, action, details) VALUES (?, ?, ?);";
    if (sqlite3_prepare_v2(db, logSql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, account_no);
        sqlite3_bind_text(stmt, 2, actionName.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, detail.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    unlockDB();
    return true;
}

bool Database::updateKyc(int account_no, const std::string& kycStatus, const std::string& idType, const std::string& idNumber, const std::string& address, std::string& err) {
    if (kycStatus != "VERIFIED" && kycStatus != "PENDING" && kycStatus != "REJECTED") {
        err = "Invalid KYC status. Must be VERIFIED, PENDING, or REJECTED.";
        return false;
    }

    lockDB();
    sqlite3_stmt* stmt = nullptr;
    const char* checkSql = "SELECT account_no FROM accounts WHERE account_no = ?;";
    if (sqlite3_prepare_v2(db, checkSql, -1, &stmt, nullptr) != SQLITE_OK) {
        err = sqlite3_errmsg(db);
        unlockDB();
        return false;
    }
    sqlite3_bind_int(stmt, 1, account_no);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        err = "Account #" + std::to_string(account_no) + " not found.";
        sqlite3_finalize(stmt);
        unlockDB();
        return false;
    }
    sqlite3_finalize(stmt);

    const char* upSql = "UPDATE accounts SET kyc_status = ?, id_type = ?, id_number = ?, address = ?, updated_at = CURRENT_TIMESTAMP WHERE account_no = ?;";
    if (sqlite3_prepare_v2(db, upSql, -1, &stmt, nullptr) != SQLITE_OK) {
        err = sqlite3_errmsg(db);
        unlockDB();
        return false;
    }
    sqlite3_bind_text(stmt, 1, kycStatus.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, idType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, idNumber.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, address.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, account_no);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    // Audit log
    const char* logSql = "INSERT INTO audit_logs (account_no, action, details) VALUES (?, 'KYC_UPDATE', ?);";
    if (sqlite3_prepare_v2(db, logSql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, account_no);
        std::string d = "KYC status updated to " + kycStatus + ". Doc: " + idType + " (" + idNumber + ")";
        sqlite3_bind_text(stmt, 2, d.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    unlockDB();
    return true;
}

bool Database::deleteOrCloseAccount(int account_no, bool permanent, std::string& err) {
    lockDB();
    sqlite3_stmt* stmt = nullptr;
    const char* qSql = "SELECT balance, status FROM accounts WHERE account_no = ?;";
    if (sqlite3_prepare_v2(db, qSql, -1, &stmt, nullptr) != SQLITE_OK) {
        err = sqlite3_errmsg(db);
        unlockDB();
        return false;
    }
    sqlite3_bind_int(stmt, 1, account_no);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        err = "Account #" + std::to_string(account_no) + " not found.";
        sqlite3_finalize(stmt);
        unlockDB();
        return false;
    }
    double curBal = sqlite3_column_double(stmt, 0);
    std::string curStatus = (const char*)sqlite3_column_text(stmt, 1);
    sqlite3_finalize(stmt);

    if (curBal > 0.009) {
        std::ostringstream ss;
        ss << "Cannot delete or close account with non-zero balance (₹" 
           << std::fixed << std::setprecision(2) << curBal 
           << "). Please withdraw or transfer all funds before deleting.";
        err = ss.str();
        unlockDB();
        return false;
    }

    executeSimple("BEGIN TRANSACTION;");

    if (permanent) {
        // Delete transactions & logs
        const char* delTx = "DELETE FROM transactions WHERE account_no = ?;";
        if (sqlite3_prepare_v2(db, delTx, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, account_no);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }

        const char* delLogs = "DELETE FROM audit_logs WHERE account_no = ?;";
        if (sqlite3_prepare_v2(db, delLogs, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, account_no);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }

        const char* delAcc = "DELETE FROM accounts WHERE account_no = ?;";
        if (sqlite3_prepare_v2(db, delAcc, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, account_no);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    } else {
        // Soft delete / closure
        const char* upSql = "UPDATE accounts SET status = 'CLOSED', updated_at = CURRENT_TIMESTAMP WHERE account_no = ?;";
        if (sqlite3_prepare_v2(db, upSql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, account_no);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }

        const char* logSql = "INSERT INTO audit_logs (account_no, action, details) VALUES (?, 'DELETE', 'Account closed and marked inactive.');";
        if (sqlite3_prepare_v2(db, logSql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, account_no);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    executeSimple("COMMIT;");
    unlockDB();
    return true;
}

bool Database::getAccount(int account_no, Account& acc) {
    lockDB();
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT account_no, holder_name, account_type, email, phone, balance, status, kyc_status, id_type, id_number, address, created_at, updated_at "
                      "FROM accounts WHERE account_no = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        unlockDB();
        return false;
    }
    sqlite3_bind_int(stmt, 1, account_no);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        unlockDB();
        return false;
    }

    acc.account_no = sqlite3_column_int(stmt, 0);
    acc.holder_name = sqlite3_column_text(stmt, 1) ? (const char*)sqlite3_column_text(stmt, 1) : "";
    acc.account_type = sqlite3_column_text(stmt, 2) ? (const char*)sqlite3_column_text(stmt, 2) : "";
    acc.email = sqlite3_column_text(stmt, 3) ? (const char*)sqlite3_column_text(stmt, 3) : "";
    acc.phone = sqlite3_column_text(stmt, 4) ? (const char*)sqlite3_column_text(stmt, 4) : "";
    acc.balance = sqlite3_column_double(stmt, 5);
    acc.status = sqlite3_column_text(stmt, 6) ? (const char*)sqlite3_column_text(stmt, 6) : "";
    acc.kyc_status = sqlite3_column_text(stmt, 7) ? (const char*)sqlite3_column_text(stmt, 7) : "";
    acc.id_type = sqlite3_column_text(stmt, 8) ? (const char*)sqlite3_column_text(stmt, 8) : "";
    acc.id_number = sqlite3_column_text(stmt, 9) ? (const char*)sqlite3_column_text(stmt, 9) : "";
    acc.address = sqlite3_column_text(stmt, 10) ? (const char*)sqlite3_column_text(stmt, 10) : "";
    acc.created_at = sqlite3_column_text(stmt, 11) ? (const char*)sqlite3_column_text(stmt, 11) : "";
    acc.updated_at = sqlite3_column_text(stmt, 12) ? (const char*)sqlite3_column_text(stmt, 12) : "";

    sqlite3_finalize(stmt);
    unlockDB();
    return true;
}

std::vector<Account> Database::getAllAccounts(const std::string& search, const std::string& statusFilter, const std::string& kycFilter) {
    lockDB();
    std::vector<Account> list;
    std::string sql = "SELECT account_no, holder_name, account_type, email, phone, balance, status, kyc_status, id_type, id_number, address, created_at, updated_at "
                      "FROM accounts WHERE 1=1 ";

    if (!statusFilter.empty() && statusFilter != "ALL") {
        sql += " AND status = '" + statusFilter + "' ";
    }
    if (!kycFilter.empty() && kycFilter != "ALL") {
        sql += " AND kyc_status = '" + kycFilter + "' ";
    }
    if (!search.empty()) {
        sql += " AND (holder_name LIKE ? OR account_no LIKE ? OR email LIKE ? OR phone LIKE ?) ";
    }
    sql += " ORDER BY account_no DESC;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        unlockDB();
        return list;
    }

    if (!search.empty()) {
        std::string pattern = "%" + search + "%";
        sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, pattern.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, pattern.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, pattern.c_str(), -1, SQLITE_TRANSIENT);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Account a;
        a.account_no = sqlite3_column_int(stmt, 0);
        a.holder_name = sqlite3_column_text(stmt, 1) ? (const char*)sqlite3_column_text(stmt, 1) : "";
        a.account_type = sqlite3_column_text(stmt, 2) ? (const char*)sqlite3_column_text(stmt, 2) : "";
        a.email = sqlite3_column_text(stmt, 3) ? (const char*)sqlite3_column_text(stmt, 3) : "";
        a.phone = sqlite3_column_text(stmt, 4) ? (const char*)sqlite3_column_text(stmt, 4) : "";
        a.balance = sqlite3_column_double(stmt, 5);
        a.status = sqlite3_column_text(stmt, 6) ? (const char*)sqlite3_column_text(stmt, 6) : "";
        a.kyc_status = sqlite3_column_text(stmt, 7) ? (const char*)sqlite3_column_text(stmt, 7) : "";
        a.id_type = sqlite3_column_text(stmt, 8) ? (const char*)sqlite3_column_text(stmt, 8) : "";
        a.id_number = sqlite3_column_text(stmt, 9) ? (const char*)sqlite3_column_text(stmt, 9) : "";
        a.address = sqlite3_column_text(stmt, 10) ? (const char*)sqlite3_column_text(stmt, 10) : "";
        a.created_at = sqlite3_column_text(stmt, 11) ? (const char*)sqlite3_column_text(stmt, 11) : "";
        a.updated_at = sqlite3_column_text(stmt, 12) ? (const char*)sqlite3_column_text(stmt, 12) : "";
        list.push_back(a);
    }

    sqlite3_finalize(stmt);
    unlockDB();
    return list;
}

std::vector<Transaction> Database::getTransactions(int account_no, int limit) {
    lockDB();
    std::vector<Transaction> list;
    std::string sql = "SELECT tx_id, account_no, type, amount, balance_after, target_account, remarks, timestamp FROM transactions ";
    if (account_no > 0) {
        sql += "WHERE account_no = " + std::to_string(account_no) + " ";
    }
    sql += "ORDER BY tx_id DESC LIMIT " + std::to_string(limit) + ";";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        unlockDB();
        return list;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Transaction t;
        t.tx_id = sqlite3_column_int(stmt, 0);
        t.account_no = sqlite3_column_int(stmt, 1);
        t.type = sqlite3_column_text(stmt, 2) ? (const char*)sqlite3_column_text(stmt, 2) : "";
        t.amount = sqlite3_column_double(stmt, 3);
        t.balance_after = sqlite3_column_double(stmt, 4);
        t.target_account = sqlite3_column_int(stmt, 5);
        t.remarks = sqlite3_column_text(stmt, 6) ? (const char*)sqlite3_column_text(stmt, 6) : "";
        t.timestamp = sqlite3_column_text(stmt, 7) ? (const char*)sqlite3_column_text(stmt, 7) : "";
        list.push_back(t);
    }

    sqlite3_finalize(stmt);
    unlockDB();
    return list;
}

std::vector<AuditLog> Database::getAuditLogs(int account_no, int limit) {
    lockDB();
    std::vector<AuditLog> list;
    std::string sql = "SELECT log_id, account_no, action, details, timestamp FROM audit_logs ";
    if (account_no > 0) {
        sql += "WHERE account_no = " + std::to_string(account_no) + " ";
    }
    sql += "ORDER BY log_id DESC LIMIT " + std::to_string(limit) + ";";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        unlockDB();
        return list;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        AuditLog l;
        l.log_id = sqlite3_column_int(stmt, 0);
        l.account_no = sqlite3_column_int(stmt, 1);
        l.action = sqlite3_column_text(stmt, 2) ? (const char*)sqlite3_column_text(stmt, 2) : "";
        l.details = sqlite3_column_text(stmt, 3) ? (const char*)sqlite3_column_text(stmt, 3) : "";
        l.timestamp = sqlite3_column_text(stmt, 4) ? (const char*)sqlite3_column_text(stmt, 4) : "";
        list.push_back(l);
    }

    sqlite3_finalize(stmt);
    unlockDB();
    return list;
}

BankStats Database::getStats() {
    lockDB();
    BankStats s;
    sqlite3_stmt* stmt = nullptr;

    const char* sqlAcc = "SELECT "
                         "COUNT(*), "
                         "SUM(CASE WHEN status = 'ACTIVE' THEN 1 ELSE 0 END), "
                         "SUM(CASE WHEN status = 'FROZEN' THEN 1 ELSE 0 END), "
                         "SUM(CASE WHEN status = 'CLOSED' THEN 1 ELSE 0 END), "
                         "COALESCE(SUM(balance), 0.0), "
                         "SUM(CASE WHEN kyc_status = 'VERIFIED' THEN 1 ELSE 0 END), "
                         "SUM(CASE WHEN kyc_status = 'PENDING' THEN 1 ELSE 0 END) "
                         "FROM accounts;";
    if (sqlite3_prepare_v2(db, sqlAcc, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            s.total_accounts = sqlite3_column_int(stmt, 0);
            s.active_accounts = sqlite3_column_int(stmt, 1);
            s.frozen_accounts = sqlite3_column_int(stmt, 2);
            s.closed_accounts = sqlite3_column_int(stmt, 3);
            s.total_reserve = sqlite3_column_double(stmt, 4);
            s.kyc_verified = sqlite3_column_int(stmt, 5);
            s.kyc_pending = sqlite3_column_int(stmt, 6);
        }
        sqlite3_finalize(stmt);
    }

    const char* sqlTx = "SELECT COUNT(*) FROM transactions;";
    if (sqlite3_prepare_v2(db, sqlTx, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            s.total_transactions = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    unlockDB();
    return s;
}

