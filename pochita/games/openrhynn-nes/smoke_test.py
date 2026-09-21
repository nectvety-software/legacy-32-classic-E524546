from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parent
WORKSPACE = ROOT.parent.parent
sys.path.insert(0, str(WORKSPACE / "tools" / "py65"))

from py65.devices.mpu6502 import MPU


class NesMemory:
    def __init__(self, rom):
        self.data = bytearray(65536)
        self.data[0x8000:0x10000] = rom[16:16 + 32768]
        self.pad = 0
        self.pad_index = 0
        self.strobe = 0

    def __getitem__(self, address):
        address &= 0xFFFF
        if address == 0x2002:
            return 0x80
        if address == 0x4016:
            if self.strobe:
                return self.pad & 1
            value = (self.pad >> self.pad_index) & 1 if self.pad_index < 8 else 1
            self.pad_index += 1
            return value
        return self.data[address]

    def __setitem__(self, address, value):
        address &= 0xFFFF
        value &= 0xFF
        if address == 0x4016:
            old = self.strobe
            self.strobe = value & 1
            if old and not self.strobe:
                self.pad_index = 0
        self.data[address] = value


def main():
    rom = (ROOT / "OpenRhynn.nes").read_bytes()
    assert rom[:4] == b"NES\x1a"
    memory = NesMemory(rom)
    cpu = MPU(memory=memory, pc=None)

    # Run reset/title, then pulse Start through the controller and keep
    # producing vblank NMIs. This catches bad vectors, illegal control flow,
    # broken stack handling and basic input/game-loop regressions.
    for frame in range(180):
        memory.pad = 0x08 if 8 <= frame < 12 else 0  # Native NES Start bit.
        cpu.nmi()
        for _ in range(1800):
            cpu.step()
            if not (0x8000 <= cpu.pc <= 0xFFFF):
                raise AssertionError(f"PC escaped PRG ROM: 0x{cpu.pc:04X}")

    assert bytes(memory.data[0x6200:0x6204]) == b"RHY1"
    assert bytes(memory.data[0x6000:0x6011]) == b"OPENRHYNN_SAVE_V1"
    assert memory.data[0x0200] != 0xF8, "Player sprite stayed hidden after Start"
    print(f"Smoke test passed: PC=0x{cpu.pc:04X}, SRAM magic=RHY1")


if __name__ == "__main__":
    main()

