import subprocess
import time
import urllib.request
import json
import os

print("--- Starting Full Apex Banking Server Verification ---", flush=True)
proc = subprocess.Popen(['.\\bank_server.exe', '8088'])
time.sleep(1.2)

base_url = "http://localhost:8088"

def get(path):
    req = urllib.request.Request(f"{base_url}{path}")
    with urllib.request.urlopen(req, timeout=5) as resp:
        return resp.status, resp.read().decode('utf-8')

def post(path, payload):
    data = json.dumps(payload).encode('utf-8')
    req = urllib.request.Request(f"{base_url}{path}", data=data, headers={'Content-Type': 'application/json'})
    try:
        with urllib.request.urlopen(req, timeout=5) as resp:
            return resp.status, resp.read().decode('utf-8')
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode('utf-8')

try:
    # 1. Test Static files
    status, html = get("/")
    print("[1] Static / served: Status", status, f"Length: {len(html)} bytes", flush=True)
    assert "<title>Apex Banking System" in html

    status, css = get("/styles.css")
    print("[2] Static /styles.css served: Status", status, f"Length: {len(css)} bytes", flush=True)
    assert ":root" in css

    status, js = get("/app.js")
    print("[3] Static /app.js served: Status", status, f"Length: {len(js)} bytes", flush=True)
    assert "Apex Core Banking" in js

    # 2. Test Stats API
    status, stats_res = get("/api/stats")
    print("[4] GET /api/stats: Status", status, flush=True)
    stats = json.loads(stats_res)
    assert stats['success'] == True

    # 3. Test Create Account
    new_acc_payload = {
        "holder_name": "Samantha Wright",
        "account_type": "Savings",
        "email": "samantha.w@example.com",
        "phone": "+1-800-444-9988",
        "initial_deposit": 1500.0,
        "id_type": "Passport",
        "id_number": "PASS-SAM-887",
        "address": "404 Wall Street, NY",
        "kyc_status": "VERIFIED"
    }
    status, create_res = post("/api/accounts", new_acc_payload)
    print("[5] POST /api/accounts: Status", status, flush=True)
    create_data = json.loads(create_res)
    assert create_data['success'] == True
    acc_no = create_data['data']['account_no']
    print(f"    --> Created Account #{acc_no} for {create_data['data']['holder_name']}", flush=True)

    # 4. Test Deposit
    status, dep_res = post("/api/deposit", {"account_no": acc_no, "amount": 500.0, "remarks": "Bonus deposit"})
    print("[6] POST /api/deposit: Status", status, flush=True)
    dep_data = json.loads(dep_res)
    assert dep_data['success'] == True
    assert dep_data['data']['balance_after'] == 2000.0
    print(f"    --> Deposited ₹500.00, New Balance: ₹{dep_data['data']['balance_after']}", flush=True)

    # 5. Test Withdraw
    status, with_res = post("/api/withdraw", {"account_no": acc_no, "amount": 300.0, "remarks": "ATM Cash"})
    print("[7] POST /api/withdraw: Status", status, flush=True)
    with_data = json.loads(with_res)
    assert with_data['success'] == True
    assert with_data['data']['balance_after'] == 1700.0
    print(f"    --> Withdrew ₹300.00, Remaining Balance: ₹{with_data['data']['balance_after']}", flush=True)

    # 6. Test Fund Transfer to seeded account 10010001
    status, tr_res = post("/api/transfer", {"from_account": acc_no, "to_account": 10010001, "amount": 200.0, "remarks": "Peer payment"})
    print("[8] POST /api/transfer: Status", status, flush=True)
    tr_data = json.loads(tr_res)
    assert tr_data['success'] == True
    assert tr_data['data']['balance_after'] == 1500.0
    print(f"    --> Transferred ₹200.00 to #10010001. Remaining: ₹{tr_data['data']['balance_after']}", flush=True)

    # 7. Test Freeze Account
    status, frz_res = post("/api/freeze", {"account_no": acc_no, "status": "FROZEN", "reason": "Customer travel security notice"})
    print("[9] POST /api/freeze (FROZEN): Status", status, flush=True)
    assert json.loads(frz_res)['success'] == True

    # 8. Test Withdraw on Frozen Account (Should Fail with 400!)
    status, bad_with = post("/api/withdraw", {"account_no": acc_no, "amount": 50.0, "remarks": "Should fail"})
    print("[10] POST /api/withdraw on FROZEN account (expected 400): Status", status, flush=True)
    assert status == 400
    print("    --> Blocked withdrawal on frozen account as expected:", json.loads(bad_with)['error'], flush=True)

    # 9. Test Unfreeze / Activate Account
    status, act_res = post("/api/freeze", {"account_no": acc_no, "status": "ACTIVE", "reason": "Customer verified via phone"})
    print("[11] POST /api/freeze (ACTIVE): Status", status, flush=True)
    assert json.loads(act_res)['success'] == True

    # 10. Test KYC Update
    status, kyc_res = post("/api/kyc", {"account_no": acc_no, "kyc_status": "VERIFIED", "id_type": "National ID", "id_number": "NID-998877", "address": "77 Sunset Blvd"})
    print("[12] POST /api/kyc: Status", status, flush=True)
    assert json.loads(kyc_res)['success'] == True

    # 11. Test Transactions and Audit queries
    status, txs = get(f"/api/transactions?account_no={acc_no}")
    tx_list = json.loads(txs)['data']
    print("[13] GET /api/transactions: Status", status, f"Account #{acc_no} has {len(tx_list)} transactions", flush=True)
    assert len(tx_list) >= 4

    status, logs = get(f"/api/audit?account_no={acc_no}")
    log_list = json.loads(logs)['data']
    print("[14] GET /api/audit: Status", status, f"Account #{acc_no} has {len(log_list)} audit logs", flush=True)
    assert len(log_list) >= 5

    # 12. Test Delete Account with non-zero balance (Should fail!)
    status, del_fail = post("/api/delete", {"account_no": acc_no, "permanent": False})
    print("[15] POST /api/delete with balance > 0 (expected 400): Status", status, flush=True)
    assert status == 400
    print("    --> Prevented premature deletion:", json.loads(del_fail)['error'], flush=True)

    # Zero out balance before closing
    post("/api/withdraw", {"account_no": acc_no, "amount": 1500.0, "remarks": "Zero out before closing"})
    status, del_ok = post("/api/delete", {"account_no": acc_no, "permanent": False})
    print("[16] POST /api/delete with ₹0.00 balance: Status", status, flush=True)
    assert json.loads(del_ok)['success'] == True
    print("    --> Account safely closed and archived in SQLite!", flush=True)

    print("\n=======================================================", flush=True)
    print("   ALL 16 BACKEND & FRONTEND TESTS PASSED 100%!       ", flush=True)
    print("=======================================================\n", flush=True)

finally:
    proc.terminate()
    proc.wait()
    print("Test server terminated cleanly.", flush=True)
