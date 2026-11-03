// Apex Core Banking Web Application
// Client-side controller interfacing with C++ SQLite Server

const API_BASE = window.location.origin.includes('http') ? window.location.origin : 'http://localhost:8080';

// State
let allAccountsCache = [];
let lastTransactionsCache = [];

// Initialize application on DOM ready
document.addEventListener('DOMContentLoaded', () => {
    initClock();
    initNavigation();
    initFormHandlers();
    initFilters();
    refreshAllData();

    // Auto-refresh stats every 15 seconds
    setInterval(loadDashboardStats, 15000);
});

// CLOCK
function initClock() {
    const clockEl = document.getElementById('live-clock');
    const updateTime = () => {
        const now = new Date();
        clockEl.textContent = now.toLocaleTimeString();
    };
    updateTime();
    setInterval(updateTime, 1000);
}

// TAB NAVIGATION
function initNavigation() {
    const navItems = document.querySelectorAll('.nav-item');
    navItems.forEach(item => {
        item.addEventListener('click', () => {
            const targetTab = item.dataset.tab;
            showTab(targetTab);
        });
    });

    document.getElementById('btn-refresh-all').addEventListener('click', () => {
        showToast('Refreshing banking data from SQLite...', 'info');
        refreshAllData();
    });
}

const tabMeta = {
    'dashboard': { title: 'Dashboard', sub: 'Real-time banking metrics and transactional oversight' },
    'accounts': { title: 'Account Directory', sub: 'Customer registry with KYC and balance tracking' },
    'create-account': { title: 'Open New Account', sub: 'Complete customer onboarding and initial KYC identity binding' },
    'deposit': { title: 'Deposit Funds', sub: 'Credit funds into an existing active account' },
    'withdraw': { title: 'Withdraw Funds', sub: 'Safely debit money with instant balance and freeze protection' },
    'transfer': { title: 'Fund Transfer', sub: 'Atomic intra-bank transfer with transaction guarantees' },
    'freeze': { title: 'Freeze / Activate Account', sub: 'Manage account security state and toggle operational locks' },
    'kyc': { title: 'KYC Compliance Verification', sub: 'Customer identity verification and compliance management' },
    'delete-acc': { title: 'Close or Delete Account', sub: 'Terminate account and archive database records' },
    'passbook': { title: 'Ledger & Passbook', sub: 'Complete immutable transaction ledger' },
    'calculator': { title: 'Financial Calculators', sub: 'Simulate compound interest returns and loan EMIs' }
};

function showTab(tabId) {
    document.querySelectorAll('.tab-pane').forEach(el => el.classList.remove('active'));
    document.querySelectorAll('.nav-item').forEach(el => el.classList.remove('active'));

    const targetPane = document.getElementById(`tab-${tabId}`);
    const targetNav = document.querySelector(`.nav-item[data-tab="${tabId}"]`);

    if (targetPane) targetPane.classList.add('active');
    if (targetNav) targetNav.classList.add('active');

    // Update Header
    if (tabMeta[tabId]) {
        document.getElementById('page-title').textContent = tabMeta[tabId].title;
        document.getElementById('page-subtitle').textContent = tabMeta[tabId].sub;
    }

    // Refresh specific tab contents if needed
    if (tabId === 'accounts') loadAccounts();
    if (tabId === 'passbook') loadPassbook();
    if (tabId === 'dashboard') refreshAllData();
}

// DATA LOADERS
async function refreshAllData() {
    await Promise.all([
        loadDashboardStats(),
        loadRecentTransactions(),
        loadRecentAuditLogs(),
        loadAccounts()
    ]);
}

async function loadDashboardStats() {
    try {
        const res = await fetch(`${API_BASE}/api/stats`);
        const data = await res.json();
        if (data.success) {
            const s = data.data;
            document.getElementById('stat-reserve').textContent = formatCurrency(s.total_reserve);
            document.getElementById('stat-total-accounts').textContent = s.total_accounts;
            document.getElementById('stat-active-accounts').textContent = `${s.active_accounts} Active`;
            document.getElementById('stat-frozen-accounts').textContent = `${s.frozen_accounts} Frozen`;
            document.getElementById('stat-kyc-verified').textContent = s.kyc_verified;
            document.getElementById('stat-kyc-pending').textContent = `${s.kyc_pending} Pending`;
            document.getElementById('stat-total-tx').textContent = s.total_transactions;
        }
    } catch (err) {
        console.error('Error loading stats:', err);
    }
}

