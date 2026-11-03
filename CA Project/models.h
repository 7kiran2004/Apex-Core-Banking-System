#ifndef MODELS_H
#define MODELS_H

#include <string>

struct Account {
    int account_no;
    std::string holder_name;
    std::string account_type; // Savings, Current, Fixed Deposit
    std::string email;
    std::string phone;
    double balance;
    std::string status;       // ACTIVE, FROZEN, CLOSED
    std::string kyc_status;   // PENDING, VERIFIED, REJECTED
    std::string id_type;      // Passport, National ID, Driving License, SSN/Aadhaar
    std::string id_number;
    std::string address;
    std::string created_at;
    std::string updated_at;

    Account() : account_no(0), balance(0.0), status("ACTIVE"), kyc_status("VERIFIED") {}
};

struct Transaction {
    int tx_id;
    int account_no;
    std::string type;         // DEPOSIT, WITHDRAWAL, TRANSFER_IN, TRANSFER_OUT
    double amount;
    double balance_after;
    int target_account;       // Used in transfers, 0 if not applicable
    std::string remarks;
    std::string timestamp;

    Transaction() : tx_id(0), account_no(0), amount(0.0), balance_after(0.0), target_account(0) {}
};

struct AuditLog {
    int log_id;
    int account_no;
    std::string action;       // CREATE, DEPOSIT, WITHDRAW, TRANSFER, FREEZE, UNFREEZE, KYC_UPDATE, DELETE
    std::string details;
    std::string timestamp;

    AuditLog() : log_id(0), account_no(0) {}
};

struct BankStats {
    int total_accounts;
    int active_accounts;
    int frozen_accounts;
    int closed_accounts;
    double total_reserve;
    int total_transactions;
    int kyc_verified;
    int kyc_pending;

    BankStats() : total_accounts(0), active_accounts(0), frozen_accounts(0), 
                  closed_accounts(0), total_reserve(0.0), total_transactions(0),
                  kyc_verified(0), kyc_pending(0) {}
};

#endif // MODELS_H

