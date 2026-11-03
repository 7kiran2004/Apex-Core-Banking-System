#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <windows.h>
#include <shellapi.h>
#include "database.h"
#include "http_server.h"

void printBanner(int port) {
    std::cout << "\n";
    std::cout << "========================================================================\n";
    std::cout << "               APEX CORE BANKING ENGINE & WEB PORTAL                    \n";
    std::cout << "                  Backend: C++11 | Engine: SQLite3                      \n";
    std::cout << "========================================================================\n";
    std::cout << " [OK] Local SQL Database   : bank.db (SQLite 3.45)\n";
    std::cout << " [OK] High-Speed HTTP API  : http://localhost:" << port << "/\n";
    std::cout << " [OK] Frontend Web Portal  : http://localhost:" << port << "/index.html\n";
    std::cout << "========================================================================\n";
    std::cout << " Access the Banking Web App in your browser or use the Console CLI below!\n";
    std::cout << "========================================================================\n\n";
}

void openBrowser(int port) {
    std::string url = "http://localhost:" + std::to_string(port);
    ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
    std::cout << "-> Opened " << url << " in your default web browser.\n";
}

void handleCliViewAccounts() {
    std::vector<Account> list = g_db.getAllAccounts();
    std::cout << "\n--- CURRENT BANK ACCOUNTS (" << list.size() << ") ---\n";
    std::cout << std::left 
              << std::setw(12) << "Acc No"
              << std::setw(20) << "Holder Name"
              << std::setw(12) << "Type"
              << std::setw(14) << "Balance (₹)"
              << std::setw(12) << "Status"
              << std::setw(12) << "KYC"
              << "\n";
    std::cout << std::string(80, '-') << "\n";

    for (const auto& a : list) {
        std::cout << std::left 
                  << std::setw(12) << a.account_no
                  << std::setw(20) << a.holder_name
                  << std::setw(12) << a.account_type
                  << std::setw(14) << std::fixed << std::setprecision(2) << a.balance
                  << std::setw(12) << a.status
                  << std::setw(12) << a.kyc_status
                  << "\n";
    }
    std::cout << "\n";
}

void handleCliCreateAccount() {
    std::cout << "\n--- CREATE NEW ACCOUNT ---\n";
    Account acc;
    std::cout << "Enter Full Name: ";
    std::string name;
    std::getline(std::cin >> std::ws, name);
    acc.holder_name = name;

    std::cout << "Enter Account Type (1: Savings, 2: Current, 3: Fixed Deposit) [Default 1]: ";
    std::string typeChoice;
    std::getline(std::cin, typeChoice);
    if (typeChoice == "2") acc.account_type = "Current";
    else if (typeChoice == "3") acc.account_type = "Fixed Deposit";
    else acc.account_type = "Savings";

    std::cout << "Enter Email: ";
    std::getline(std::cin, acc.email);

    std::cout << "Enter Phone: ";
    std::getline(std::cin, acc.phone);

    std::cout << "Enter Initial Deposit Amount (₹): ";
    double initialDeposit = 0.0;
    std::cin >> initialDeposit;

    std::cout << "Enter ID Proof Document Type (Passport/National ID/Driving License): ";
    std::getline(std::cin >> std::ws, acc.id_type);

    std::cout << "Enter ID Document Number: ";
    std::getline(std::cin, acc.id_number);

    std::cout << "Enter Address: ";
    std::getline(std::cin, acc.address);

    std::string err;
    if (g_db.createAccount(acc, initialDeposit, err)) {
        std::cout << "\n[SUCCESS] Account successfully opened! Assigned Account Number: #" << acc.account_no << "\n\n";
    } else {
        std::cout << "\n[ERROR] Failed to create account: " << err << "\n\n";
    }
}

void handleCliDeposit() {
    std::cout << "\n--- DEPOSIT FUNDS ---\n";
    std::cout << "Enter Account Number: ";
    int accNo = 0;
    std::cin >> accNo;

    std::cout << "Enter Deposit Amount (₹): ";
    double amount = 0.0;
    std::cin >> amount;

    std::cout << "Enter Remarks (or press Enter for standard): ";
    std::string remarks;
    std::getline(std::cin >> std::ws, remarks);

    Transaction tx;
    std::string err;
    if (g_db.deposit(accNo, amount, remarks, tx, err)) {
        std::cout << "\n[SUCCESS] Deposit processed! New Balance: ₹" << std::fixed << std::setprecision(2) << tx.balance_after << "\n\n";
    } else {
        std::cout << "\n[ERROR] Deposit failed: " << err << "\n\n";
    }
}