async function loadRecentTransactions() {
    try {
        const res = await fetch(`${API_BASE}/api/transactions?limit=8`);
        const data = await res.json();
        const tbody = document.getElementById('dash-tx-table-body');
        if (!data.success || !data.data || data.data.length === 0) {
            tbody.innerHTML = '<tr><td colspan="7" class="text-center py-4 text-muted">No transactions found</td></tr>';
            return;
        }

        lastTransactionsCache = data.data;
        tbody.innerHTML = data.data.map(tx => `
            <tr>
                <td class="font-mono">#TX-${tx.tx_id}</td>
                <td><span class="font-mono">#${tx.account_no}</span></td>
                <td>${getTypeBadge(tx.type)}</td>
                <td class="font-mono ${tx.type.includes('WITHDRAW') || tx.type === 'TRANSFER_OUT' ? 'text-danger' : 'text-success'}">
                    ${tx.type.includes('WITHDRAW') || tx.type === 'TRANSFER_OUT' ? '-' : '+'}${formatCurrency(tx.amount)}
                </td>
                <td class="font-mono">${formatCurrency(tx.balance_after)}</td>
                <td class="text-muted font-mono" style="font-size:0.75rem">${tx.timestamp}</td>
                <td>
                    <button class="btn btn-sm btn-outline" onclick="openReceiptModalById(${tx.tx_id})">Advice</button>
                </td>
            </tr>
        `).join('');
    } catch (err) {
        console.error('Error loading recent transactions:', err);
    }
}

async function loadRecentAuditLogs() {
    try {
        const res = await fetch(`${API_BASE}/api/audit?limit=10`);
        const data = await res.json();
        const container = document.getElementById('dash-audit-list');
        if (!data.success || !data.data || data.data.length === 0) {
            container.innerHTML = '<div class="text-center py-4 text-muted">No audit logs yet.</div>';
            return;
        }

        container.innerHTML = data.data.map(l => `
            <div class="audit-item">
                <span class="audit-action-badge">${l.action}</span>
                <div class="audit-content">
                    <div class="audit-details"><strong>#${l.account_no}</strong>: ${escapeHtml(l.details)}</div>
                    <div class="audit-time">${l.timestamp}</div>
                </div>
            </div>
        `).join('');
    } catch (err) {
        console.error('Error loading audit logs:', err);
    }
}

async function loadAccounts() {
    try {
        const search = document.getElementById('acc-search-input')?.value || '';
        const status = document.getElementById('acc-filter-status')?.value || 'ALL';
        const kyc = document.getElementById('acc-filter-kyc')?.value || 'ALL';

        const queryParams = new URLSearchParams();
        if (search) queryParams.set('q', search);
        if (status && status !== 'ALL') queryParams.set('status', status);
        if (kyc && kyc !== 'ALL') queryParams.set('kyc', kyc);

        const res = await fetch(`${API_BASE}/api/accounts?${queryParams.toString()}`);
        const data = await res.json();
        const tbody = document.getElementById('accounts-table-body');

        if (!data.success || !data.data || data.data.length === 0) {
            tbody.innerHTML = '<tr><td colspan="8" class="text-center py-4 text-muted">No accounts match the criteria</td></tr>';
            allAccountsCache = [];
            return;
        }

        allAccountsCache = data.data;
        tbody.innerHTML = data.data.map(acc => `
            <tr>
                <td class="font-mono font-bold">#${acc.account_no}</td>
                <td>
                    <strong>${escapeHtml(acc.holder_name)}</strong>
                    <div class="text-muted" style="font-size:0.75rem">${escapeHtml(acc.email || 'No email')}</div>
                </td>
                <td><span class="chip" style="font-size:0.7rem">${acc.account_type}</span></td>
                <td class="font-mono font-bold">${formatCurrency(acc.balance)}</td>
                <td>${getStatusBadge(acc.status)}</td>
                <td>${getKycBadge(acc.kyc_status)}</td>
                <td class="font-mono" style="font-size:0.8rem">${escapeHtml(acc.phone || '-')}</td>
                <td>
                    <div style="display:flex; gap:6px;">
                        <button class="btn btn-sm btn-secondary" onclick="inspectAccount(${acc.account_no})">Details</button>
                        <button class="btn btn-sm btn-outline" onclick="quickSelectAccount(${acc.account_no}, 'deposit')">Deposit</button>
                        <button class="btn btn-sm btn-outline" onclick="quickSelectAccount(${acc.account_no}, 'withdraw')">Withdraw</button>
                    </div>
                </td>
            </tr>
        `).join('');
    } catch (err) {
        console.error('Error loading accounts:', err);
    }
}

