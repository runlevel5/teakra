#pragma once

#include <array>
#include <memory>
#include <vector>
#include "common_types.h"
#include "oprand.h"


struct RegisterState {
    void Reset() {
        pc = 0;

    }

    u32 pc;

    u16 GetPcL() {
        return pc & 0xFFFF;
    }
    u16 GetPcH() {
        return (pc >> 16) & 0xFFFF;
    }
    void SetPC(u16 low, u16 high) {
        pc = (u32)low | ((u32)high << 16);
    }

    u16 dvm;
    u16 repc;
    u16 lc;
    u16 mixp;
    u16 sv;
    u16 sp;

    std::array<u16, 8> r;

    struct Accumulator {
        // 40-bit 2's comp on real TeakLite.
        // Use 64-bit 2's comp here. The upper 24 bits are always sign extension
        u64 value;
    };
    std::array<Accumulator, 2> a;
    std::array<Accumulator, 2> b;

    struct Product {
        u32 value;
    };
    std::array<u16, 2> x;
    std::array<u16, 2> y;
    std::array<Product, 2> p;

    std::array<u16, 2> ar; // ?
    std::array<u16, 4> arp; // ?
    u16 stepi0, stepj0; // alternative step
    std::array<u16, 2> vtr; // fc/fc1 latching

    class RegisterProxy {
    public:
        virtual ~RegisterProxy() = default;
        virtual u16 Get() = 0;
        virtual void Set(u16 value) = 0;
    };

    class Redirector : public RegisterProxy {
    public:
        Redirector(u16& target) : target(target) {}
        u16 Get() override {
            return target;
        }
        void Set(u16 value) override {
            target = value;
        }
    private:
        u16& target;
    };

    class DoubleRedirector : public RegisterProxy {
    public:
        DoubleRedirector(u16& target0, u16& target1) : target0(target0), target1(target1) {}
        u16 Get() override {
            return target0 | target1;
        }
        void Set(u16 value) override {
            target0 = target1 = value;
        }
    private:
        u16& target0, target1;
    };

    class RORedirector : public Redirector {
        using Redirector::Redirector;
        void Set(u16 value) override {}
    };

    class AccEProxy : public RegisterProxy {
    public:
        AccEProxy(Accumulator& target) : target(target) {}
        u16 Get() override {
            return (u16)((target.value >> 32) & 0xF);
        }
        void Set(u16 value) override {
            u32 value32 = SignExtend<4>((u32)value);
            target.value &= 0xFFFFFFFF;
            target.value |= (u64)value32 << 32;
        }
    private:
        Accumulator& target;
    };

    struct ProxySlot {
        std::shared_ptr<RegisterProxy> proxy;
        unsigned position;
        unsigned length;
    };

    struct PseudoRegister {
        std::vector<ProxySlot> slots;

        u16 Get() {
            u16 result = 0;
            for (const auto& slot : slots) {
                result |= slot.proxy->Get() << slot.position;
            }
            return result;
        }
        void Set(u16 value) {
            for (const auto& slot : slots) {
                slot.proxy->Set((value >> slot.position) & ((1 << slot.length) - 1));
            }
        }
    };

    u16 stepi, stepj; // 7 bit 2's comp
    u16 modi, modj; // 9 bit

    PseudoRegister cfgi {{
        {std::make_shared<Redirector>(stepi), 0, 7},
        {std::make_shared<Redirector>(modi), 7, 9},
    }};
    PseudoRegister cfgj {{
        {std::make_shared<Redirector>(stepj), 0, 7},
        {std::make_shared<Redirector>(modj), 7, 9},
    }};

    u16 fz, fm, fn, fv, fc, fe;
    std::array<u16, 2> fl; // when is fl[1] used?
    u16 fr;
    u16 fc1; // used by max||max||vtrshr?
    u16 nimc;
    std::array<u16, 3> ip;
    u16 vip;
    std::array<u16, 3> im;
    u16 vim;
    std::array<u16, 3> ic;
    u16 vic;
    u16 ie;
    u16 movpd;
    u16 bcn;
    u16 lp;
    std::array<u16, 2> sar; // sar[0]=1 disable saturation when read from acc; sar[1]=1 disable saturation when write to acc?
    std::array<u16, 2> ps;
    std::array<u16, 2> psm; // product shift mode. 0: logic; 1: arithmatic?
    u16 s;
    std::array<u16, 2> ou;
    std::array<u16, 2> iu;
    u16 page;

