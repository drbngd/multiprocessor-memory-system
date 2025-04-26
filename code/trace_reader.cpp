#include <cstdio>
#include <cstring>
#include <string>
#include <iostream>
#include <iomanip>
#include <set>
#include <map>
#include <vector>
#include <algorithm>
#include <sstream>

// ChampSim trace format structures
constexpr std::size_t NUM_INSTR_DESTINATIONS = 2;
constexpr std::size_t NUM_INSTR_SOURCES = 4;

struct input_instr {
    unsigned long long ip;  // instruction pointer
    unsigned char is_branch;
    unsigned char branch_taken;
    unsigned char destination_registers[NUM_INSTR_DESTINATIONS];
    unsigned char source_registers[NUM_INSTR_SOURCES];
    unsigned long long destination_memory[NUM_INSTR_DESTINATIONS];
    unsigned long long source_memory[NUM_INSTR_SOURCES];
};

// Special registers that might indicate context/ASID changes
constexpr int REG_STACK_POINTER = 6;
constexpr int REG_FLAGS = 25;
constexpr int REG_INSTRUCTION_POINTER = 26;

// Instruction types
enum class InstrType {
    LOAD,
    STORE,
    BRANCH_TAKEN,
    BRANCH_NOT_TAKEN,
    ALU,
    UNKNOWN
};

std::string instr_type_str(InstrType type) {
    switch(type) {
        case InstrType::LOAD: return "LOAD";
        case InstrType::STORE: return "STORE";
        case InstrType::BRANCH_TAKEN: return "BRANCH(T)";
        case InstrType::BRANCH_NOT_TAKEN: return "BRANCH(NT)";
        case InstrType::ALU: return "ALU";
        default: return "UNKNOWN";
    }
}

// Helper function to format memory addresses
std::string format_address(unsigned long long addr) {
    std::stringstream ss;
    ss << "0x" << std::setfill('0') << std::setw(16) << std::hex << addr;
    return ss.str();
}

// Helper function to format register names
std::string format_register(unsigned char reg) {
    std::string result = "R" + std::to_string((int)reg);
    if (reg == REG_STACK_POINTER) result += "(SP)";
    else if (reg == REG_FLAGS) result += "(FLAGS)";
    else if (reg == REG_INSTRUCTION_POINTER) result += "(IP)";
    return result;
}

class TraceAnalyzer {
    std::set<unsigned long long> unique_pages;
    std::map<unsigned char, int> register_writes;
    std::map<InstrType, int> instr_type_count;
    unsigned long long last_page = 0;
    int instruction_count = 0;
    int page_switches = 0;

    InstrType classify_instruction(const input_instr& instr) {
        if (instr.is_branch) {
            return instr.branch_taken ? InstrType::BRANCH_TAKEN : InstrType::BRANCH_NOT_TAKEN;
        }
        
        bool has_mem_read = false;
        bool has_mem_write = false;
        
        for (int i = 0; i < NUM_INSTR_SOURCES; i++) {
            if (instr.source_memory[i] != 0) {
                has_mem_read = true;
                break;
            }
        }
        
        for (int i = 0; i < NUM_INSTR_DESTINATIONS; i++) {
            if (instr.destination_memory[i] != 0) {
                has_mem_write = true;
                break;
            }
        }
        
        if (has_mem_read && !has_mem_write) return InstrType::LOAD;
        if (has_mem_write) return InstrType::STORE;
        if (!has_mem_read && !has_mem_write && !instr.is_branch) return InstrType::ALU;
        
        return InstrType::UNKNOWN;
    }

public:
    void analyze_instruction(const input_instr& instr) {
        instruction_count++;
        
        // Classify and count instruction type
        InstrType type = classify_instruction(instr);
        instr_type_count[type]++;
        
        // Print instruction header
        std::cout << "\n┌─────────────────────────────────────────────────────────\n";
        std::cout << "│ Instruction " << std::setw(8) << instruction_count << " at " 
                  << format_address(instr.ip) << "\n";
        std::cout << "│ Type: " << std::setw(10) << instr_type_str(type) << "\n";
        
        // Track unique pages and print memory accesses
        unsigned long long current_page = instr.ip >> 12;
        if (current_page != last_page) {
            page_switches++;
            last_page = current_page;
        }
        unique_pages.insert(current_page);

        bool has_memory = false;
        // Memory reads
        for (int i = 0; i < NUM_INSTR_SOURCES; i++) {
            if (instr.source_memory[i] != 0) {
                if (!has_memory) {
                    std::cout << "├─ Memory Access ────────────────────────────────────────\n";
                    has_memory = true;
                }
                unique_pages.insert(instr.source_memory[i] >> 12);
                std::cout << "│  READ  " << format_address(instr.source_memory[i]) << "\n";
            }
        }
        // Memory writes
        for (int i = 0; i < NUM_INSTR_DESTINATIONS; i++) {
            if (instr.destination_memory[i] != 0) {
                if (!has_memory) {
                    std::cout << "├─ Memory Access ────────────────────────────────────────\n";
                    has_memory = true;
                }
                unique_pages.insert(instr.destination_memory[i] >> 12);
                std::cout << "│  WRITE " << format_address(instr.destination_memory[i]) << "\n";
            }
        }

        // Register accesses
        std::cout << "├─ Registers ──────────────────────────────────────────\n";
        
        // Source registers
        bool has_src_regs = false;
        for (int i = 0; i < NUM_INSTR_SOURCES; i++) {
            if (instr.source_registers[i] != 0) {
                if (!has_src_regs) {
                    std::cout << "│  Source:      ";
                    has_src_regs = true;
                } else {
                    std::cout << ", ";
                }
                std::cout << format_register(instr.source_registers[i]);
            }
        }
        if (has_src_regs) std::cout << "\n";

        // Destination registers
        bool has_dst_regs = false;
        for (int i = 0; i < NUM_INSTR_DESTINATIONS; i++) {
            if (instr.destination_registers[i] != 0) {
                if (!has_dst_regs) {
                    std::cout << "│  Destination: ";
                    has_dst_regs = true;
                } else {
                    std::cout << ", ";
                }
                register_writes[instr.destination_registers[i]]++;
                std::cout << format_register(instr.destination_registers[i]);
            }
        }
        if (has_dst_regs) std::cout << "\n";
        
        std::cout << "└─────────────────────────────────────────────────────────\n";
    }