async function loadPassbook() {
    try {
        const accFilter = document.getElementById('passbook-acc-filter').value;
        let url = `${API_BASE}/api/transactions?limit=150`;
        if (accFilter) url += `&account_no=${accFilter}`;

        const res = await fetch(url);
        const data = await res.json();
        const tbody = document.getElementById('passbook-table-body');

        if (!data.success || !data.data || data.data.length === 0) {
            tbody.innerHTML = '<tr><td colspan="9" class="text-center py-4 text-muted">No transactions found</td></tr>';
            return;
        }

        lastTransactionsCache = data.data;
        tbody.innerHTML = data.data.map(tx => `
            <tr>
                <td class="font-mono">#TX-${tx.tx_id}</td>
                <td class="font-mono font-bold">#${tx.account_no}</td>
                <td>${getTypeBadge(tx.type)}</td>
                <td class="font-mono ${tx.type.includes('WITHDRAW') || tx.type === 'TRANSFER_OUT' ? 'text-danger' : 'text-success'}">
                    ${tx.type.includes('WITHDRAW') || tx.type === 'TRANSFER_OUT' ? '-' : '+'}${formatCurrency(tx.amount)}
                </td>
                <td class="font-mono font-bold">${formatCurrency(tx.balance_after)}</td>
                <td class="font-mono text-muted">${tx.target_account ? '#' + tx.target_account : '-'}</td>
                <td style="max-width:200px; overflow:hidden; text-overflow:ellipsis; white-space:nowrap;">${escapeHtml(tx.remarks || '')}</td>
                <td class="font-mono text-muted" style="font-size:0.75rem">${tx.timestamp}</td>
                <td>
                    <button class="btn btn-sm btn-outline" onclick="openReceiptModalById(${tx.tx_id})">Receipt</button>
                </td>
            </tr>
        `).join('');
    } catch (err) {
        console.error('Error loading passbook:', err);
    }
}

// QUICK SELECT ACCOUNT INTO OPERATIONS
function quickSelectAccount(accNo, operation) {
    showTab(operation);
    if (operation === 'deposit') {
        document.getElementById('deposit-acc-no').value = accNo;
        lookupAccountForOperation('deposit');
    } else if (operation === 'withdraw') {
        document.getElementById('withdraw-acc-no').value = accNo;
        lookupAccountForOperation('withdraw');
    }
}

// ACCOUNT LOOKUP UTILITY FOR FORMS
async function lookupAccountForOperation(op) {
    let accNoInputId = '';
    let badgeId = '';

    if (op === 'deposit') {
        accNoInputId = 'deposit-acc-no';
        badgeId = 'deposit-lookup-info';
    } else if (op === 'withdraw') {
        accNoInputId = 'withdraw-acc-no';
        badgeId = 'withdraw-lookup-info';
    } else if (op === 'transfer-from') {
        accNoInputId = 'transfer-from-acc';
        badgeId = 'transfer-from-info';
    } else if (op === 'transfer-to') {
        accNoInputId = 'transfer-to-acc';
        badgeId = 'transfer-to-info';
    } else if (op === 'freeze') {
        accNoInputId = 'freeze-acc-no';
        badgeId = 'freeze-lookup-info';
    } else if (op === 'kyc') {
        accNoInputId = 'kyc-acc-no';
        badgeId = 'kyc-lookup-info';
    } else if (op === 'delete') {
        accNoInputId = 'delete-acc-no';
        badgeId = 'delete-lookup-info';
    }

    const input = document.getElementById(accNoInputId);
    const badge = document.getElementById(badgeId);
    if (!input || !badge) return;

    const accNo = parseInt(input.value);
    if (!accNo) {
        badge.className = 'account-lookup-badge error';
        badge.textContent = 'Please enter an account number.';
        badge.style.display = 'block';
        return;
    }

    try {
        const res = await fetch(`${API_BASE}/api/accounts/${accNo}`);
        const data = await res.json();
        if (data.success) {
            const acc = data.data;
            badge.className = 'account-lookup-badge success';
            badge.innerHTML = `<strong>${escapeHtml(acc.holder_name)}</strong> &bull; Type: ${acc.account_type} &bull; Balance: <strong>${formatCurrency(acc.balance)}</strong> &bull; Status: <strong>${acc.status}</strong> &bull; KYC: <strong>${acc.kyc_status}</strong>`;
            badge.style.display = 'block';

            // Pre-fill KYC if in KYC tab
            if (op === 'kyc') {
                document.getElementById('kyc-new-status').value = acc.kyc_status || 'VERIFIED';
                document.getElementById('kyc-id-type').value = acc.id_type || 'Passport';
                document.getElementById('kyc-id-number').value = acc.id_number || '';
                document.getElementById('kyc-address').value = acc.address || '';
            }
        } else {
            badge.className = 'account-lookup-badge error';
            badge.textContent = data.error || 'Account not found.';
            badge.style.display = 'block';
        }
    } catch (err) {
        badge.className = 'account-lookup-badge error';
        badge.textContent = 'Failed to verify account from server.';
        badge.style.display = 'block';
    }
}

