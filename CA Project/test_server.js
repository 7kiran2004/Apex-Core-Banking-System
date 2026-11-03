const { spawn } = require('child_process');
const http = require('http');

console.log('--- Starting Full Apex Banking Server Verification (Node.js) ---');

const PORT = 8088;
const BASE_URL = `http://localhost:${PORT}`;

// Launch server process
const serverProcess = spawn('.\\bank_server.exe', [PORT.toString()], {
    stdio: 'ignore'
});

function request(method, path, body = null) {
    return new Promise((resolve, reject) => {
        const url = new URL(`${BASE_URL}${path}`);
        const options = {
            hostname: url.hostname,
            port: url.port,
            path: url.pathname + url.search,
            method: method,
            headers: {}
        };

        let postData = null;
        if (body) {
            postData = JSON.stringify(body);
            options.headers['Content-Type'] = 'application/json';
            options.headers['Content-Length'] = Buffer.byteLength(postData);
        }

        const req = http.request(options, (res) => {
            let data = '';
            res.on('data', chunk => data += chunk);
            res.on('end', () => {
                let parsed = null;
                try {
                    parsed = JSON.parse(data);
                } catch (e) {
                    parsed = data;
                }
                resolve({ status: res.statusCode, data: parsed, raw: data });
            });
        });

        req.on('error', reject);
        if (postData) req.write(postData);
        req.end();
    });
}

function sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

