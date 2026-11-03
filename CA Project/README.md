# Apex Core Banking System (C++ & SQLite Core)

A full-stack, enterprise-grade **Banking Application** engineered with a high-performance **C++ backend**, embedded **SQLite3 database**, and a modern, responsive **HTML5 / CSS3 / JavaScript frontend**.

---

## 🌟 Key Highlights & Architecture

- **Backend**: C++11 (`database.cpp`, `http_server.cpp`, `main.cpp`)
- **Database**: Local SQL Database (`bank.db`) using the SQLite3 C/C++ Engine (ACID compliant with Write-Ahead Logging `WAL`)
- **Networking**: Multi-threaded HTTP/1.1 REST Server built with native Windows Sockets (`Winsock2` / `ws2_32`)
- **Frontend**: Modern FinTech Single-Page Web Portal (`HTML5`, `CSS3`, `Vanilla JavaScript`)
- **Dual Interface**: Operate seamlessly via **Modern Web Browser GUI** or **Interactive Terminal Console CLI**!

```
┌────────────────────────────────────────────────────────┐
│           Apex Modern Web Banking Portal               │
│          (HTML5 / CSS3 Glassmorphism / JS)             │
└──────────────────────────┬─────────────────────────────┘
                           │ HTTP REST (JSON)
                           ▼
┌────────────────────────────────────────────────────────┐
│              C++ Winsock HTTP REST Server              │
│       Multi-threaded request routing & JSON parser     │
└──────────────────────────┬─────────────────────────────┘
                           │ C++ SQLite API
                           ▼
┌────────────────────────────────────────────────────────┐
│         Local SQL Database Engine (bank.db)            │
│   [accounts]        [transactions]       [audit_logs]  │
└────────────────────────────────────────────────────────┘
```

---

## 🚀 Core Banking Features Implemented