// FORM SUBMISSIONS
function initFormHandlers() {
    // 1. Create Account
    const createForm = document.getElementById('form-create-account');
    createForm.addEventListener('submit', async(e) => {
        e.preventDefault();
        const payload = {
            holder_name: document.getElementById('create-name').value.trim(),
            email: document.getElementById('create-email').value.trim(),
            phone: document.getElementById('create-phone').value.trim(),
            account_type: document.getElementById('create-type').value,
            initial_deposit: parseFloat(document.getElementById('create-deposit').value) || 0.0,
            id_type: document.getElementById('create-id-type').value,
            id_number: document.getElementById('create-id-number').value.trim(),
            address: document.getElementById('create-address').value.trim(),
            kyc_status: document.getElementById('create-kyc-status').value
        };

        const btn = document.getElementById('btn-submit-create');
        btn.disabled = true;

        try {
            const res = await fetch(`${API_BASE}/api/accounts`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            });
            const data = await res.json();
            if (data.success) {
                showToast(`Success! Account #${data.data.account_no} opened for ${data.data.holder_name}`, 'success');
                createForm.reset();
                refreshAllData();
                showTab('accounts');
            } else {
                showToast(data.error || 'Failed to create account.', 'error');
            }
        } catch (err) {
            showToast('Network error connecting to C++ backend.', 'error');
        } finally {
            btn.disabled = false;
        }
    });

    // 2. Deposit Funds
    const depositForm = document.getElementById('form-deposit');
    depositForm.addEventListener('submit', async(e) => {
        e.preventDefault();
        const payload = {
            account_no: parseInt(document.getElementById('deposit-acc-no').value),
            amount: parseFloat(document.getElementById('deposit-amount').value),
            remarks: document.getElementById('deposit-remarks').value.trim()
        };

        const btn = document.getElementById('btn-submit-deposit');
        btn.disabled = true;

        try {
            const res = await fetch(`${API_BASE}/api/deposit`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            });
            const data = await res.json();
            if (data.success) {
                showToast(data.message, 'success');
                depositForm.reset();
                document.getElementById('deposit-lookup-info').style.display = 'none';
                refreshAllData();
                openReceiptModal(data.data);
            } else {
                showToast(data.error || 'Deposit failed.', 'error');
            }
        } catch (err) {
            showToast('Network error connecting to C++ backend.', 'error');
        } finally {
            btn.disabled = false;
        }
    });

    // 3. Withdraw Funds
    const withdrawForm = document.getElementById('form-withdraw');
    withdrawForm.addEventListener('submit', async(e) => {
        e.preventDefault();
        const payload = {
            account_no: parseInt(document.getElementById('withdraw-acc-no').value),
            amount: parseFloat(document.getElementById('withdraw-amount').value),
            remarks: document.getElementById('withdraw-remarks').value.trim()
        };

        const btn = document.getElementById('btn-submit-withdraw');
        btn.disabled = true;

        try {
            const res = await fetch(`${API_BASE}/api/withdraw`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            });
            const data = await res.json();
            if (data.success) {
                showToast(data.message, 'success');
                withdrawForm.reset();
                document.getElementById('withdraw-lookup-info').style.display = 'none';
                refreshAllData();
                openReceiptModal(data.data);
            } else {
                showToast(data.error || 'Withdrawal failed.', 'error');
            }
        } catch (err) {
            showToast('Network error connecting to C++ backend.', 'error');
        } finally {
            btn.disabled = false;
        }
    });

    // 4. Fund Transfer
    const transferForm = document.getElementById('form-transfer');
    transferForm.addEventListener('submit', async(e) => {
        e.preventDefault();
        const payload = {
            from_account: parseInt(document.getElementById('transfer-from-acc').value),
            to_account: parseInt(document.getElementById('transfer-to-acc').value),
            amount: parseFloat(document.getElementById('transfer-amount').value),
            remarks: document.getElementById('transfer-remarks').value.trim()
        };

        const btn = document.getElementById('btn-submit-transfer');
        btn.disabled = true;

        try {
            const res = await fetch(`${API_BASE}/api/transfer`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            });
            const data = await res.json();
            if (data.success) {
                showToast(data.message, 'success');
                transferForm.reset();
                document.getElementById('transfer-from-info').style.display = 'none';
                document.getElementById('transfer-to-info').style.display = 'none';
                refreshAllData();
                openReceiptModal(data.data);
            } else {
                showToast(data.error || 'Transfer failed.', 'error');
            }
        } catch (err) {
            showToast('Network error connecting to C++ backend.', 'error');
        } finally {
            btn.disabled = false;
        }
    });

    // 5. Freeze / Activate
    const freezeForm = document.getElementById('form-freeze');
    freezeForm.addEventListener('submit', async(e) => {
        e.preventDefault();
        const statusSelected = document.querySelector('input[name="freeze-action"]:checked').value;
        const payload = {
            account_no: parseInt(document.getElementById('freeze-acc-no').value),
            status: statusSelected,
            reason: document.getElementById('freeze-reason').value.trim()
        };

        const btn = document.getElementById('btn-submit-freeze');
        btn.disabled = true;

        try {
            const res = await fetch(`${API_BASE}/api/freeze`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            });
            const data = await res.json();
            if (data.success) {
                showToast(data.message, 'success');
                freezeForm.reset();
                document.getElementById('freeze-lookup-info').style.display = 'none';
                refreshAllData();
            } else {
                showToast(data.error || 'Operation failed.', 'error');
            }
        } catch (err) {
            showToast('Network error connecting to C++ backend.', 'error');
        } finally {
            btn.disabled = false;
        }
    });

    // 6. KYC Update
    const kycForm = document.getElementById('form-kyc');
    kycForm.addEventListener('submit', async(e) => {
        e.preventDefault();
        const payload = {
            account_no: parseInt(document.getElementById('kyc-acc-no').value),
            kyc_status: document.getElementById('kyc-new-status').value,
            id_type: document.getElementById('kyc-id-type').value,
            id_number: document.getElementById('kyc-id-number').value.trim(),
            address: document.getElementById('kyc-address').value.trim()
        };

        const btn = document.getElementById('btn-submit-kyc');
        btn.disabled = true;

        try {
            const res = await fetch(`${API_BASE}/api/kyc`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            });
            const data = await res.json();
            if (data.success) {
                showToast(data.message, 'success');
                kycForm.reset();
                document.getElementById('kyc-lookup-info').style.display = 'none';
                refreshAllData();
            } else {
                showToast(data.error || 'KYC update failed.', 'error');
            }
        } catch (err) {
            showToast('Network error connecting to C++ backend.', 'error');
        } finally {
            btn.disabled = false;
        }
    });

    // 7. Delete Account
    const deleteForm = document.getElementById('form-delete');
    deleteForm.addEventListener('submit', async(e) => {
        e.preventDefault();
        const isPermanent = document.querySelector('input[name="delete-mode"]:checked').value === 'hard';
        const payload = {
            account_no: parseInt(document.getElementById('delete-acc-no').value),
            permanent: isPermanent
        };

        if (isPermanent && !confirm(`WARNING: Are you absolutely sure you want to PERMANENTLY PURGE Account #${payload.account_no} and all its transaction history from the SQLite database? This cannot be undone!`)) {
            return;
        }

        const btn = document.getElementById('btn-submit-delete');
        btn.disabled = true;

        try {
            const res = await fetch(`${API_BASE}/api/delete`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            });
            const data = await res.json();
            if (data.success) {
                showToast(data.message, 'success');
                deleteForm.reset();
                document.getElementById('delete-lookup-info').style.display = 'none';
                refreshAllData();
                showTab('accounts');
            } else {
                showToast(data.error || 'Closure failed.', 'error');
            }
        } catch (err) {
            showToast('Network error connecting to C++ backend.', 'error');
        } finally {
            btn.disabled = false;
        }
    });
}