    // m=0, ms=0: use stepi/j (no modulo)
    // m=1, ms=0: use stepi/j with modulo
    // m=0, ms=1: use stepi0/j0 (no modulo)
    // m=1, ms=1: use stepi/j  (no modulo)??
    std::array<u16, 8> m;
    std::array<u16, 8> ms;

    PseudoRegister stt0 {{
        {std::make_shared<Redirector>(fl[0]), 0, 1},
        {std::make_shared<Redirector>(fl[1]), 1, 1},
        {std::make_shared<Redirector>(fe), 2, 1},
        {std::make_shared<Redirector>(fc), 3, 1},
        {std::make_shared<Redirector>(fv), 4, 1},
        {std::make_shared<Redirector>(fn), 5, 1},
        {std::make_shared<Redirector>(fm), 6, 1},
        {std::make_shared<Redirector>(fz), 7, 1},
        {std::make_shared<Redirector>(fc1), 11, 1},
    }};
    PseudoRegister stt1 {{
        {std::make_shared<Redirector>(fr), 4, 1},

        {std::make_shared<Redirector>(psm[0]), 14, 1},
        {std::make_shared<Redirector>(psm[1]), 15, 1},
    }};
    PseudoRegister stt2 {{
        {std::make_shared<RORedirector>(ip[0]), 0, 1},
        {std::make_shared<RORedirector>(ip[1]), 1, 1},
        {std::make_shared<RORedirector>(ip[2]), 2, 1},
        {std::make_shared<RORedirector>(vip), 3, 1},

        {std::make_shared<Redirector>(movpd), 6, 2},

        {std::make_shared<RORedirector>(bcn), 12, 3},
        {std::make_shared<RORedirector>(lp), 15, 1},
    }};
    PseudoRegister mod0 {{
        {std::make_shared<Redirector>(sar[0]), 0, 1},
        {std::make_shared<Redirector>(sar[1]), 1, 1},
        //{std::make_shared<RORedirector>(??), 2, 2},

        {std::make_shared<Redirector>(s), 7, 1},
        {std::make_shared<Redirector>(ou[0]), 8, 1},
        {std::make_shared<Redirector>(ou[0]), 9, 1},
        {std::make_shared<Redirector>(ps[0]), 10, 2},

        {std::make_shared<Redirector>(ps[1]), 13, 2},
    }};
    PseudoRegister mod1 {{
        {std::make_shared<Redirector>(page), 0, 8},
    }};
    PseudoRegister mod2 {{
        {std::make_shared<Redirector>(m[0]), 0, 1},
        {std::make_shared<Redirector>(m[1]), 1, 1},
        {std::make_shared<Redirector>(m[2]), 2, 1},
        {std::make_shared<Redirector>(m[3]), 3, 1},
        {std::make_shared<Redirector>(m[4]), 4, 1},
        {std::make_shared<Redirector>(m[5]), 5, 1},
        {std::make_shared<Redirector>(m[6]), 6, 1},
        {std::make_shared<Redirector>(m[7]), 7, 1},
        {std::make_shared<Redirector>(ms[0]), 8, 1},
        {std::make_shared<Redirector>(ms[1]), 9, 1},
        {std::make_shared<Redirector>(ms[2]), 10, 1},
        {std::make_shared<Redirector>(ms[3]), 11, 1},
        {std::make_shared<Redirector>(ms[4]), 12, 1},
        {std::make_shared<Redirector>(ms[5]), 13, 1},
        {std::make_shared<Redirector>(ms[6]), 14, 1},
        {std::make_shared<Redirector>(ms[7]), 15, 1},
    }};
    PseudoRegister mod3 {{
        {std::make_shared<Redirector>(nimc), 0, 1},
        {std::make_shared<Redirector>(ic[0]), 1, 1},
        {std::make_shared<Redirector>(ic[1]), 2, 1},
        {std::make_shared<Redirector>(ic[2]), 3, 1},
        {std::make_shared<Redirector>(vic), 4, 1}, // ?

        {std::make_shared<Redirector>(ie), 7, 1},
        {std::make_shared<Redirector>(im[0]), 8, 1},
        {std::make_shared<Redirector>(im[1]), 9, 1},
        {std::make_shared<Redirector>(im[2]), 10, 1},
        {std::make_shared<Redirector>(vim), 11, 1}, // ?
    }};

