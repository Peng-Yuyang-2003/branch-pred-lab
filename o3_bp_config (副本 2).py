import argparse
import os
import sys

import m5
from m5.objects import *


def add_common_path():
    this_dir = os.path.dirname(os.path.realpath(__file__))
    gem5_root = os.path.abspath(os.path.join(this_dir, "../.."))
    sys.path.append(gem5_root)


add_common_path()


class L1Cache(Cache):
    assoc = 2
    tag_latency = 2
    data_latency = 2
    response_latency = 2
    mshrs = 8
    tgts_per_mshr = 20

    def connect_bus(self, bus):
        self.mem_side = bus.cpu_side_ports


class L1ICache(L1Cache):
    def __init__(self, size):
        super().__init__()
        self.size = size

    def connect_cpu(self, cpu):
        self.cpu_side = cpu.icache_port


class L1DCache(L1Cache):
    def __init__(self, size):
        super().__init__()
        self.size = size

    def connect_cpu(self, cpu):
        self.cpu_side = cpu.dcache_port


class L2Cache(Cache):
    assoc = 8
    tag_latency = 20
    data_latency = 20
    response_latency = 20
    mshrs = 20
    tgts_per_mshr = 12

    def __init__(self, size):
        super().__init__()
        self.size = size

    def connect_cpu_side_bus(self, bus):
        self.cpu_side = bus.mem_side_ports

    def connect_mem_side_bus(self, bus):
        self.mem_side = bus.cpu_side_ports


def parse_args():
    parser = argparse.ArgumentParser(description="Run an X86 binary on a two-level-cache CPU.")
    parser.add_argument("--binary", required=True, help="X86 binary to run")
    parser.add_argument("--program-args", default="", help="Arguments passed to the binary")
    parser.add_argument("--clock", default="2GHz")
    parser.add_argument("--mem-size", default="512MiB")
    parser.add_argument("--l1i-size", default="32KiB")
    parser.add_argument("--l1d-size", default="32KiB")
    parser.add_argument("--l2-size", default="512KiB")
    parser.add_argument("--bp-type", choices=["none", "local", "tournament", "bimode", "tage"], default="local")
    parser.add_argument("--local-size", type=int, default=2048)
    parser.add_argument("--local-hist-size", type=int, default=2048)
    parser.add_argument("--global-size", type=int, default=8192)
    parser.add_argument("--choice-size", type=int, default=8192)
    parser.add_argument("--ctr-bits", type=int, default=2)
    parser.add_argument("--tage-tables", type=int, default=7)
    parser.add_argument("--tage-min-hist", type=int, default=5)
    parser.add_argument("--tage-max-hist", type=int, default=130)
    return parser.parse_args()


def build_branch_predictor(args):
    if args.bp_type == "none":
        return BranchPredictor(
            conditionalBranchPred=LocalBP(localPredictorSize=1, localCtrBits=1),
            btb=SimpleBTB(numEntries=1),
            ras=ReturnAddrStack(numEntries=1),
        )
    if args.bp_type == "local":
        return BranchPredictor(
            conditionalBranchPred=LocalBP(
                localPredictorSize=args.local_size,
                localCtrBits=args.ctr_bits,
            )
        )
    raise ValueError(f"unsupported bp type: {args.bp_type}")


def main():
    args = parse_args()

    system = System()
    system.clk_domain = SrcClockDomain(clock=args.clock, voltage_domain=VoltageDomain())
    system.mem_mode = "timing"
    system.mem_ranges = [AddrRange(args.mem_size)]

    system.cpu = X86O3CPU()
    system.cpu.branchPred = build_branch_predictor(args)

    system.cpu.icache = L1ICache(args.l1i_size)
    system.cpu.dcache = L1DCache(args.l1d_size)
    system.cpu.icache.connect_cpu(system.cpu)
    system.cpu.dcache.connect_cpu(system.cpu)

    system.l2bus = L2XBar()
    system.cpu.icache.connect_bus(system.l2bus)
    system.cpu.dcache.connect_bus(system.l2bus)

    system.l2cache = L2Cache(args.l2_size)
    system.l2cache.connect_cpu_side_bus(system.l2bus)

    system.membus = SystemXBar()
    system.l2cache.connect_mem_side_bus(system.membus)

    system.cpu.createInterruptController()
    system.cpu.interrupts[0].pio = system.membus.mem_side_ports
    system.cpu.interrupts[0].int_requestor = system.membus.cpu_side_ports
    system.cpu.interrupts[0].int_responder = system.membus.mem_side_ports
    system.system_port = system.membus.cpu_side_ports

    system.mem_ctrl = MemCtrl()
    system.mem_ctrl.dram = DDR3_1600_8x8()
    system.mem_ctrl.dram.range = system.mem_ranges[0]
    system.mem_ctrl.port = system.membus.mem_side_ports

    process = Process()
    process.cmd = [args.binary] + args.program_args.split()
    system.workload = SEWorkload.init_compatible(args.binary)
    system.cpu.workload = process
    system.cpu.createThreads()

    root = Root(full_system=False, system=system)
    m5.instantiate()

    print(f"Starting simulation: cmd={' '.join(process.cmd)}")
    exit_event = m5.simulate()
    print(f"Exiting @ tick {m5.curTick()} because {exit_event.getCause()}")


if __name__ == "__m5_main__":
    main()