function initFilters() {
    const searchInput = document.getElementById('acc-search-input');
    let debounceTimer;
    if (searchInput) {
        searchInput.addEventListener('input', () => {
            clearTimeout(debounceTimer);
            debounceTimer = setTimeout(loadAccounts, 300);
        });
    }

    document.getElementById('acc-filter-status')?.addEventListener('change', loadAccounts);
    document.getElementById('acc-filter-kyc')?.addEventListener('change', loadAccounts);
}

// QUICK AMOUNT CHIP HELPERS
function setDepositAmount(val) {
    const input = document.getElementById('deposit-amount');
    input.value = (parseFloat(input.value || 0) + val).toFixed(2);
}

function setWithdrawAmount(val) {
    document.getElementById('withdraw-amount').value = val.toFixed(2);
}

// RECEIPT MODAL
async function openReceiptModal(tx) {
    if (!tx) return;

    let holderName = 'Account Holder';
    try {
        const res = await fetch(`${API_BASE}/api/accounts/${tx.account_no}`);
        const data = await res.json();
        if (data.success) holderName = data.data.holder_name;
    } catch (e) {}

    document.getElementById('rec-tx-id').textContent = `#TX-${tx.tx_id}`;
    document.getElementById('rec-time').textContent = tx.timestamp || new Date().toLocaleString();
    document.getElementById('rec-acc-no').textContent = `#${tx.account_no}`;
    document.getElementById('rec-holder').textContent = holderName;
    document.getElementById('rec-type').textContent = tx.type;
    document.getElementById('rec-type').className = `badge ${tx.type.includes('WITHDRAW') ? 'badge-withdraw' : 'badge-deposit'}`;

    const isOut = tx.type.includes('WITHDRAW') || tx.type === 'TRANSFER_OUT';
    document.getElementById('rec-amount').textContent = `${isOut ? '-' : '+'}${formatCurrency(tx.amount)}`;
    document.getElementById('rec-amount').style.color = isOut ? '#dc2626' : '#059669';
    document.getElementById('rec-balance').textContent = formatCurrency(tx.balance_after);
    document.getElementById('rec-remarks').textContent = tx.remarks || 'Standard Operation';

    const targetRow = document.getElementById('rec-target-row');
    if (tx.target_account && tx.target_account > 0) {
        targetRow.style.display = 'flex';
        document.getElementById('rec-target').textContent = `#${tx.target_account}`;
    } else {
        targetRow.style.display = 'none';
    }

    document.getElementById('receipt-modal').classList.add('active');
}

