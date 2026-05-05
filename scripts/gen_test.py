#!/usr/bin/env python3
"""
gen_test.py — compile JSON test scripts to gcio-sim bytecode.

A test JSON is a list of commands (strings). Sugar commands expand to multiple
low-level opcodes. The compiler tracks a running GC poll snapshot so each
`poll` emits the buffer state implied by the most recent `gc ...` lines.

Low-level commands:
  label <text>                    set the current label for following checks
  gc clear                        zero the staged GC poll buffer
  gc <field> <value>              stage a GC field (button, stick, trigger)
  poll                            emit GC + POLL
  out <0|1>                       drive OUT pin (latch / data line)
  clk                             pulse CLK rising edge
  expect_d0 <0|1>                 assert D0 pin state
  expect_buf target|output <hex...>  assert buffer matches 8 hex bytes
  end                             explicit end marker (also implicit at EOF)

Sugar:
  cmd <N>                         out 1 ; clk*N ; out 0   — selects task=N
  read_legacy <BITS>              cmd 0 ; expect_bits BITS  (8 bit pattern)
  expect_bits <BITS>              for each bit: clk ; expect_d0 <bit>
  write_bits <BITS>               for each bit: out <bit> ; clk
                                    (used during BEHAVE/INMASK/INVERT/LSETUP)
  send_cmd <task> <BITS>          cmd <task> ; write_bits BITS

Field names (gc):
  buttons (bool, 0|1):
    a b x y start                 (in arr[0])
    left right down up z r l      (in arr[1])
  sticks (vec):
    lstick <x> <y>                (sets arr[2..3])
    cstick <x> <y>                (sets arr[4..5])
  triggers (uint8):
    lt <v>                        (arr[6])
    rt <v>                        (arr[7])
"""

import json
import os
import sys


# Bytecode opcodes — keep in sync with sim/main.cpp
OP_GC      = 0x01
OP_POLL    = 0x02
OP_OUT     = 0x03
OP_CLK     = 0x04
OP_EXP_D0  = 0x05
OP_LABEL   = 0x06
OP_EXP_BUF = 0x07
OP_ORIGIN  = 0x08
OP_EXP_RUM = 0x09
OP_END     = 0xFF

# Bit positions WITHIN their byte (matches src/main.h EInButtons after the
# user's recent edit where ILeft..IL are 1<<0..1<<6 of arr[1]).
GC_BUTTONS = {
    # arr[0]
    'a':     ('arr0', 0),
    'b':     ('arr0', 1),
    'x':     ('arr0', 2),
    'y':     ('arr0', 3),
    'start': ('arr0', 4),
    # arr[1]
    'left':  ('arr1', 0),
    'right': ('arr1', 1),
    'down':  ('arr1', 2),
    'up':    ('arr1', 3),
    'z':     ('arr1', 4),
    'r':     ('arr1', 5),
    'l':     ('arr1', 6),
}

TASKS = {
    'legacy': 0, 'report': 1, 'behave': 2,
    'invert': 3, 'rumble': 4, 'dezone': 5, 'lsetup': 6,
}


