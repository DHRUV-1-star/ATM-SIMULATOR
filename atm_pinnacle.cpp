/*
 * ============================================================
 *   ATM SIMULATOR — C++ Console Application
 *   Features: Authentication, Balance, Deposit, Withdraw,
 *             Transfer, Transaction History, Account Locking
 * ============================================================
 */

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <map>
#include <ctime>
#include <sstream>
#include <algorithm>
#include <limits>

// ─────────────────────────────────────────────
//  CONSTANTS
// ─────────────────────────────────────────────
const int    MAX_PIN_ATTEMPTS   = 3;
const double WITHDRAWAL_LIMIT   = 50000.0;   // per transaction
const double DAILY_LIMIT        = 100000.0;  // total daily withdrawals
const double MIN_BALANCE        = 500.0;     // minimum balance requirement

// ─────────────────────────────────────────────
//  HELPER: Current timestamp string
// ─────────────────────────────────────────────
std::string currentTimestamp() {
    std::time_t now = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    return std::string(buf);
}

// ─────────────────────────────────────────────
//  STRUCT: Transaction Record
// ─────────────────────────────────────────────
struct Transaction {
    std::string timestamp;
    std::string type;        // DEPOSIT | WITHDRAWAL | TRANSFER_OUT | TRANSFER_IN
    double      amount;
    double      balanceAfter;
    std::string note;        // optional note / counterpart account

    Transaction(const std::string& t, double amt, double bal, const std::string& n = "")
        : timestamp(currentTimestamp()), type(t), amount(amt), balanceAfter(bal), note(n) {}
};

// ─────────────────────────────────────────────
//  CLASS: BankAccount
// ─────────────────────────────────────────────
class BankAccount {
private:
    std::string          accountNumber;
    std::string          holderName;
    std::string          pin;             // stored as plain text (demo only)
    double               balance;
    bool                 locked;
    int                  failedAttempts;
    double               dailyWithdrawn;  // resets on new login session
    std::vector<Transaction> history;

public:
    BankAccount() : balance(0), locked(false), failedAttempts(0), dailyWithdrawn(0) {}

    BankAccount(const std::string& accNo, const std::string& name,
                const std::string& pin, double initialBalance)
        : accountNumber(accNo), holderName(name), pin(pin),
          balance(initialBalance), locked(false), failedAttempts(0), dailyWithdrawn(0) {}

    // ── Getters ──────────────────────────────
    std::string getAccountNumber() const { return accountNumber; }
    std::string getHolderName()    const { return holderName; }
    double      getBalance()       const { return balance; }
    bool        isLocked()         const { return locked; }
    double      getDailyWithdrawn()const { return dailyWithdrawn; }
    const std::vector<Transaction>& getHistory() const { return history; }

    // ── PIN Verification ─────────────────────
    bool verifyPIN(const std::string& enteredPin) {
        if (locked) return false;
        if (enteredPin == pin) {
            failedAttempts = 0;
            dailyWithdrawn = 0; // reset daily limit per session
            return true;
        }
        failedAttempts++;
        if (failedAttempts >= MAX_PIN_ATTEMPTS) locked = true;
        return false;
    }

    int getRemainingAttempts() const { return MAX_PIN_ATTEMPTS - failedAttempts; }

    // ── Change PIN ───────────────────────────
    bool changePIN(const std::string& oldPin, const std::string& newPin) {
        if (oldPin != pin)  return false;
        if (newPin.length() < 4) return false;
        pin = newPin;
        history.emplace_back("PIN_CHANGE", 0.0, balance, "PIN updated successfully");
        return true;
    }

    // ── Deposit ──────────────────────────────
    bool deposit(double amount, const std::string& note = "") {
        if (amount <= 0) return false;
        balance += amount;
        history.emplace_back("DEPOSIT", amount, balance, note);
        return true;
    }

    // ── Withdraw ─────────────────────────────
    std::string withdraw(double amount) {
        if (amount <= 0)
            return "INVALID_AMOUNT";
        if (amount > WITHDRAWAL_LIMIT)
            return "EXCEEDS_TXN_LIMIT";
        if (dailyWithdrawn + amount > DAILY_LIMIT)
            return "EXCEEDS_DAILY_LIMIT";
        if (balance - amount < MIN_BALANCE)
            return "INSUFFICIENT_FUNDS";

        balance        -= amount;
        dailyWithdrawn += amount;
        history.emplace_back("WITHDRAWAL", amount, balance);
        return "OK";
    }