function openReceiptModalById(txId) {
    const tx = lastTransactionsCache.find(t => t.tx_id === txId);
    if (tx) {
        openReceiptModal(tx);
    } else {
        showToast('Fetching transaction receipt details...', 'info');
        fetch(`${API_BASE}/api/transactions?limit=200`)
            .then(r => r.json())
            .then(d => {
                if (d.success) {
                    const found = d.data.find(t => t.tx_id === txId);
                    if (found) openReceiptModal(found);
                }
            });
    }
}

function closeReceiptModal() {
    document.getElementById('receipt-modal').classList.remove('active');
}

function printReceipt() {
    window.print();
}

// ACCOUNT PROFILE INSPECTOR MODAL
async function inspectAccount(accNo) {
    try {
        const res = await fetch(`${API_BASE}/api/accounts/${accNo}`);
        const data = await res.json();
        if (!data.success) {
            showToast('Unable to fetch account details.', 'error');
            return;
        }

        const acc = data.data;
        const body = document.getElementById('account-detail-body');
        body.innerHTML = `
            <div style="display:flex; justify-content:space-between; align-items:flex-start; margin-bottom:16px;">
                <div>
                    <h2 style="font-size:1.3rem; font-weight:800; color:#fff;">${escapeHtml(acc.holder_name)}</h2>
                    <span class="font-mono text-muted" style="font-size:0.85rem;">Account Number: #${acc.account_no}</span>
                </div>
                <div style="text-align:right;">
                    <div style="font-size:1.5rem; font-weight:800; color:#10b981; font-family:var(--font-mono);">${formatCurrency(acc.balance)}</div>
                    <span class="text-muted" style="font-size:0.75rem;">Current Cleared Balance</span>
                </div>
            </div>

            <div style="display:grid; grid-template-columns:1fr 1fr; gap:12px; margin-bottom:16px; font-size:0.85rem;">
                <div style="background:var(--bg-surface-elevated); padding:10px; border-radius:8px;">
                    <span class="text-muted" style="display:block; font-size:0.7rem;">Account Type:</span>
                    <strong>${acc.account_type}</strong>
                </div>
                <div style="background:var(--bg-surface-elevated); padding:10px; border-radius:8px;">
                    <span class="text-muted" style="display:block; font-size:0.7rem;">Account Status:</span>
                    ${getStatusBadge(acc.status)}
                </div>
                <div style="background:var(--bg-surface-elevated); padding:10px; border-radius:8px;">
                    <span class="text-muted" style="display:block; font-size:0.7rem;">KYC Compliance:</span>
                    ${getKycBadge(acc.kyc_status)}
                </div>
                <div style="background:var(--bg-surface-elevated); padding:10px; border-radius:8px;">
                    <span class="text-muted" style="display:block; font-size:0.7rem;">Identity Proof:</span>
                    <strong>${acc.id_type}: ${escapeHtml(acc.id_number || 'N/A')}</strong>
                </div>
                <div style="background:var(--bg-surface-elevated); padding:10px; border-radius:8px; grid-column:span 2;">
                    <span class="text-muted" style="display:block; font-size:0.7rem;">Contact Email & Phone:</span>
                    <strong>${escapeHtml(acc.email || 'None')} &bull; ${escapeHtml(acc.phone || 'None')}</strong>
                </div>
                <div style="background:var(--bg-surface-elevated); padding:10px; border-radius:8px; grid-column:span 2;">
                    <span class="text-muted" style="display:block; font-size:0.7rem;">Registered Address:</span>
                    <strong>${escapeHtml(acc.address || 'None on file')}</strong>
                </div>
                <div style="background:var(--bg-surface-elevated); padding:10px; border-radius:8px; grid-column:span 2; font-family:var(--font-mono); font-size:0.75rem;">
                    <span class="text-muted" style="display:block;">Created: ${acc.created_at} | Last Activity: ${acc.updated_at}</span>
                </div>
            </div>

            <div style="display:flex; gap:10px; margin-top:20px;">
                <button class="btn btn-secondary" style="flex:1;" onclick="closeAccountModal(); quickSelectAccount(${acc.account_no}, 'deposit')">Deposit</button>
                <button class="btn btn-secondary" style="flex:1;" onclick="closeAccountModal(); quickSelectAccount(${acc.account_no}, 'withdraw')">Withdraw</button>
                <button class="btn btn-outline" style="flex:1;" onclick="closeAccountModal(); showTab('freeze'); document.getElementById('freeze-acc-no').value = ${acc.account_no}; lookupAccountForOperation('freeze');">Security Lock</button>
            </div>
        `;
        document.getElementById('account-detail-modal').classList.add('active');
    } catch (err) {
        showToast('Error displaying account profile.', 'error');
    }
}