async function runTests() {
    try {
        await sleep(1500);

        // 1. Static Root
        const resRoot = await request('GET', '/');
        console.log(`[1] Static / served: Status ${resRoot.status}, Length: ${resRoot.raw.length} bytes`);
        if (!resRoot.raw.includes('<title>Apex Banking System')) throw new Error('Missing title');

        // 2. Static CSS
        const resCss = await request('GET', '/styles.css');
        console.log(`[2] Static /styles.css served: Status ${resCss.status}, Length: ${resCss.raw.length} bytes`);
        if (!resCss.raw.includes(':root')) throw new Error('Missing CSS root variables');

        // 3. Static JS
        const resJs = await request('GET', '/app.js');
        console.log(`[3] Static /app.js served: Status ${resJs.status}, Length: ${resJs.raw.length} bytes`);
        if (!resJs.raw.includes('Apex Core Banking')) throw new Error('Missing JS identifier');

        // 4. Stats API
        const resStats = await request('GET', '/api/stats');
        console.log(`[4] GET /api/stats: Status ${resStats.status}`);
        if (!resStats.data.success) throw new Error('Stats API returned false');

        // 5. Create Account
        const newAcc = {
            holder_name: "Samantha Wright",
            account_type: "Savings",
            email: "samantha.w@example.com",
            phone: "+1-800-444-9988",
            initial_deposit: 1500.0,
            id_type: "Passport",
            id_number: "PASS-SAM-887",
            address: "404 Wall Street, NY",
            kyc_status: "VERIFIED"
        };
        const resCreate = await request('POST', '/api/accounts', newAcc);
        console.log(`[5] POST /api/accounts: Status ${resCreate.status}`);
        if (!resCreate.data.success) throw new Error('Create account failed');
        const accNo = resCreate.data.data.account_no;
        console.log(`    --> Created Account #${accNo} for ${resCreate.data.data.holder_name}`);

        // 6. Deposit
        const resDep = await request('POST', '/api/deposit', { account_no: accNo, amount: 500.0, remarks: "Bonus deposit" });
        console.log(`[6] POST /api/deposit: Status ${resDep.status}`);
        if (!resDep.data.success || resDep.data.data.balance_after !== 2000.0) throw new Error('Deposit failed');
        console.log(`    --> Deposited $500.00, New Balance: $${resDep.data.data.balance_after}`);

        // 7. Withdraw
        const resWith = await request('POST', '/api/withdraw', { account_no: accNo, amount: 300.0, remarks: "ATM Cash" });
        console.log(`[7] POST /api/withdraw: Status ${resWith.status}`);
        if (!resWith.data.success || resWith.data.data.balance_after !== 1700.0) throw new Error('Withdraw failed');
        console.log(`    --> Withdrew $300.00, Remaining Balance: $${resWith.data.data.balance_after}`);

        // 8. Intra-bank Transfer to #10010001
        const resTr = await request('POST', '/api/transfer', { from_account: accNo, to_account: 10010001, amount: 200.0, remarks: "Peer payment" });
        console.log(`[8] POST /api/transfer: Status ${resTr.status}`);
        if (!resTr.data.success || resTr.data.data.balance_after !== 1500.0) throw new Error('Transfer failed');
        console.log(`    --> Transferred $200.00 to #10010001. Remaining: $${resTr.data.data.balance_after}`);

        // 9. Freeze Account
        const resFrz = await request('POST', '/api/freeze', { account_no: accNo, status: "FROZEN", reason: "Customer security notice" });
        console.log(`[9] POST /api/freeze (FROZEN): Status ${resFrz.status}`);
        if (!resFrz.data.success) throw new Error('Freeze failed');

        // 10. Attempt Withdraw on Frozen Account (Expect 400)
        const resBadWith = await request('POST', '/api/withdraw', { account_no: accNo, amount: 50.0, remarks: "Should fail" });
        console.log(`[10] POST /api/withdraw on FROZEN account (expected 400): Status ${resBadWith.status}`);
        if (resBadWith.status !== 400) throw new Error('Expected 400 on frozen account withdrawal');
        console.log(`    --> Blocked withdrawal on frozen account as expected: ${resBadWith.data.error}`);

        // 11. Unfreeze / Activate
        const resAct = await request('POST', '/api/freeze', { account_no: accNo, status: "ACTIVE", reason: "Customer verified" });
        console.log(`[11] POST /api/freeze (ACTIVE): Status ${resAct.status}`);
        if (!resAct.data.success) throw new Error('Unfreeze failed');

        // 12. KYC Update
        const resKyc = await request('POST', '/api/kyc', { account_no: accNo, kyc_status: "VERIFIED", id_type: "National ID", id_number: "NID-998877", address: "77 Sunset Blvd" });
        console.log(`[12] POST /api/kyc: Status ${resKyc.status}`);
        if (!resKyc.data.success) throw new Error('KYC update failed');

        // 13. Transactions Query
        const resTxs = await request('GET', `/api/transactions?account_no=${accNo}`);
        console.log(`[13] GET /api/transactions: Status ${resTxs.status}, count: ${resTxs.data.data.length}`);
        if (!resTxs.data.success || resTxs.data.data.length < 4) throw new Error('Transaction query failed');

        // 14. Audit Query
        const resLogs = await request('GET', `/api/audit?account_no=${accNo}`);
        console.log(`[14] GET /api/audit: Status ${resLogs.status}, count: ${resLogs.data.data.length}`);
        if (!resLogs.data.success || resLogs.data.data.length < 5) throw new Error('Audit query failed');

        // 15. Delete Account with balance > 0 (Expect 400)
        const resDelBad = await request('POST', '/api/delete', { account_no: accNo, permanent: false });
        console.log(`[15] POST /api/delete with balance > 0 (expected 400): Status ${resDelBad.status}`);
        if (resDelBad.status !== 400) throw new Error('Expected 400 on non-zero balance closure');
        console.log(`    --> Prevented premature deletion: ${resDelBad.data.error}`);

        // 16. Empty balance and Close Account
        await request('POST', '/api/withdraw', { account_no: accNo, amount: 1500.0, remarks: "Zero out balance" });
        const resDelOk = await request('POST', '/api/delete', { account_no: accNo, permanent: false });
        console.log(`[16] POST /api/delete with $0.00 balance: Status ${resDelOk.status}`);
        if (!resDelOk.data.success) throw new Error('Final deletion failed');
        console.log(`    --> Account successfully closed and archived!`);

        console.log('\n=======================================================');
        console.log('   ALL 16 BACKEND & FRONTEND TESTS PASSED 100%!       ');
        console.log('=======================================================\n');

    } catch (err) {
        console.error('[TEST FAILURE]', err);
        process.exitCode = 1;
    } finally {
        serverProcess.kill();
        // Give time to release port
        await sleep(500);
        console.log('Test server process terminated.');
    }
}

runTests();

