#include <cstdlib>
#include <ctime>

#include "basic_timer.h"
#include "core/common/clint.h"
#include "display.hpp"
#include "dma.h"
#include "elf_loader.h"
#include "ethernet.h"
#include "fe310_plic.h"
#include "flash.h"
#include "debug_memory.h"
#include "iss.h"
#include "mem.h"
#include "memory.h"
#include "memory_mapped_file.h"
#include "sensor.h"
#include "sensor2.h"
#include "syscall.h"
#include "uart.h"
#include "lenet.h" //Add
#include "util/options.h"
#include "platform/common/options.h"
#include "platform/common/terminal.h"
#include "gdb-mc/gdb_server.h"
#include "gdb-mc/gdb_runner.h"

#include <boost/io/ios_state.hpp>
#include <boost/program_options.hpp>
#include <iomanip>
#include <iostream>


using namespace rv32;
namespace po = boost::program_options;

class BasicOptions : public Options {
public:
	typedef unsigned int addr_t;

	std::string mram_image;
	std::string flash_device;
	std::string network_device;
	std::string test_signature;

	addr_t mem_size = 1024 * 1024 * 32;  // 32 MB ram, to place it before the CLINT and run the base examples (assume
	                                     // memory start at zero) without modifications
	addr_t mem_start_addr = 0x00000000;
	addr_t mem_end_addr = mem_start_addr + mem_size - 1;
	addr_t clint_start_addr = 0x02000000;
	addr_t clint_end_addr = 0x0200ffff;
	addr_t sys_start_addr = 0x02010000;
	addr_t sys_end_addr = 0x020103ff;
	addr_t term_start_addr = 0x20000000;
	addr_t term_end_addr = term_start_addr + 16;
	addr_t uart_start_addr = 0x20010000;
	addr_t uart_end_addr = uart_start_addr + 0xfff;
	addr_t ethernet_start_addr = 0x30000000;
	addr_t ethernet_end_addr = ethernet_start_addr + 1500;
	addr_t plic_start_addr = 0x40000000;
	addr_t plic_end_addr = 0x41000000;
	addr_t sensor_start_addr = 0x50000000;
	addr_t sensor_end_addr = 0x50001000;
	addr_t sensor2_start_addr = 0x50002000;
	addr_t sensor2_end_addr = 0x50004000;
	addr_t mram_start_addr = 0x60000000;
	addr_t mram_size = 0x10000000;
	addr_t mram_end_addr = mram_start_addr + mram_size - 1;
	addr_t dma_start_addr = 0x70000000;
	addr_t dma_end_addr = 0x70001000;
	addr_t flash_start_addr = 0x71000000;
	addr_t flash_end_addr = flash_start_addr + Flashcontroller::ADDR_SPACE;  // Usually 528 Byte
	addr_t display_start_addr = 0x72000000;
	addr_t display_end_addr = display_start_addr + Display::addressRange;
	//Add
	addr_t lenetFilter_start_addr = 0x73000000;
 	addr_t lenetFilter_size = 0x01000000;
 	addr_t lenetFilter_end_addr = lenetFilter_start_addr + lenetFilter_size - 1;
	
	addr_t lenetFilter2_start_addr = 0x74000000;
 	addr_t lenetFilter2_end_addr = lenetFilter2_start_addr + lenetFilter_size - 1;

	addr_t lenetFilter3_start_addr = 0x75000000;
 	addr_t lenetFilter3_end_addr = lenetFilter3_start_addr + lenetFilter_size - 1;

	addr_t lenetFilter4_start_addr = 0x76000000;
 	addr_t lenetFilter4_end_addr = lenetFilter4_start_addr + lenetFilter_size - 1;

	bool quiet = false;
	bool use_E_base_isa = false;

	OptionValue<unsigned long> entry_point;

	BasicOptions(void) {
        	// clang-format off
		add_options()
			("quiet", po::bool_switch(&quiet), "do not output register values on exit")
			("memory-start", po::value<unsigned int>(&mem_start_addr),"set memory start address")
			("memory-size", po::value<unsigned int>(&mem_size), "set memory size")
			("use-E-base-isa", po::bool_switch(&use_E_base_isa), "use the E instead of the I integer base ISA")
			("entry-point", po::value<std::string>(&entry_point.option),"set entry point address (ISS program counter)")
			("mram-image", po::value<std::string>(&mram_image)->default_value(""),"MRAM image file for persistency")
			("mram-image-size", po::value<unsigned int>(&mram_size), "MRAM image size")
			("flash-device", po::value<std::string>(&flash_device)->default_value(""),"blockdevice for flash emulation")
			("network-device", po::value<std::string>(&network_device)->default_value(""),"name of the tap network adapter, e.g. /dev/tap6")
			("signature", po::value<std::string>(&test_signature)->default_value(""),"output filename for the test execution signature");
        	// clang-format on
	};