    // ── Transfer Out (deduct from this account) ─
    std::string transferOut(double amount, const std::string& toAccount) {
        if (amount <= 0)
            return "INVALID_AMOUNT";
        if (amount > WITHDRAWAL_LIMIT)
            return "EXCEEDS_TXN_LIMIT";
        if (dailyWithdrawn + amount > DAILY_LIMIT)
            return "EXCEEDS_DAILY_LIMIT";
        if (balance - amount < MIN_BALANCE)
            return "INSUFFICIENT_FUNDS";

        balance        -= amount;
        dailyWithdrawn += amount;
        history.emplace_back("TRANSFER_OUT", amount, balance, "To: " + toAccount);
        return "OK";
    }

    // ── Transfer In (credit to this account) ─
    void transferIn(double amount, const std::string& fromAccount) {
        balance += amount;
        history.emplace_back("TRANSFER_IN", amount, balance, "From: " + fromAccount);
    }

    // ── Admin: Unlock account ─────────────────
    void unlock() {
        locked         = false;
        failedAttempts = 0;
    }
};

// ─────────────────────────────────────────────
//  CLASS: ATM  (the controller)
// ─────────────────────────────────────────────
class ATM {
private:
    std::map<std::string, BankAccount> accounts;
    std::string  bankName;
    BankAccount* currentAccount;

    // ─── UI helpers ──────────────────────────
    void clearScreen() {
#ifdef _WIN32
        system("cls");
#else
        system("clear");
#endif
    }

    void printBanner() {
        std::cout << "\n";
        std::cout << "  ╔══════════════════════════════════════════════╗\n";
        std::cout << "  ║         " << std::left << std::setw(36) << bankName << "║\n";
        std::cout << "  ║              ATM TERMINAL                    ║\n";
        std::cout << "  ╚══════════════════════════════════════════════╝\n\n";
    }

    void printLine(char c = '-', int len = 50) {
        std::cout << "  " << std::string(len, c) << "\n";
    }

    void printHeader(const std::string& title) {
        printLine();
        std::cout << "  " << title << "\n";
        printLine();
    }

    double getAmount(const std::string& prompt) {
        double amount = 0;
        while (true) {
            std::cout << "  " << prompt;
            std::cin  >> amount;
            if (std::cin.fail() || amount < 0) {
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "  [!] Invalid input. Please enter a positive number.\n";
            } else {
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                return amount;
            }
        }
    }

    int getMenuChoice(int lo, int hi) {
        int choice;
        while (true) {
            std::cout << "  Your choice: ";
            std::cin  >> choice;
            if (std::cin.fail() || choice < lo || choice > hi) {
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "  [!] Invalid option. Enter " << lo << "-" << hi << ": ";
            } else {
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                return choice;
            }
        }
    }