### 1. Account Opening & Onboarding
- Generate unique 8-digit account numbers (e.g. `#10010001`).
- Support for multiple account types: **Savings (4.5% APY)**, **Current / Checking**, and **Fixed Deposit (7.2% APY)**.
- Initial deposit support with immediate ledger credit.
- Initial identity binding (Passport, National ID, Driver's License, SSN).

### 2. Deposit Funds
- Credit funds to any existing active account.
- Instant balance calculation and ledger record.
- Quick chips for common amounts (₹50, ₹100, ₹500, ₹1,000, ₹5,000).
- Automatic generation of an official printable **Transaction Receipt / Advice**.

### 3. Withdraw Funds
- Real-time balance check to prevent overdrafts.
- **Freeze Protection**: Prohibits withdrawals if the account is frozen.
- Closed account safeguard: Blocks any debit on terminated accounts.
- Generates official withdrawal receipt with closing balance.

### 4. Intra-Bank Fund Transfer
- Atomic C++ database transaction (`BEGIN TRANSACTION ... COMMIT`).
- Debits sender account, credits recipient account, and creates corresponding `TRANSFER_OUT` and `TRANSFER_IN` audit records.
- Automatic rollback if either leg fails.

### 5. Freeze / Activate Account
- Administrative toggle between **ACTIVE** and **FROZEN** status.
- Prevents withdrawals and deposits while frozen.
- Comprehensive security audit log with timestamps and reason strings.

### 6. KYC (Know Your Customer) Compliance Update
- Verification lifecycle: `PENDING` ➔ `VERIFIED` ➔ `REJECTED`.
- Update customer ID document type (Passport, National ID, Driver's License, Tax ID).
- Update ID number and registered residential address.

### 7. Close / Delete Account
- **Banking Compliance Rule**: Strict check requiring `₹0.00` remaining balance prior to account closure.
- **Soft Delete / Archive**: Marks account as `CLOSED`, disabling transactions while retaining full financial history for audit compliance.
- **Permanent Purge**: Optional administrative purge from SQLite.

### 8. Passbook & Transaction Ledger
- Full historical statement view with account filtering.
- One-click **Export to CSV** for spreadsheets and external audits.
- Printable electronic transaction advice.

### 9. Built-in Financial Calculators
- **Compound Interest Calculator**: Forecasts savings growth with monthly compounding.
- **Loan EMI Simulator**: Calculates monthly installments and total repayment amount.

---

## 📂 Project Structure

```
CA Project/
├── bank_server.exe       # Pre-compiled standalone C++ executable
├── database.h            # Database interface definitions
├── database.cpp          # SQLite queries, ACID transactions, and thread locks
├── http_server.h         # Winsock HTTP server interface & request models
├── http_server.cpp       # Winsock socket listener, MIME routing, and API handlers
├── json_utils.h          # Zero-dependency C++ JSON parser & serializer
├── models.h              # Data models (Account, Transaction, AuditLog, BankStats)
├── main.cpp              # Entry point: Server launcher & Interactive CLI
├── sqlite3.c             # SQLite 3.45.2 C amalgamation engine
├── sqlite3.h             # SQLite header
├── sqlite3.o             # Pre-compiled SQLite object file
├── bank.db               # Embedded local SQLite database (auto-created)
├── build.bat             # Automated batch compilation script using MinGW
├── run.bat               # Starts server and opens browser to http://localhost:8080
├── test_server.py        # Automated 16-step integration test suite
└── web/                  # Web frontend assets
    ├── index.html        # Banking Dashboard, Forms, and Modals
    ├── styles.css        # Premium Dark FinTech theme & responsive layout
    └── app.js            # Client-side API controller & reactive UI
```

---

## 🛠️ How to Build and Run

### Quick Start (Recommended)
Simply double-click `run.bat` or run:
```cmd
.\run.bat
```
This starts the C++ server and automatically opens your default browser at **`http://localhost:8080/`**.

---

### Manual Compilation
To compile from source using MinGW GCC/G++:
```cmd
.\build.bat
```
Or execute the compilation commands directly in PowerShell / Command Prompt:
```cmd
g++ -std=c++11 -O2 -c database.cpp -o database.o
g++ -std=c++11 -O2 -c http_server.cpp -o http_server.o
g++ -std=c++11 -O2 main.cpp database.o http_server.o sqlite3.o -o bank_server.exe -lws2_32
```
Then launch the server:
```cmd
.\bank_server.exe 8080
```

---

## 🧪 Automated Integration Tests
The repository includes an end-to-end Python test suite verifying all 16 endpoints and business rules:
```cmd
python test_server.py
```
**Test Coverage**:
1. Static asset delivery (`/`, `/styles.css`, `/app.js`)
2. Bank reserve statistics API
3. Account creation with initial deposit
4. Cash deposit & balance verification
5. Cash withdrawal & balance verification
6. Intra-bank fund transfer
7. Account freezing
8. Withdrawal rejection on frozen account (Expected 400 Bad Request)
9. Account unfreezing / re-activation
10. KYC document & address updates
11. Transaction ledger retrieval
12. Audit trail logging
13. Prevention of account deletion with non-zero balance
14. Successful closure with zero balance

---

## 🔌 REST API Reference

| Method | Endpoint | Description |
|---|---|---|
| `GET` | `/api/stats` | Bank reserves, total accounts, active/frozen count |
| `GET` | `/api/accounts` | Query accounts with optional `?q=`, `?status=`, `?kyc=` |
| `GET` | `/api/accounts/:id` | Fetch specific account details |
| `POST` | `/api/accounts` | Open account with personal details & initial deposit |
| `POST` | `/api/deposit` | Deposit funds (`account_no`, `amount`, `remarks`) |
| `POST` | `/api/withdraw` | Withdraw funds (`account_no`, `amount`, `remarks`) |
| `POST` | `/api/transfer` | Intra-bank transfer (`from_account`, `to_account`, `amount`) |
| `POST` | `/api/freeze` | Update status (`account_no`, `status`: ACTIVE/FROZEN, `reason`) |
| `POST` | `/api/kyc` | Update KYC (`account_no`, `kyc_status`, `id_type`, `id_number`, `address`) |
| `POST` | `/api/delete` | Close or purge account (`account_no`, `permanent`: bool) |
| `GET` | `/api/transactions` | Query ledger entries with optional `?account_no=` |
| `GET` | `/api/audit` | Query security audit events |

---

## 💻 Console CLI Interface
When running `bank_server.exe` in the terminal, you can interact with the system simultaneously using the interactive menu:
```
>> Select Option:
   [1] View All Accounts
   [2] Create Account
   [3] Deposit Money
   [4] Withdraw Money
   [5] Freeze / Activate Account
   [6] Update KYC Status
   [7] Open Web App in Browser
   [8] Keep Server Running in Background (Web Only)
   [9] Exit Application
```
You can create accounts, view balances, or deposit money directly in the terminal, and watch the updates appear in real time on the web dashboard!