function closeAccountModal() {
    document.getElementById('account-detail-modal').classList.remove('active');
}

// FINANCIAL CALCULATORS
function calculateInterest() {
    const P = parseFloat(document.getElementById('calc-principal').value) || 0;
    const r = (parseFloat(document.getElementById('calc-rate').value) || 0) / 100;
    const t = parseFloat(document.getElementById('calc-years').value) || 0;
    const n = 12; // Monthly compounding

    const A = P * Math.pow((1 + (r / n)), (n * t));
    const earned = A - P;

    document.getElementById('calc-maturity-val').textContent = formatCurrency(A);
    document.getElementById('calc-earned-val').textContent = formatCurrency(earned);
}

function calculateLoanEMI() {
    const P = parseFloat(document.getElementById('calc-loan-amount').value) || 0;
    const annualRate = parseFloat(document.getElementById('calc-loan-rate').value) || 0;
    const n = parseFloat(document.getElementById('calc-loan-months').value) || 1;
    const r = (annualRate / 12) / 100;

    let emi = 0;
    if (r > 0) {
        emi = (P * r * Math.pow(1 + r, n)) / (Math.pow(1 + r, n) - 1);
    } else {
        emi = P / n;
    }
    const totalRepay = emi * n;

    document.getElementById('calc-emi-val').textContent = `${formatCurrency(emi)} / mo`;
    document.getElementById('calc-loan-total').textContent = formatCurrency(totalRepay);
}