    PseudoRegister st0 {{
        {std::make_shared<Redirector>(sar[0]), 0, 1},
        {std::make_shared<Redirector>(ie), 1, 1},
        {std::make_shared<Redirector>(im[0]), 2, 1},
        {std::make_shared<Redirector>(im[1]), 3, 1},
        {std::make_shared<Redirector>(fr), 4, 1},
        {std::make_shared<DoubleRedirector>(fl[0], fl[1]), 5, 1},
        {std::make_shared<Redirector>(fe), 6, 1},
        {std::make_shared<Redirector>(fc), 7, 1},
        {std::make_shared<Redirector>(fv), 8, 1},
        {std::make_shared<Redirector>(fn), 9, 1},
        {std::make_shared<Redirector>(fm), 10, 1},
        {std::make_shared<Redirector>(fz), 11, 1},
        {std::make_shared<AccEProxy>(a[01]), 12, 4},
    }};
    PseudoRegister st1 {{
        {std::make_shared<Redirector>(page), 0, 8},
        // 8, 9: reserved
        {std::make_shared<Redirector>(ps[0]), 10, 2},
        {std::make_shared<AccEProxy>(a[1]), 12, 4},
    }};
    PseudoRegister st2 {{
        {std::make_shared<Redirector>(m[0]), 0, 1},
        {std::make_shared<Redirector>(m[1]), 1, 1},
        {std::make_shared<Redirector>(m[2]), 2, 1},
        {std::make_shared<Redirector>(m[3]), 3, 1},
        {std::make_shared<Redirector>(m[4]), 4, 1},
        {std::make_shared<Redirector>(m[5]), 5, 1},
        {std::make_shared<Redirector>(im[2]), 6, 1},
        {std::make_shared<Redirector>(s), 7, 1},
        {std::make_shared<Redirector>(ou[0]), 8, 1},
        {std::make_shared<Redirector>(ou[1]), 9, 1},
        {std::make_shared<RORedirector>(iu[0]), 10, 1},
        {std::make_shared<RORedirector>(iu[1]), 11, 1},
        // 12: reserved
        {std::make_shared<RORedirector>(ip[2]), 13, 1},
        {std::make_shared<RORedirector>(ip[0]), 14, 1},
        {std::make_shared<RORedirector>(ip[1]), 15, 1},
    }};
    PseudoRegister icr {{
        {std::make_shared<Redirector>(nimc), 0, 1},
        {std::make_shared<Redirector>(ic[0]), 1, 1},
        {std::make_shared<Redirector>(ic[1]), 2, 1},
        {std::make_shared<Redirector>(ic[2]), 3, 1},
        {std::make_shared<RORedirector>(lp), 4, 1},
        {std::make_shared<RORedirector>(bcn), 5, 3},
        // reserved
    }};

    bool ConditionPass(Cond cond) {
        switch(cond.GetName()) {
        case CondValue::True: return true;
        case CondValue::Eq: return fz == 1;
        case CondValue::Neq: return fz == 0;
        case CondValue::Gt: return fz == 0 && fm == 0;
        case CondValue::Ge: return fm == 0;
        case CondValue::Lt: return fm == 1;
        case CondValue::Le: return fm == 1 || fz == 1;
        case CondValue::Nn: return fn == 0;
        case CondValue::C: return fc == 1; // ?
        case CondValue::V: return fv == 1;
        case CondValue::E: return fe == 1;
        case CondValue::L: return fl[0] == 1 || fl[1] == 1; // ??
        case CondValue::Nr: return fr == 0;
        case CondValue::Niu0: return iu[0] == 0;
        case CondValue::Iu0: return iu[0] == 1;
        case CondValue::Iu1: return iu[1] == 1;
        default: throw "";
        }
    }

};
