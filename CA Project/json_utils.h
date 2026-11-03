#ifndef JSON_UTILS_H
#define JSON_UTILS_H

#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include "models.h"

namespace JsonUtils {

inline std::string escape(const std::string& s) {
    std::ostringstream o;
    for (char c : s) {
        if (c == '"') o << "\\\"";
        else if (c == '\\') o << "\\\\";
        else if (c == '\b') o << "\\b";
        else if (c == '\f') o << "\\f";
        else if (c == '\n') o << "\\n";
        else if (c == '\r') o << "\\r";
        else if (c == '\t') o << "\\t";
        else if (static_cast<unsigned char>(c) <= 0x1f) {
            o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
        } else {
            o << c;
        }
    }
    return o.str();
}

inline std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}

// Simple JSON field extractor for flat/semi-flat JSON objects
inline std::string getString(const std::string& json, const std::string& key, const std::string& defVal = "") {
    std::string pattern = "\"" + key + "\"";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return defVal;

    pos += pattern.length();
    // find colon
    pos = json.find(':', pos);
    if (pos == std::string::npos) return defVal;
    pos++;

    // find start of value
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\r' || json[pos] == '\n')) {
        pos++;
    }
    if (pos >= json.length()) return defVal;

    if (json[pos] == '"') {
        pos++;
        std::string val;
        while (pos < json.length()) {
            if (json[pos] == '\\' && pos + 1 < json.length()) {
                val += json[pos + 1];
                pos += 2;
            } else if (json[pos] == '"') {
                break;
            } else {
                val += json[pos++];
            }
        }
        return val;
    } else {
        // Unquoted string or token
        size_t endPos = json.find_first_of(",}\n\r", pos);
        if (endPos == std::string::npos) endPos = json.length();
        std::string val = json.substr(pos, endPos - pos);
        return trim(val);
    }
}

inline int getInt(const std::string& json, const std::string& key, int defVal = 0) {
    std::string s = getString(json, key, "");
    if (s.empty() || s == "null") return defVal;
    try {
        return std::stoi(s);
    } catch (...) {
        return defVal;
    }
}

inline double getDouble(const std::string& json, const std::string& key, double defVal = 0.0) {
    std::string s = getString(json, key, "");
    if (s.empty() || s == "null") return defVal;
    try {
        return std::stod(s);
    } catch (...) {
        return defVal;
    }
}

inline bool getBool(const std::string& json, const std::string& key, bool defVal = false) {
    std::string s = getString(json, key, "");
    if (s == "true" || s == "1") return true;
    if (s == "false" || s == "0") return false;
    return defVal;
}

// Model serializers
inline std::string accountToJson(const Account& a) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);
    ss << "{"
       << "\"account_no\":" << a.account_no << ","
       << "\"holder_name\":\"" << escape(a.holder_name) << "\","
       << "\"account_type\":\"" << escape(a.account_type) << "\","
       << "\"email\":\"" << escape(a.email) << "\","
       << "\"phone\":\"" << escape(a.phone) << "\","
       << "\"balance\":" << a.balance << ","
       << "\"status\":\"" << escape(a.status) << "\","
       << "\"kyc_status\":\"" << escape(a.kyc_status) << "\","
       << "\"id_type\":\"" << escape(a.id_type) << "\","
       << "\"id_number\":\"" << escape(a.id_number) << "\","
       << "\"address\":\"" << escape(a.address) << "\","
       << "\"created_at\":\"" << escape(a.created_at) << "\","
       << "\"updated_at\":\"" << escape(a.updated_at) << "\""
       << "}";
    return ss.str();
}

inline std::string transactionToJson(const Transaction& t) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);
    ss << "{"
       << "\"tx_id\":" << t.tx_id << ","
       << "\"account_no\":" << t.account_no << ","
       << "\"type\":\"" << escape(t.type) << "\","
       << "\"amount\":" << t.amount << ","
       << "\"balance_after\":" << t.balance_after << ","
       << "\"target_account\":" << t.target_account << ","
       << "\"remarks\":\"" << escape(t.remarks) << "\","
       << "\"timestamp\":\"" << escape(t.timestamp) << "\""
       << "}";
    return ss.str();
}

inline std::string auditLogToJson(const AuditLog& l) {
    std::ostringstream ss;
    ss << "{"
       << "\"log_id\":" << l.log_id << ","
       << "\"account_no\":" << l.account_no << ","
       << "\"action\":\"" << escape(l.action) << "\","
       << "\"details\":\"" << escape(l.details) << "\","
       << "\"timestamp\":\"" << escape(l.timestamp) << "\""
       << "}";
    return ss.str();
}

inline std::string statsToJson(const BankStats& s) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);
    ss << "{"
       << "\"total_accounts\":" << s.total_accounts << ","
       << "\"active_accounts\":" << s.active_accounts << ","
       << "\"frozen_accounts\":" << s.frozen_accounts << ","
       << "\"closed_accounts\":" << s.closed_accounts << ","
       << "\"total_reserve\":" << s.total_reserve << ","
       << "\"total_transactions\":" << s.total_transactions << ","
       << "\"kyc_verified\":" << s.kyc_verified << ","
       << "\"kyc_pending\":" << s.kyc_pending
       << "}";
    return ss.str();
}

inline std::string successResponse(const std::string& message, const std::string& dataJson = "null") {
    return "{\"success\":true,\"message\":\"" + escape(message) + "\",\"data\":" + dataJson + "}";
}

inline std::string errorResponse(const std::string& error) {
    return "{\"success\":false,\"error\":\"" + escape(error) + "\"}";
}

} // namespace JsonUtils

#endif // JSON_UTILS_H