	void printValues(std::ostream& os) const override {
		os << std::hex;
		os << "mem_start_addr:\t" << +mem_start_addr << std::endl;
		os << "mem_end_addr:\t"   << +mem_end_addr   << std::endl;
		static_cast <const Options&>( *this ).printValues(os);
	}

	void parse(int argc, char **argv) override {
		Options::parse(argc, argv);

		entry_point.finalize(parse_ulong_option);
		mem_end_addr = mem_start_addr + mem_size - 1;
		assert((mem_end_addr < clint_start_addr || mem_start_addr > display_end_addr) &&
		       "RAM too big, would overlap memory");
		mram_end_addr = mram_start_addr + mram_size - 1;
		assert(mram_end_addr < dma_start_addr && "MRAM too big, would overlap memory");
	}
};


int sc_main(int argc, char **argv) {
	BasicOptions opt;
	opt.parse(argc, argv);

	std::srand(std::time(nullptr));  // use current time as seed for random generator

	tlm::tlm_global_quantum::instance().set(sc_core::sc_time(opt.tlm_global_quantum, sc_core::SC_NS));

	ISS core0(0, opt.use_E_base_isa);
	ISS core1(1, opt.use_E_base_isa);
	ISS core2(2, opt.use_E_base_isa);
	ISS core3(3, opt.use_E_base_isa);
	SimpleMemory mem("SimpleMemory", opt.mem_size);
	SimpleTerminal term("SimpleTerminal");
	UART uart("Generic_UART", 6);
	ELFLoader loader(opt.input_program.c_str());
	//Add one target socket to bus (13-->14)
	SimpleBus<6, 17> bus("SimpleBus"); //FIXME
	CombinedMemoryInterface iss_mem_if0("MemoryInterface0", core0);
	CombinedMemoryInterface iss_mem_if1("MemoryInterface1", core1);
	CombinedMemoryInterface iss_mem_if2("MemoryInterface2", core2);
	CombinedMemoryInterface iss_mem_if3("MemoryInterface3", core3);
	SyscallHandler sys("SyscallHandler");
	FE310_PLIC<4, 64, 96, 32> plic("PLIC");
	CLINT<4> clint("CLINT");
	SimpleSensor sensor("SimpleSensor", 2);
	SimpleSensor2 sensor2("SimpleSensor2", 5);
	BasicTimer timer("BasicTimer", 3);
	MemoryMappedFile mram("MRAM", opt.mram_image, opt.mram_size);
	SimpleDMA dma("SimpleDMA", 4);
	Flashcontroller flashController("Flashcontroller", opt.flash_device);
	EthernetDevice ethernet("EthernetDevice", 7, mem.data, opt.network_device);
	Display display("Display");
	DebugMemoryInterface dbg_if("DebugMemoryInterface");
	//Add
	Lenet lenet("lenet", 6);
	Lenet lenet2("lenet2", 8);
	Lenet lenet3("lenet3", 9);
	Lenet lenet4("lenet4", 10);

	MemoryDMI dmi = MemoryDMI::create_start_size_mapping(mem.data, opt.mem_start_addr, mem.size);
	InstrMemoryProxy instr_mem0(dmi, core0);
	InstrMemoryProxy instr_mem1(dmi, core1);
	InstrMemoryProxy instr_mem2(dmi, core2);
	InstrMemoryProxy instr_mem3(dmi, core3);

	std::shared_ptr<BusLock> bus_lock = std::make_shared<BusLock>();
	iss_mem_if0.bus_lock = bus_lock;
	iss_mem_if1.bus_lock = bus_lock;
	iss_mem_if2.bus_lock = bus_lock;
	iss_mem_if3.bus_lock = bus_lock;

	instr_memory_if *instr_mem_if0 = &iss_mem_if0;
	instr_memory_if *instr_mem_if1 = &iss_mem_if1;
	instr_memory_if *instr_mem_if2 = &iss_mem_if2;
	instr_memory_if *instr_mem_if3 = &iss_mem_if3;
	data_memory_if *data_mem_if0 = &iss_mem_if0;
	data_memory_if *data_mem_if1 = &iss_mem_if1;
	data_memory_if *data_mem_if2 = &iss_mem_if2;
	data_memory_if *data_mem_if3 = &iss_mem_if3;
	if (opt.use_instr_dmi) {
		instr_mem_if0 = &instr_mem0;
		instr_mem_if1 = &instr_mem1;
		instr_mem_if2 = &instr_mem2;
		instr_mem_if3 = &instr_mem3;
	}
	if (opt.use_data_dmi) {
		iss_mem_if0.dmi_ranges.emplace_back(dmi);
		iss_mem_if1.dmi_ranges.emplace_back(dmi);
		iss_mem_if2.dmi_ranges.emplace_back(dmi);
		iss_mem_if3.dmi_ranges.emplace_back(dmi);
	}

	uint64_t entry_point = loader.get_entrypoint();
	if (opt.entry_point.available)
		entry_point = opt.entry_point.value;
	try {
		loader.load_executable_image(mem, mem.size, opt.mem_start_addr);
	} catch(ELFLoader::load_executable_exception& e) {
		std::cerr << e.what() << std::endl;
		std::cerr << "Memory map: " << std::endl;
		opt.printValues(std::cerr);
		return -1;
	}
	/*
	 * The rv32 calling convention defaults to 32 bit, but as this Config is
	 * mainly used together with the syscall handler, this helps for certain floats.
	 * https://github.com/riscv-non-isa/riscv-elf-psabi-doc/blob/master/riscv-elf.adoc
	 */
	core0.init(instr_mem_if0, data_mem_if0, &clint, entry_point, (opt.mem_end_addr-3));
	core1.init(instr_mem_if1, data_mem_if1, &clint, entry_point, (opt.mem_end_addr-32767));
	core2.init(instr_mem_if2, data_mem_if2, &clint, entry_point, (opt.mem_end_addr-65535));
	core3.init(instr_mem_if3, data_mem_if3, &clint, entry_point, (opt.mem_end_addr-98303));
	sys.init(mem.data, opt.mem_start_addr, loader.get_heap_addr());
	sys.register_core(&core0);
	sys.register_core(&core1);
	sys.register_core(&core2);
	sys.register_core(&core3);

	if (opt.intercept_syscalls) {
		core0.sys = &sys;
		core1.sys = &sys;
		core2.sys = &sys;
		core3.sys = &sys;
	}
	core0.error_on_zero_traphandler = opt.error_on_zero_traphandler;
	core1.error_on_zero_traphandler = opt.error_on_zero_traphandler;
	core2.error_on_zero_traphandler = opt.error_on_zero_traphandler;
	core3.error_on_zero_traphandler = opt.error_on_zero_traphandler;


	// address mapping
	{
		unsigned it = 0;
		bus.ports[it++] = new PortMapping(opt.mem_start_addr, opt.mem_end_addr);
		bus.ports[it++] = new PortMapping(opt.clint_start_addr, opt.clint_end_addr);
		bus.ports[it++] = new PortMapping(opt.plic_start_addr, opt.plic_end_addr);
		bus.ports[it++] = new PortMapping(opt.term_start_addr, opt.term_end_addr);
		bus.ports[it++] = new PortMapping(opt.uart_start_addr, opt.uart_end_addr);
		bus.ports[it++] = new PortMapping(opt.sensor_start_addr, opt.sensor_end_addr);
		bus.ports[it++] = new PortMapping(opt.dma_start_addr, opt.dma_end_addr);
		bus.ports[it++] = new PortMapping(opt.sensor2_start_addr, opt.sensor2_end_addr);
		bus.ports[it++] = new PortMapping(opt.mram_start_addr, opt.mram_end_addr);
		bus.ports[it++] = new PortMapping(opt.flash_start_addr, opt.flash_end_addr);
		bus.ports[it++] = new PortMapping(opt.ethernet_start_addr, opt.ethernet_end_addr);
		bus.ports[it++] = new PortMapping(opt.display_start_addr, opt.display_end_addr);
		bus.ports[it++] = new PortMapping(opt.sys_start_addr, opt.sys_end_addr);
		//Add
		bus.ports[it++] = new PortMapping(opt.lenetFilter_start_addr, opt.lenetFilter_end_addr);
		bus.ports[it++] = new PortMapping(opt.lenetFilter2_start_addr, opt.lenetFilter2_end_addr);
		bus.ports[it++] = new PortMapping(opt.lenetFilter3_start_addr, opt.lenetFilter3_end_addr);
		bus.ports[it++] = new PortMapping(opt.lenetFilter4_start_addr, opt.lenetFilter4_end_addr);
	}

	// connect TLM sockets
	iss_mem_if0.isock.bind(bus.tsocks[0]);
	iss_mem_if1.isock.bind(bus.tsocks[1]);
	iss_mem_if2.isock.bind(bus.tsocks[2]);
	iss_mem_if3.isock.bind(bus.tsocks[3]);
	dbg_if.isock.bind(bus.tsocks[4]);

	PeripheralWriteConnector dma_connector("SimpleDMA-Connector");  // to respect ISS bus locking
	dma_connector.isock.bind(bus.tsocks[5]);
	dma.isock.bind(dma_connector.tsock);
	dma_connector.bus_lock = bus_lock;

	{
		unsigned it = 0;
		bus.isocks[it++].bind(mem.tsock);
		bus.isocks[it++].bind(clint.tsock);
		bus.isocks[it++].bind(plic.tsock);
		bus.isocks[it++].bind(term.tsock);
		bus.isocks[it++].bind(uart.tsock);
		bus.isocks[it++].bind(sensor.tsock);
		bus.isocks[it++].bind(dma.tsock);
		bus.isocks[it++].bind(sensor2.tsock);
		bus.isocks[it++].bind(mram.tsock);
		bus.isocks[it++].bind(flashController.tsock);
		bus.isocks[it++].bind(ethernet.tsock);
		bus.isocks[it++].bind(display.tsock);
		bus.isocks[it++].bind(sys.tsock);
		//Add
		bus.isocks[it++].bind(lenet.tsock);
		bus.isocks[it++].bind(lenet2.tsock);
		bus.isocks[it++].bind(lenet3.tsock);
		bus.isocks[it++].bind(lenet4.tsock);
	}

	// connect interrupt signals/communication
	plic.target_harts[0] = &core0;
	plic.target_harts[1] = &core1;
	plic.target_harts[2] = &core2;
	plic.target_harts[3] = &core3;
	clint.target_harts[0] = &core0;
	clint.target_harts[1] = &core1;
	clint.target_harts[2] = &core2;
	clint.target_harts[3] = &core3;
	sensor.plic = &plic;
	dma.plic = &plic;
	timer.plic = &plic;
	sensor2.plic = &plic;
	ethernet.plic = &plic;
	lenet.plic = &plic;
	lenet2.plic = &plic;
	lenet3.plic = &plic;
	lenet4.plic = &plic;

	std::vector<debug_target_if *> threads;
	threads.push_back(&core0);
	threads.push_back(&core1);
	threads.push_back(&core2);
	threads.push_back(&core3);

	core0.trace = opt.trace_mode;  // switch for printing instructions
	core1.trace = opt.trace_mode;  // switch for printing instructions
	core2.trace = opt.trace_mode;  // switch for printing instructions
	core3.trace = opt.trace_mode;  // switch for printing instructions
	if (opt.use_debug_runner) {
		auto server0 = new GDBServer("GDBServer0", threads, &dbg_if, opt.debug_port);
		auto server1 = new GDBServer("GDBServer1", threads, &dbg_if, opt.debug_port);
		auto server2 = new GDBServer("GDBServer2", threads, &dbg_if, opt.debug_port);
		auto server3 = new GDBServer("GDBServer3", threads, &dbg_if, opt.debug_port);
		new GDBServerRunner("GDBRunner0", server0, &core0);
		new GDBServerRunner("GDBRunner1", server1, &core1);
		new GDBServerRunner("GDBRunner2", server2, &core2);
		new GDBServerRunner("GDBRunner3", server3, &core3);
	} else {
		new DirectCoreRunner(core0);
		new DirectCoreRunner(core1);
		new DirectCoreRunner(core2);
		new DirectCoreRunner(core3);
	}

	if (opt.quiet)
		sc_core::sc_report_handler::set_verbosity_level(sc_core::SC_NONE);
	sc_core::sc_start();
	if (!opt.quiet) {
		core0.show();
		core1.show();
		core2.show();
		core3.show();
	}

	if (opt.test_signature != "") {
		auto begin_sig = loader.get_begin_signature_address();
		auto end_sig = loader.get_end_signature_address();

		{
			boost::io::ios_flags_saver ifs(cout);
			std::cout << std::hex;
			std::cout << "begin_signature: " << begin_sig << std::endl;
			std::cout << "end_signature: " << end_sig << std::endl;
			std::cout << "signature output file: " << opt.test_signature << std::endl;
		}

		assert(end_sig >= begin_sig);
		assert(begin_sig >= opt.mem_start_addr);

		auto begin = begin_sig - opt.mem_start_addr;
		auto end = end_sig - opt.mem_start_addr;

		ofstream sigfile(opt.test_signature, ios::out);

		auto n = begin;
		while (n < end) {
			sigfile << std::hex << std::setw(2) << std::setfill('0') << (unsigned)mem.data[n];
			++n;
		}
	}

	return 0;
}