class Compiler:
    def __init__(self):
        self.bc: bytearray   = bytearray()
        self.gc: bytearray   = bytearray(8)   # staged GC poll snapshot

    # --- emitters ---
    def emit(self, *bs: int) -> None:
        for b in bs:
            if not 0 <= b <= 0xFF:
                raise ValueError(f"byte out of range: {b}")
            self.bc.append(b)

    def emit_label(self, text: str) -> None:
        data = text.encode('utf-8')
        if len(data) > 255:
            raise ValueError(f"label too long: {len(data)}")
        self.emit(OP_LABEL, len(data))
        self.bc.extend(data)

    def emit_gc(self) -> None:
        self.emit(OP_GC)
        self.bc.extend(self.gc)

    def emit_buf_check(self, which: str, hex_bytes: list[str]) -> None:
        if which not in ('target', 'output'):
            raise ValueError(f"expect_buf: which must be target|output, got {which}")
        if len(hex_bytes) != 8:
            raise ValueError(f"expect_buf: need 8 bytes, got {len(hex_bytes)}")
        self.emit(OP_EXP_BUF, 0 if which == 'target' else 1)
        for h in hex_bytes:
            self.emit(int(h, 16))

    # --- gc state mutators ---
    def gc_clear(self) -> None:
        self.gc = bytearray(8)

    def gc_set(self, field: str, args: list[str]) -> None:
        if field in GC_BUTTONS:
            if len(args) != 1:
                raise ValueError(f"gc {field}: expected 0|1")
            arr_key, bit = GC_BUTTONS[field]
            idx = 0 if arr_key == 'arr0' else 1
            v = int(args[0])
            if v: self.gc[idx] |=  (1 << bit)
            else: self.gc[idx] &= ~(1 << bit) & 0xFF
        elif field == 'bit':
            # gc bit <byte_idx> <bit_idx> <0|1> — direct status-bit poke
            if len(args) != 3: raise ValueError("gc bit: byte_idx bit_idx 0|1")
            bidx = int(args[0]); bit = int(args[1]); v = int(args[2])
            if v: self.gc[bidx] |=  (1 << bit)
            else: self.gc[bidx] &= ~(1 << bit) & 0xFF
        elif field == 'lstick':
            if len(args) != 2: raise ValueError("gc lstick: expected x y")
            self.gc[2] = int(args[0]) & 0xFF
            self.gc[3] = int(args[1]) & 0xFF
        elif field == 'cstick':
            if len(args) != 2: raise ValueError("gc cstick: expected x y")
            self.gc[4] = int(args[0]) & 0xFF
            self.gc[5] = int(args[1]) & 0xFF
        elif field == 'lt':
            if len(args) != 1: raise ValueError("gc lt: expected v")
            self.gc[6] = int(args[0]) & 0xFF
        elif field == 'rt':
            if len(args) != 1: raise ValueError("gc rt: expected v")
            self.gc[7] = int(args[0]) & 0xFF
        else:
            raise ValueError(f"unknown gc field: {field}")

    # --- top-level dispatch ---
    def compile(self, src: list[str]) -> bytes:
        for line_no, raw in enumerate(src, 1):
            try:
                self._line(raw)
            except Exception as e:
                raise RuntimeError(f"line {line_no} ({raw!r}): {e}") from e
        self.emit(OP_END)
        return bytes(self.bc)

    def _line(self, raw: str) -> None:
        toks = raw.strip().split()
        if not toks or toks[0].startswith('#'):
            return
        op, *args = toks

        if op == 'label':
            self.emit_label(' '.join(args))

        elif op == 'gc':
            if not args: raise ValueError("gc: missing field")
            if args[0] == 'clear':
                self.gc_clear()
            else:
                self.gc_set(args[0], args[1:])

        elif op == 'poll':
            self.emit_gc()
            self.emit(OP_POLL)

        elif op == 'origin':
            # commit the staged GC state as the stored stick origin
            self.emit(OP_ORIGIN)
            self.bc.extend(self.gc)

        elif op == 'out':
            if len(args) != 1: raise ValueError("out: 0|1")
            self.emit(OP_OUT, int(args[0]) & 1)

        elif op == 'clk':
            n = int(args[0]) if args else 1
            for _ in range(n):
                self.emit(OP_CLK)

        elif op == 'expect_d0':
            if len(args) != 1: raise ValueError("expect_d0: 0|1")
            self.emit(OP_EXP_D0, int(args[0]) & 1)

        elif op == 'expect_buf':
            if len(args) < 2: raise ValueError("expect_buf: target|output b0 b1 ...")
            self.emit_buf_check(args[0], args[1:9])

        elif op == 'expect_rumble':
            if len(args) != 1: raise ValueError("expect_rumble: 0|1")
            self.emit(OP_EXP_RUM, int(args[0]) & 1)

        elif op == 'cmd':
            # idle low, latch high, N command-clocks during latch high, latch low.
            # Leading out 0 forces a clean rising edge if a prior write left OUT=1.
            if len(args) != 1: raise ValueError("cmd: <task-index-or-name>")
            n = TASKS.get(args[0], None)
            if n is None: n = int(args[0])
            self.emit(OP_OUT, 0)
            self.emit(OP_OUT, 1)
            for _ in range(n):
                self.emit(OP_CLK)
            self.emit(OP_OUT, 0)

        elif op == 'expect_bits':
            if len(args) != 1: raise ValueError("expect_bits: BITS")
            for ch in args[0]:
                if ch not in '01': raise ValueError(f"bad bit char: {ch!r}")
                self.emit(OP_CLK)
                self.emit(OP_EXP_D0, 1 if ch == '1' else 0)

        elif op == 'read_legacy':
            if len(args) != 1: raise ValueError("read_legacy: 8 bits")
            if len(args[0]) != 8: raise ValueError("read_legacy: need exactly 8 bits")
            # cmd 0 + expect_bits — leading out 0 forces clean rising edge
            self.emit(OP_OUT, 0)
            self.emit(OP_OUT, 1)
            self.emit(OP_OUT, 0)
            for ch in args[0]:
                self.emit(OP_CLK)
                self.emit(OP_EXP_D0, 1 if ch == '1' else 0)

        elif op == 'write_bits':
            # During non-LEGACY data phase, the game drives OUT to send each bit
            # then strobes CLK. This will trigger OUT-edge ISRs — that's the
            # current firmware behavior, which the test exposes.
            if len(args) != 1: raise ValueError("write_bits: BITS")
            for ch in args[0]:
                if ch not in '01': raise ValueError(f"bad bit char: {ch!r}")
                self.emit(OP_OUT, 1 if ch == '1' else 0)
                self.emit(OP_CLK)

        elif op == 'send_cmd':
            # cmd <task> ; write_bits BITS — leading out 0 ensures fresh rising
            if len(args) != 2: raise ValueError("send_cmd: <task> BITS")
            n = TASKS.get(args[0], None)
            if n is None: n = int(args[0])
            self.emit(OP_OUT, 0)
            self.emit(OP_OUT, 1)
            for _ in range(n):
                self.emit(OP_CLK)
            self.emit(OP_OUT, 0)
            for ch in args[1]:
                if ch not in '01': raise ValueError(f"bad bit char: {ch!r}")
                self.emit(OP_OUT, 1 if ch == '1' else 0)
                self.emit(OP_CLK)

        elif op == 'end':
            pass  # END is appended unconditionally

        else:
            raise ValueError(f"unknown op: {op}")


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print("usage: gen_test.py <test.json> [<test.json> ...]", file=sys.stderr)
        return 2
    rc = 0
    for path in argv[1:]:
        with open(path, 'r', encoding='utf-8') as f:
            src = json.load(f)
        if not isinstance(src, list):
            print(f"{path}: top-level must be a JSON list of strings", file=sys.stderr)
            rc = 1
            continue
        bc = Compiler().compile([str(x) for x in src])
        out_path = os.path.splitext(path)[0] + '.bc'
        with open(out_path, 'wb') as f:
            f.write(bc)
        print(f"{path} -> {out_path} ({len(bc)} bytes)")
    return rc


if __name__ == "__main__":
    sys.exit(main(sys.argv))