    void pause() {
        std::cout << "\n  Press ENTER to continue...";
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    // ─── Transaction type display label ──────
    std::string txnLabel(const std::string& type) {
        if (type == "DEPOSIT")      return "  (+) DEPOSIT      ";
        if (type == "WITHDRAWAL")   return "  (-) WITHDRAWAL   ";
        if (type == "TRANSFER_OUT") return "  (-) TRANSFER OUT ";
        if (type == "TRANSFER_IN")  return "  (+) TRANSFER IN  ";
        if (type == "PIN_CHANGE")   return "  (*) PIN CHANGE   ";
        return "  ( ) " + type + "  ";
    }

    // ─── Authentication Flow ─────────────────
    bool authenticate() {
        std::string accNo, pin;

        std::cout << "  Account Number : ";
        std::getline(std::cin, accNo);

        if (accounts.find(accNo) == accounts.end()) {
            std::cout << "\n  [!] Account not found.\n";
            pause();
            return false;
        }

        BankAccount& acc = accounts[accNo];

        if (acc.isLocked()) {
            std::cout << "\n  [!] This account is LOCKED due to multiple failed PIN attempts.\n";
            std::cout << "      Please visit your nearest bank branch.\n";
            pause();
            return false;
        }

        // Mask PIN input (basic cross-platform approach)
        std::cout << "  PIN            : ";
        std::getline(std::cin, pin);

        if (acc.verifyPIN(pin)) {
            currentAccount = &accounts[accNo];
            std::cout << "\n  Welcome, " << acc.getHolderName() << "!\n";
            pause();
            return true;
        } else {
            if (acc.isLocked()) {
                std::cout << "\n  [!] Too many failed attempts. Account LOCKED.\n";
            } else {
                std::cout << "\n  [!] Incorrect PIN. "
                          << acc.getRemainingAttempts()
                          << " attempt(s) remaining.\n";
            }
            pause();
            return false;
        }
    }

    // ─── MENU: Check Balance ─────────────────
    void menuBalance() {
        printHeader(" CHECK BALANCE");
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "  Account    : " << currentAccount->getAccountNumber() << "\n";
        std::cout << "  Holder     : " << currentAccount->getHolderName()    << "\n";
        std::cout << "  Balance    : INR " << currentAccount->getBalance()   << "\n";
        std::cout << "  Min. Hold  : INR " << MIN_BALANCE                    << "\n";
        std::cout << "  Avail. W/D : INR "
                  << std::max(0.0, currentAccount->getBalance() - MIN_BALANCE) << "\n";
        pause();
    }

    // ─── MENU: Deposit ───────────────────────
    void menuDeposit() {
        printHeader(" DEPOSIT CASH");
        double amount = getAmount("Amount to deposit (INR): ");
        if (amount <= 0) {
            std::cout << "  [!] Amount must be greater than zero.\n";
            pause();
            return;
        }
        currentAccount->deposit(amount, "Cash deposit at ATM");
        std::cout << "\n  Deposit successful!\n";
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "     Deposited  : INR " << amount                           << "\n";
        std::cout << "     New Balance: INR " << currentAccount->getBalance()     << "\n";
        pause();
    }

    // ─── MENU: Withdraw ──────────────────────
    void menuWithdraw() {
        printHeader(" WITHDRAW CASH");
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "  Current Balance   : INR " << currentAccount->getBalance()       << "\n";
        std::cout << "  Per-Txn Limit     : INR " << WITHDRAWAL_LIMIT                   << "\n";
        std::cout << "  Daily Limit Used  : INR " << currentAccount->getDailyWithdrawn()<< "\n";
        std::cout << "  Daily Remaining   : INR "
                  << std::max(0.0, DAILY_LIMIT - currentAccount->getDailyWithdrawn())   << "\n\n";

        double amount = getAmount("Amount to withdraw (INR): ");

        // Quick amount options
        std::cout << "\n  Quick amounts: [1] 500  [2] 1000  [3] 2000  [4] 5000  [5] Custom\n";
        std::cout << "  Select or press ENTER to use entered amount: ";
        std::string opt;
        std::getline(std::cin, opt);
        if      (opt == "1") amount = 500;
        else if (opt == "2") amount = 1000;
        else if (opt == "3") amount = 2000;
        else if (opt == "4") amount = 5000;

        std::string result = currentAccount->withdraw(amount);

        if (result == "OK") {
            std::cout << "\n  Withdrawal successful!\n";
            std::cout << "     Withdrawn   : INR " << amount                         << "\n";
            std::cout << "     New Balance : INR " << currentAccount->getBalance()   << "\n";
        } else if (result == "INVALID_AMOUNT") {
            std::cout << "\n  [!] Invalid amount entered.\n";
        } else if (result == "EXCEEDS_TXN_LIMIT") {
            std::cout << "\n  [!] Amount exceeds per-transaction limit of INR "
                      << WITHDRAWAL_LIMIT << ".\n";
        } else if (result == "EXCEEDS_DAILY_LIMIT") {
            std::cout << "\n  [!] Amount exceeds your daily withdrawal limit.\n";
        } else if (result == "INSUFFICIENT_FUNDS") {
            std::cout << "\n  [!] Insufficient funds. Minimum balance of INR "
                      << MIN_BALANCE << " must be maintained.\n";
        }
        pause();
    }

    // ─── MENU: Transfer ──────────────────────
    void menuTransfer() {
        printHeader(" FUND TRANSFER");
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "  Your Balance : INR " << currentAccount->getBalance() << "\n\n";

        std::string toAcc;
        std::cout << "  Beneficiary Account No: ";
        std::getline(std::cin, toAcc);

        if (toAcc == currentAccount->getAccountNumber()) {
            std::cout << "\n  [!] Cannot transfer to your own account.\n";
            pause();
            return;
        }
        if (accounts.find(toAcc) == accounts.end()) {
            std::cout << "\n  [!] Beneficiary account not found.\n";
            pause();
            return;
        }

        BankAccount& beneficiary = accounts[toAcc];
        std::cout << "  Beneficiary Name : " << beneficiary.getHolderName() << "\n";
        std::cout << "\n  Confirm? This is the correct recipient.\n";
        std::cout << "  [1] Yes  [2] No\n";
        int confirm = getMenuChoice(1, 2);
        if (confirm == 2) {
            std::cout << "\n  Transfer cancelled.\n";
            pause();
            return;
        }

        double amount = getAmount("\n  Amount to transfer (INR): ");

        // Confirm with PIN re-authentication for security
        std::cout << "\n  Re-enter your PIN to authorise transfer: ";
        std::string pin;
        std::getline(std::cin, pin);

        // Temporarily verify without modifying attempt counters
        BankAccount tempCheck = *currentAccount;
        if (!tempCheck.verifyPIN(pin)) {
            std::cout << "\n  [!] Incorrect PIN. Transfer CANCELLED.\n";
            pause();
            return;
        }

        std::string result = currentAccount->transferOut(amount, toAcc);
        if (result == "OK") {
            beneficiary.transferIn(amount, currentAccount->getAccountNumber());
            std::cout << "\n  Transfer successful!\n";
            std::cout << "     Transferred : INR " << amount                         << "\n";
            std::cout << "     To          : " << toAcc << " (" << beneficiary.getHolderName() << ")\n";
            std::cout << "     New Balance : INR " << currentAccount->getBalance()   << "\n";
        } else if (result == "INVALID_AMOUNT") {
            std::cout << "\n  [!] Invalid amount entered.\n";
        } else if (result == "EXCEEDS_TXN_LIMIT") {
            std::cout << "\n  [!] Amount exceeds per-transaction limit of INR "
                      << WITHDRAWAL_LIMIT << ".\n";
        } else if (result == "EXCEEDS_DAILY_LIMIT") {
            std::cout << "\n  [!] Daily limit exceeded.\n";
        } else if (result == "INSUFFICIENT_FUNDS") {
            std::cout << "\n  [!] Insufficient funds. Minimum balance of INR "
                      << MIN_BALANCE << " must be maintained.\n";
        }
        pause();
    }

    // ─── MENU: Transaction History ───────────
    void menuHistory() {
        printHeader(" TRANSACTION HISTORY");
        const auto& hist = currentAccount->getHistory();
        if (hist.empty()) {
            std::cout << "  No transactions recorded yet.\n";
            pause();
            return;
        }

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "  " << std::left
                  << std::setw(22) << "DATE & TIME"
                  << std::setw(20) << "TYPE"
                  << std::setw(14) << "AMOUNT (INR)"
                  << std::setw(16) << "BALANCE (INR)"
                  << "NOTE\n";
        printLine('-', 90);

        // Show last 20 transactions (most recent first)
        int start = std::max(0, (int)hist.size() - 20);
        for (int i = (int)hist.size() - 1; i >= start; --i) {
            const Transaction& t = hist[i];
            std::cout << "  " << std::left
                      << std::setw(22) << t.timestamp
                      << std::setw(20) << t.type
                      << std::setw(14) << (t.amount > 0 ? std::to_string(t.amount).substr(0, 10) : "-")
                      << std::setw(16) << std::to_string(t.balanceAfter).substr(0, 10)
                      << t.note << "\n";
        }
        printLine('-', 90);
        std::cout << "  Showing last " << std::min(20, (int)hist.size())
                  << " of " << hist.size() << " transactions.\n";
        pause();
    }

    // ─── MENU: Change PIN ────────────────────
    void menuChangePIN() {
        printHeader(" CHANGE PIN");
        std::string oldPin, newPin, confirmPin;

        std::cout << "  Current PIN   : ";
        std::getline(std::cin, oldPin);

        std::cout << "  New PIN       : ";
        std::getline(std::cin, newPin);

        std::cout << "  Confirm PIN   : ";
        std::getline(std::cin, confirmPin);

        if (newPin != confirmPin) {
            std::cout << "\n  [!] PINs do not match. Operation cancelled.\n";
            pause();
            return;
        }
        if (newPin.length() < 4) {
            std::cout << "\n  [!] PIN must be at least 4 digits.\n";
            pause();
            return;
        }
        bool onlyDigits = true;
        for (char c : newPin) if (!std::isdigit(c)) { onlyDigits = false; break; }
        if (!onlyDigits) {
            std::cout << "\n  [!] PIN must contain digits only.\n";
            pause();
            return;
        }

        if (currentAccount->changePIN(oldPin, newPin)) {
            std::cout << "\n  PIN changed successfully!\n";
        } else {
            std::cout << "\n  [!] Incorrect current PIN. Operation failed.\n";
        }
        pause();
    }

    // ─── Main Menu ───────────────────────────
    void showMainMenu() {
        while (true) {
            clearScreen();
            printBanner();
            std::cout << "  Logged in as : " << currentAccount->getHolderName()    << "\n";
            std::cout << "  Account No.  : " << currentAccount->getAccountNumber() << "\n";
            std::cout << "  Session Time : " << currentTimestamp()                 << "\n\n";
            printLine();
            std::cout << "  [1]  Check Balance\n";
            std::cout << "  [2]  Deposit Cash\n";
            std::cout << "  [3]  Withdraw Cash\n";
            std::cout << "  [4]  Transfer Funds\n";
            std::cout << "  [5]  Transaction History\n";
            std::cout << "  [6]  Change PIN\n";
            std::cout << "  [7]  Logout\n";
            printLine();

            int choice = getMenuChoice(1, 7);

            clearScreen();
            printBanner();

            switch (choice) {
                case 1: menuBalance();   break;
                case 2: menuDeposit();   break;
                case 3: menuWithdraw();  break;
                case 4: menuTransfer();  break;
                case 5: menuHistory();   break;
                case 6: menuChangePIN(); break;
                case 7:
                    currentAccount = nullptr;
                    std::cout << "\n  You have been logged out. Thank you for banking with us!\n\n";
                    pause();
                    return;
            }
        }
    }

public:
    ATM(const std::string& name) : bankName(name), currentAccount(nullptr) {
        // ── Seed demo accounts ────────────────
        accounts["1001001001"] = BankAccount("1001001001", "Alice Johnson", "1234", 25000.00);
        accounts["1001002002"] = BankAccount("1001002002", "Bob Smith",     "5678", 15000.00);
        accounts["1001003003"] = BankAccount("1001003003", "Carol White",   "9012", 50000.00);

        // Add some sample history for Alice
        accounts["1001001001"].deposit(5000, "Initial deposit");
        accounts["1001001001"].deposit(10000, "Salary credit");
    }

    // ── Entry point ───────────────────────────
    void run() {
        while (true) {
            clearScreen();
            printBanner();
            std::cout << "  Welcome! Please identify yourself.\n\n";
            printLine();
            std::cout << "  [1]  Login to Account\n";
            std::cout << "  [2]  Demo Account Info\n";
            std::cout << "  [0]  Exit\n";
            printLine();

            int choice = getMenuChoice(0, 2);

            if (choice == 0) {
                clearScreen();
                printBanner();
                std::cout << "  Thank you for using " << bankName << " ATM.\n";
                std::cout << "  Have a great day!\n\n";
                break;
            } else if (choice == 2) {
                clearScreen();
                printBanner();
                std::cout << "  -- DEMO ACCOUNTS --\n\n";
                std::cout << "  Account No.  : 1001001001\n";
                std::cout << "  Name         : Alice Johnson\n";
                std::cout << "  PIN          : 1234\n\n";
                std::cout << "  Account No.  : 1001002002\n";
                std::cout << "  Name         : Bob Smith\n";
                std::cout << "  PIN          : 5678\n\n";
                std::cout << "  Account No.  : 1001003003\n";
                std::cout << "  Name         : Carol White\n";
                std::cout << "  PIN          : 9012\n\n";
                pause();
            } else {
                clearScreen();
                printBanner();
                printHeader(" SECURE LOGIN");
                if (authenticate()) {
                    showMainMenu();
                }
            }
        }
    }
};

// ─────────────────────────────────────────────
//  MAIN
// ─────────────────────────────────────────────
int main() {
    ATM atm("NOVA BANK");
    atm.run();
    return 0;
}
