uint16_t memory[UINT16_MAX];
uint16_t reg[R_COUNT];

// The registers
enum
{
    R_R0 = 0,
    R_R1,
    R_R2,
    R_R3,
    R_R4,
    R_R5,
    R_R6,
    R_R7,
    R_PC,
    R_COND,
    R_COUNT,
};

// Opcodes
enum
{
    OP_BR = 0, // Branch
    OP_ADD,    // Add
    OP_LD,     // Load
    OP_ST,     // Store
    OP_JSR,    // Jump register
    OP_AND,    // Bitwise and
    OP_LDR,    // Load register
    OP_STR,    // Store register
    OP_RTI,    // Unused
    OP_NOT,    // Bitwise not
    OP_LDI,    // Load indirect
    OP_STI,    // Store indirect
    OP_JMP,    // Jump
    OP_RES,    // Reserved (unused)
    OP_LEA,    // Load effective address
};

// Condition flags
enum {
    FL_POS = 1 << 0, /* P */
    FL_ZRO = 1 << 1, /* Z */
    FL_NEG = 1 << 2, /* N */
};

// Updates the condition flag register
void update_flags(uint16_t r)
{
    if (reg[r] == 0)
    {
        reg[R_COND] = FL_ZRO;
    }
    else if (reg[r] >> 15) /* a 1 in the left-most bit indicates negative */
    {
        reg[R_COND] = FL_NEG;
    }
    else
    {
        reg[R_COND] = FL_POS;
    }
}

/* sign_extend
extends the sign on binary number, if positive, extend 0, otherwise 1 since we are
using two's complement
*/
uint16_t sign_extend(uint16_t x, int bit_count)
{
    if ((x >> (bit_count - 1)) & 1) {
        x |= (0xFFFF << bit_count);
    }
    return x;
}

int main(int argc, const char* argv[])
{
    {Load Arguments, 12}
    {Setup, 12}

    /* set the PC to starting position */
    /* 0x3000 is the default */
    enum { PC_START = 0x3000 };
    reg[R_PC] = PC_START;

    int running = 1;
    // Opcode impelementation from https://justinmeiners.github.io/lc3-vm/supplies/lc3-isa.pdf
    while (running)
    {
        /* FETCH */
        uint16_t instr = mem_read(reg[R_PC]++);
        uint16_t op = instr >> 12;

        switch (op)
        {
            case OP_ADD:
                {
                    /* destination register (DR) */
                    uint16_t dr = (instr >> 9) & 0x7;
                    /* first operand (SR1) */
                    uint16_t sr1 = (instr >> 6) & 0x7;
                    /* whether we are in immediate mode */
                    uint16_t imm_flag = (instr >> 5) & 0x1;

                    if (imm_flag)
                    {
                        uint16_t imm5 = sign_extend(instr & 0x1F, 5);
                        reg[dr] = reg[sr1] + imm5;
                    }
                    else
                    {
                        uint16_t sr2 = instr & 0x7;
                        reg[dr] = reg[sr1] + reg[sr2];
                    }

                    update_flags(r0);
                }
                break;
            case OP_AND:
                {
                    // destination register (DR)
                    uint16_t dr = (instr >> 9) & 0x7;
                    // first operand (SR1)
                    uint16_t sr1 = (instr >> 6) & 0x7;
                    // immediate flag
                    uint16_t imm_flag = (instr >> 5) & 0x1;

                    if (imm_flag)
                    {
                        uint16_t imm5 = sign_extend(instr & 0x1F, 5);
                        reg[dr] = reg[sr1] & imm5;
                    }
                    else
                    {
                        uint16_t sr2 = instr & 0x7;
                        reg[dr] = reg[sr1] & reg[sr2];
                    }

                    update_flags(r0);
                }
                break;
            case OP_NOT:
                {
                    uint16_t dr = (instr >> 9) & 0x7;
                    uint16_t sr = (instr >> 9) & 0x7;
                    reg[dr] = ~reg[sr];
                    update_flags(dr);
                }
                break;
            case OP_BR:
                {
                    uint16_t cond_flag = (instr >> 9) & 0x7;
                    uint16_t pc_offset = sign_extend(instr & 0x1ff, 9);
                    if (cond_flag & reg[R_COND])
                    {
                        reg[R_PC] += pc_offset;
                    }
                }
                break;
            case OP_JMP:
                {
                    uint16_t br = (instr >> 6) & 0x7;
                    reg[R_PC] = reg[br];
                }
                break;
            case OP_JSR:
                {
                    // Store current PC and then jump to subroutine
                    reg[R_R7] = reg[R_PC];
                    // flag for JSR or JSRR
                    uint16_t flag = (instr >> 11) & 1;
                    if (flag)
                    {
                        // Get the extended pc offset
                        reg[R_PC] = sign_extend(instr & 0x7ff, 11);
                    }
                    else
                    {
                        // get the BaseR
                        uint16_t br = (instr >> 6) & 0x7;
                        reg[R_PC] = reg[br];
                    }
                }
                break;
            case OP_LD:
                {
                    uint16_t dr = (instr >> 9) & 0x7;
                    uint16_t pc_offset = sign_extend(instr & 0x1ff, 9);
                    reg[dr] = mem_read(reg[R_PC] + pc_offset);
                    update_flags(dr);
                }
                break;
            case OP_LDI:
                {
                    /* destination register (DR) */
                    uint16_t dr = (instr >> 9) & 0x7;
                    /* PCoffset 9*/
                    uint16_t pc_offset = sign_extend(instr & 0x1ff, 9);
                    /* add pc_offset to the current PC, look at that memory location to get the final address */
                    reg[dr] = mem_read(mem_read(reg[R_PC] + pc_offset));
                    update_flags(dr);
                }
                break;
            case OP_LDR:
                {
                    uint16_t dr = (instr >> 9) & 0x7;
                    uint16_t br = (instr >> 6) & 0x7;
                    uint16_t offset = sign_extend(instr & 0x3F, 6);
                    reg[dr] = mem_read(reg[br] + offset);
                    update_flags(dr);
                }
                break;
            case OP_LEA:
                {LEA, 7}
                break;
            case OP_ST:
                {ST, 7}
                break;
            case OP_STI:
                {STI, 7}
                break;
            case OP_STR:
                {STR, 7}
                break;
            case OP_TRAP:
                {TRAP, 8}
                break;
            case OP_RES:
            case OP_RTI:
            default:
                {BAD OPCODE, 7}
                break;
        }
    }
    {Shutdown, 12}
}