    void print_summary() {
        std::cout << "\n╔═════════════════════════════════════════════════════════\n";
        std::cout << "║ Trace Analysis Summary\n";
        std::cout << "╠═════════════════════════════════════════════════════════\n";
        std::cout << "║ Total Instructions: " << instruction_count << "\n";
        std::cout << "║ Unique Pages: " << unique_pages.size() << "\n";
        std::cout << "║ Page Switches: " << page_switches << "\n";
        
        std::cout << "╟─────────────────────────────────────────────────────────\n";
        std::cout << "║ Instruction Distribution\n";
        std::cout << "╟─────────────────────────────────────────────────────────\n";
        for (const auto& pair : instr_type_count) {
            float percentage = (100.0f * pair.second) / instruction_count;
            std::cout << "║ " << std::setw(12) << std::left << instr_type_str(pair.first) 
                     << ": " << std::setw(8) << pair.second 
                     << " (" << std::fixed << std::setprecision(2) << std::setw(6) << percentage << "%)\n";
        }
        
        std::cout << "╟─────────────────────────────────────────────────────────\n";
        std::cout << "║ Top Register Usage\n";
        std::cout << "╟─────────────────────────────────────────────────────────\n";
        
        std::vector<std::pair<unsigned char, int>> reg_vec(register_writes.begin(), register_writes.end());
        std::sort(reg_vec.begin(), reg_vec.end(), 
                 [](const std::pair<unsigned char, int>& a, const std::pair<unsigned char, int>& b) { 
                     return a.second > b.second; 
                 });
        
        for (int i = 0; i < std::min(10, (int)reg_vec.size()); i++) {
            std::cout << "║ " << std::setw(15) << std::left 
                     << format_register(reg_vec[i].first) 
                     << ": " << reg_vec[i].second << " writes\n";
        }
        std::cout << "╚═════════════════════════════════════════════════════════\n\n";
    }
};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <trace_file.champsimtrace.xz>\n";
        return 1;
    }

    std::string cmd = "xz -dc ";
    cmd += argv[1];
    
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        std::cerr << "Error opening pipe to xz\n";
        return 1;
    }

    input_instr curr_instr;
    TraceAnalyzer analyzer;
    const int MAX_INSTRUCTIONS = 1000000; // Analyze first million instructions
    int count = 0;

    std::cout << "╔═════════════════════════════════════════════════════════\n";
    std::cout << "║ ChampSim Trace Analysis\n";
    std::cout << "╚═════════════════════════════════════════════════════════\n\n";
    
    while (fread(&curr_instr, sizeof(input_instr), 1, pipe) == 1 && count < MAX_INSTRUCTIONS) {
        analyzer.analyze_instruction(curr_instr);
        count++;
        
        // Only show first 20 instructions in detail
        if (count == 20) {
            std::cout << "\n... Showing summary for remaining instructions ...\n\n";
        }
        if (count > 20) {
            // Don't print instruction details after the first 20
            continue;
        }
    }

    analyzer.print_summary();

    pclose(pipe);
    return 0;
} 