#include "cpu.h"

using gameboy::emulator::CPU;

unsigned CPU::next_step(const MMU& mmu) noexcept
{
    if (interrupt_master_enable)
    {
        const auto mask = IF & IE & 0x1F;
        if (mask)
        {
            interrupt_master_enable = false;
            for (unsigned i = 0; i < 5; ++i)
            {
                if (mask >> i & 1)
                {
                    static constexpr unsigned values[]{0x0040, 0x0048, 0x0050, 0x0058, 0x0060};
                    mmu.write_word(regs.SP -= 2, regs.PC); regs.PC = values[i];
                    IF &= ~(1 << i);
                }
            }
            return 5;
        }
    }
    // build a table of opcodes
    static constexpr unsigned (*insts[512])(CPU&, const MMU&) noexcept
    {
#define a(p,n) ExecuteInstruction<p, n    >::exec,   \
               ExecuteInstruction<p, n + 1>::exec,

#define b(p,n) a(p,n)a(p,n+2)a(p,n+04)a(p,n+06)//vorecci
#define c(p,n) b(p,n)b(p,n+8)b(p,n+16)b(p,n+24)b(p,n+32)b(p,n+40)b(p,n+48)b(p,n+56)

        c(0,0)c(0,64)c(0,128)c(0,192)
        c(1,0)c(1,64)c(1,128)c(1,192)

#undef c
#undef b
#undef a
    };

    unsigned opcode = mmu.read_byte(regs.PC++);
    if ((opcode & 0xFF) == 0xCB)
         opcode = mmu.read_byte(regs.PC++) + 256;

    //std::clog << regs.PC << std::endl;

    return (insts[opcode])(*this, mmu);
}