// EXPORT TO CSV
function exportPassbookCSV() {
    if (!lastTransactionsCache || lastTransactionsCache.length === 0) {
        showToast('No transactions to export.', 'info');
        return;
    }

    const headers = ['TX_ID', 'Account_No', 'Type', 'Amount', 'Balance_After', 'Target_Account', 'Remarks', 'Timestamp'];
    const rows = lastTransactionsCache.map(t => [
        t.tx_id,
        t.account_no,
        t.type,
        t.amount.toFixed(2),
        t.balance_after.toFixed(2),
        t.target_account || '',
        `"${(t.remarks || '').replace(/"/g, '""')}"`,
        `"${t.timestamp}"`
    ]);

    const csvContent = 'data:text/csv;charset=utf-8,' + [headers.join(','), ...rows.map(e => e.join(','))].join('\n');
    const encodedUri = encodeURI(csvContent);
    const link = document.createElement('a');
    link.setAttribute('href', encodedUri);
    link.setAttribute('download', `apex_bank_ledger_${Date.now()}.csv`);
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
    showToast('Transaction statement exported to CSV.', 'success');
}

// TOAST NOTIFICATIONS
function showToast(message, type = 'info') {
    const container = document.getElementById('toast-container');
    const toast = document.createElement('div');
    toast.className = `toast ${type}`;

    let iconSvg = '';
    if (type === 'success') {
        iconSvg = '<svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="#10b981" stroke-width="2"><polyline points="20 6 9 17 4 12"></polyline></svg>';
    } else if (type === 'error') {
        iconSvg = '<svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="#ef4444" stroke-width="2"><circle cx="12" cy="12" r="10"></circle><line x1="15" y1="9" x2="9" y2="15"></line><line x1="9" y1="9" x2="15" y2="15"></line></svg>';
    } else {
        iconSvg = '<svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="#06b6d4" stroke-width="2"><circle cx="12" cy="12" r="10"></circle><line x1="12" y1="16" x2="12" y2="12"></line><line x1="12" y1="8" x2="12.01" y2="8"></line></svg>';
    }

    toast.innerHTML = `${iconSvg}<span>${escapeHtml(message)}</span>`;
    container.appendChild(toast);

    setTimeout(() => {
        toast.style.opacity = '0';
        toast.style.transform = 'translateX(50px)';
        toast.style.transition = 'all 0.3s ease';
        setTimeout(() => toast.remove(), 300);
    }, 4000);
}

// FORMATTING & BADGE HELPERS
function formatCurrency(val) {
    const num = parseFloat(val) || 0;
    return '₹' + num.toLocaleString('en-IN', { minimumFractionDigits: 2, maximumFractionDigits: 2 });
}

function getStatusBadge(status) {
    if (status === 'ACTIVE') return '<span class="badge badge-active">Active</span>';
    if (status === 'FROZEN') return '<span class="badge badge-frozen">Frozen</span>';
    if (status === 'CLOSED') return '<span class="badge badge-closed">Closed</span>';
    return `<span class="badge">${escapeHtml(status)}</span>`;
}

function getKycBadge(kyc) {
    if (kyc === 'VERIFIED') return '<span class="badge badge-verified">Verified</span>';
    if (kyc === 'PENDING') return '<span class="badge badge-pending">Pending</span>';
    if (kyc === 'REJECTED') return '<span class="badge badge-rejected">Rejected</span>';
    return `<span class="badge">${escapeHtml(kyc)}</span>`;
}

function getTypeBadge(type) {
    if (type === 'DEPOSIT') return '<span class="badge badge-deposit">Deposit</span>';
    if (type === 'WITHDRAWAL') return '<span class="badge badge-withdraw">Withdraw</span>';
    if (type === 'TRANSFER_IN') return '<span class="badge badge-transfer-in">Transfer In</span>';
    if (type === 'TRANSFER_OUT') return '<span class="badge badge-transfer-out">Transfer Out</span>';
    return `<span class="badge">${escapeHtml(type)}</span>`;
}

function escapeHtml(str) {
    if (!str) return '';
    return str.replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#039;');
}