void handleCliWithdraw() {
    std::cout << "\n--- WITHDRAW FUNDS ---\n";
    std::cout << "Enter Account Number: ";
    int accNo = 0;
    std::cin >> accNo;

    std::cout << "Enter Withdrawal Amount (₹): ";
    double amount = 0.0;
    std::cin >> amount;

    std::cout << "Enter Remarks (or press Enter for standard): ";
    std::string remarks;
    std::getline(std::cin >> std::ws, remarks);

    Transaction tx;
    std::string err;
    if (g_db.withdraw(accNo, amount, remarks, tx, err)) {
        std::cout << "\n[SUCCESS] Withdrawal processed! Remaining Balance: ₹" << std::fixed << std::setprecision(2) << tx.balance_after << "\n\n";
    } else {
        std::cout << "\n[ERROR] Withdrawal failed: " << err << "\n\n";
    }
}

void handleCliFreeze() {
    std::cout << "\n--- FREEZE / ACTIVATE ACCOUNT ---\n";
    std::cout << "Enter Account Number: ";
    int accNo = 0;
    std::cin >> accNo;

    std::cout << "Choose Action (1: Freeze, 2: Activate): ";
    int opt = 0;
    std::cin >> opt;

    std::string status = (opt == 1) ? "FROZEN" : "ACTIVE";
    std::cout << "Enter Reason: ";
    std::string reason;
    std::getline(std::cin >> std::ws, reason);

    std::string err;
    if (g_db.setAccountStatus(accNo, status, reason, err)) {
        std::cout << "\n[SUCCESS] Account #" << accNo << " status updated to " << status << "!\n\n";
    } else {
        std::cout << "\n[ERROR] Status update failed: " << err << "\n\n";
    }
}

void handleCliKyc() {
    std::cout << "\n--- UPDATE KYC STATUS ---\n";
    std::cout << "Enter Account Number: ";
    int accNo = 0;
    std::cin >> accNo;

    std::cout << "Select Status (1: VERIFIED, 2: PENDING, 3: REJECTED): ";
    int kChoice = 1;
    std::cin >> kChoice;
    std::string kStatus = (kChoice == 1) ? "VERIFIED" : ((kChoice == 2) ? "PENDING" : "REJECTED");

    std::cout << "Enter ID Document Type: ";
    std::string idType;
    std::getline(std::cin >> std::ws, idType);

    std::cout << "Enter ID Document Number: ";
    std::string idNumber;
    std::getline(std::cin, idNumber);

    std::cout << "Enter Address: ";
    std::string address;
    std::getline(std::cin, address);

    std::string err;
    if (g_db.updateKyc(accNo, kStatus, idType, idNumber, address, err)) {
        std::cout << "\n[SUCCESS] KYC updated for Account #" << accNo << " to " << kStatus << "!\n\n";
    } else {
        std::cout << "\n[ERROR] KYC update failed: " << err << "\n\n";
    }
}

int main(int argc, char* argv[]) {
    int port = 8080;
    if (argc > 1) {
        try {
            port = std::stoi(argv[1]);
        } catch (...) {}
    }

    std::cout << "[SYSTEM] Initializing Apex Banking Core..." << std::endl;

    if (!g_db.init("bank.db")) {
        std::cerr << "[FATAL] Failed to initialize SQLite database. Exiting." << std::endl;
        return 1;
    }

    HttpServer server(port, "web");
    if (!server.start()) {
        std::cerr << "[FATAL] Failed to start HTTP Server on port " << port << ". Exiting." << std::endl;
        return 1;
    }

    printBanner(port);

    bool running = true;
    while (running) {
        std::cout << ">> Select Option:\n";
        std::cout << "   [1] View All Accounts\n";
        std::cout << "   [2] Create Account\n";
        std::cout << "   [3] Deposit Money\n";
        std::cout << "   [4] Withdraw Money\n";
        std::cout << "   [5] Freeze / Activate Account\n";
        std::cout << "   [6] Update KYC Status\n";
        std::cout << "   [7] Open Web App in Browser\n";
        std::cout << "   [8] Keep Server Running in Background (Web Only)\n";
        std::cout << "   [9] Exit Application\n";
        std::cout << ">> Choice (1-9): ";

        int choice = 0;
        if (!(std::cin >> choice)) {
            std::cin.clear();
            std::string dummy;
            std::getline(std::cin, dummy);
            continue;
        }

        switch (choice) {
            case 1: handleCliViewAccounts(); break;
            case 2: handleCliCreateAccount(); break;
            case 3: handleCliDeposit(); break;
            case 4: handleCliWithdraw(); break;
            case 5: handleCliFreeze(); break;
            case 6: handleCliKyc(); break;
            case 7: openBrowser(port); break;
            case 8:
                std::cout << "\n[INFO] Server is running at http://localhost:" << port << "\n";
                std::cout << "Press [Enter] anytime to return to the interactive console menu...\n";
                std::cin.ignore();
                std::cin.get();
                break;
            case 9:
                std::cout << "\n[SHUTDOWN] Stopping server and closing database...\n";
                running = false;
                break;
            default:
                std::cout << "[!] Invalid option. Please choose 1 - 9.\n";
                break;
        }
    }

    server.stop();
    g_db.close();
    std::cout << "[DONE] Apex Banking Application closed successfully. Goodbye!\n";
    return 0;
}

