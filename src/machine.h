// Copyright Cartesi and individual authors (see AUTHORS)
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// This program is free software: you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any
// later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
// PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License along
// with this program (see COPYING). If not, see <https://www.gnu.org/licenses/>.
//

#ifndef MACHINE_H
#define MACHINE_H

/// \file
/// \brief Cartesi machine interface

#include <boost/container/static_vector.hpp>
#include <memory>

#include "access-log.h"
#include "interpret.h"
#include "machine-config.h"
#include "machine-memory-range-descr.h"
#include "machine-merkle-tree.h"
#include "machine-runtime-config.h"
#include "machine-state.h"
#include "os.h"
#include "uarch-interpret.h"
#include "uarch-machine.h"
#include "virtio-device.h"

namespace cartesi {

/// \brief Tag type used to indicate that merkle tree updates should be skipped.
struct skip_merkle_tree_update_t {
    explicit skip_merkle_tree_update_t() = default;
};

/// \brief Tag indicating that merkle tree updates should be skipped.
constexpr skip_merkle_tree_update_t skip_merkle_tree_update;

/// \class machine
/// \brief Cartesi Machine implementation
class machine final {
private:
    //??D Ideally, we would hold a unique_ptr to the state. This
    //    would allow us to remove the machine-state.h include and
    //    therefore hide its contents from anyone who includes only
    //    machine.h. Maybe the compiler can do a good job we we are
    //    not constantly going through the extra indirection. We
    //    should test this.

    mutable machine_state m_s;          ///< Opaque machine state
    mutable machine_merkle_tree m_t;    ///< Merkle tree of state
    std::vector<pma_entry *> m_pmas;    ///< List of all pmas used to compute the machine hash: big machine and uarch
    machine_config m_c;                 ///< Copy of initialization config
    uarch_machine m_uarch;              ///< Microarchitecture machine
    machine_runtime_config m_r;         ///< Copy of initialization runtime config
    machine_memory_range_descrs m_mrds; ///< List of memory ranges returned by get_memory_ranges().

    boost::container::static_vector<std::unique_ptr<virtio_device>, VIRTIO_MAX> m_vdevs; ///< Array of VirtIO devices

    static const pma_entry::flags m_dtb_flags;                   ///< PMA flags used for DTB
    static const pma_entry::flags m_ram_flags;                   ///< PMA flags used for RAM
    static const pma_entry::flags m_flash_drive_flags;           ///< PMA flags used for flash drives
    static const pma_entry::flags m_rollup_rx_buffer_flags;      ///< PMA flags used for rollup rx buffer
    static const pma_entry::flags m_rollup_tx_buffer_flags;      ///< PMA flags used for rollup tx buffer
    static const pma_entry::flags m_rollup_input_metadata_flags; ///< PMA flags used for rollup input metadata
    static const pma_entry::flags m_rollup_voucher_hashes_flags; ///< PMA flags used for rollup voucher hashes
    static const pma_entry::flags m_rollup_notice_hashes_flags;  ///< PMA flags used for rollup notice hashes

    /// \brief Allocates a new PMA entry.
    /// \param pma PMA entry to add to machine.
    /// \returns Reference to corresponding entry in machine state.
    pma_entry &register_pma_entry(pma_entry &&pma);

    /// \brief Creates a new PMA entry reflecting a memory range configuration.
    /// \param description Informative description of PMA entry for use in error messages
    /// \param c Memory range configuration.
    /// \returns New PMA entry (with default flags).
    static pma_entry make_memory_range_pma_entry(const std::string &description, const memory_range_config &c);

    /// \brief Creates a new flash drive PMA entry.
    /// \param description Informative description of PMA entry for use in error messages
    /// \param c Memory range configuration.
    /// \returns New PMA entry with flash drive flags already set.
    static pma_entry make_flash_drive_pma_entry(const std::string &description, const memory_range_config &c);

    /// \brief Creates a new rollup rx buffer PMA entry.
    /// \param c Memory range configuration.
    /// \returns New PMA entry with rx buffer flags already set.
    static pma_entry make_rollup_rx_buffer_pma_entry(const memory_range_config &c);

    /// \brief Creates a new rollup tx buffer PMA entry.
    /// \param c Memory range configuration.
    /// \returns New PMA entry with tx buffer flags already set.
    static pma_entry make_rollup_tx_buffer_pma_entry(const memory_range_config &c);

    /// \brief Creates a new rollup input metadata PMA entry.
    /// \param c Memory range configuration.
    /// \returns New PMA entry with rollup input metadata flags already set.
    static pma_entry make_rollup_input_metadata_pma_entry(const memory_range_config &c);

    /// \brief Creates a new rollup voucher hashes PMA entry.
    /// \param c Memory range configuration.
    /// \returns New PMA entry with rollup voucher hashes flags already set.
    static pma_entry make_rollup_voucher_hashes_pma_entry(const memory_range_config &c);

    /// \brief Creates a new rollup notice hahes PMA entry.
    /// \param c Memory range configuration.
    /// \returns New PMA entry with rollup notice hashes flags already set.
    static pma_entry make_rollup_notice_hashes_pma_entry(const memory_range_config &c);

    /// \brief Saves PMAs into files for serialization
    /// \param config Machine config to be stored
    /// \param directory Directory where PMAs will be stored
    void store_pmas(const machine_config &config, const std::string &directory) const;

    /// \brief Obtain PMA entry that covers a given physical memory region
    /// \param pmas Container of pmas to be searched.
    /// \param s Pointer to machine state.
    /// \param paddr Start of physical memory region.
    /// \param length Length of physical memory region.
    /// \returns Corresponding entry if found, or a sentinel entry
    /// for an empty range.
    template <typename CONTAINER>
    pma_entry &find_pma_entry(const CONTAINER &pmas, uint64_t paddr, size_t length);

    template <typename CONTAINER>
    const pma_entry &find_pma_entry(const CONTAINER &pmas, uint64_t paddr, size_t length) const;

public:
    /// \brief Type of hash
    using hash_type = machine_merkle_tree::hash_type;

    /// \brief List of CSRs to use with read_csr and write_csr
    enum class csr {
        pc,
        fcsr,
        mvendorid,
        marchid,
        mimpid,
        mcycle,
        icycleinstret,
        mstatus,
        mtvec,
        mscratch,
        mepc,
        mcause,
        mtval,
        misa,
        mie,
        mip,
        medeleg,
        mideleg,
        mcounteren,
        menvcfg,
        stvec,
        sscratch,
        sepc,
        scause,
        stval,
        satp,
        scounteren,
        senvcfg,
        ilrsc,
        iflags,
        iunrep,
        clint_mtimecmp,
        plic_girqpend,
        plic_girqsrvd,
        htif_tohost,
        htif_fromhost,
        htif_ihalt,
        htif_iconsole,
        htif_iyield,
        uarch_pc,
        uarch_cycle,
        uarch_halt_flag,
        last
    };

    static constexpr auto num_csr = static_cast<int>(csr::last);

    /// \brief Constructor from machine configuration
    /// \param config Machine config to use instantiating machine
    /// \param runtime Runtime config to use with machine
    explicit machine(const machine_config &config, const machine_runtime_config &runtime = {});

    /// \brief Constructor from previously serialized directory
    /// \param directory Directory to load stored machine from
    /// \param runtime Runtime config to use with machine
    explicit machine(const std::string &directory, const machine_runtime_config &runtime = {});

    /// \brief Serialize entire state to directory
    /// \param directory Directory to store machine into
    void store(const std::string &directory) const;

    /// \brief No default constructor
    machine(void) = delete;
    /// \brief No copy constructor
    machine(const machine &other) = delete;
    /// \brief No move constructor
    machine(machine &&other) = delete;
    /// \brief No copy assignment
    machine &operator=(const machine &other) = delete;
    /// \brief No move assignment
    machine &operator=(machine &&other) = delete;

    /// \brief Runs the machine until mcycle reaches mcycle_end, the machine halts or yields.
    /// \param mcycle_end Maximum value of mcycle before function returns.
    /// \returns The reason the machine was interrupted.
    /// \details Several conditions can cause the function to break before mcycle reaches mcycle_end. The most
    ///  frequent scenario is when the program executes a WFI instruction. Another example is when the machine halts.
    interpreter_break_reason run(uint64_t mcycle_end);

    /// \brief Runs the machine in the microarchitecture until the mcycles advances by one unit or the micro cycle
    /// counter (uarch_cycle) reaches uarch_cycle_end
    /// \param uarch_cycle_end uarch_cycle limit
    uarch_interpreter_break_reason run_uarch(uint64_t uarch_cycle_end);

    /// \brief Advances one micro step and returns a state access log.
    /// \param log_type Type of access log to generate.
    /// \param one_based Use 1-based indices when reporting errors.
    /// \returns The state access log.
    access_log log_uarch_step(const access_log::type &log_type, bool one_based = false);

    /// \brief Resets the microarchitecture state
    void reset_uarch();

    /// \brief Resets the microarchitecture state and returns an access log
    /// \param log_type Type of access log to generate.
    /// \param one_based Use 1-based indices when reporting errors.
    /// \param log_data If true, access data is recorded in the log, otherwise only hashes. The default is false.
    /// \returns The state access log.
    access_log log_uarch_reset(const access_log::type &log_type, bool one_based = false);

    /// \brief Checks the internal consistency of an access log.
    /// \param log State access log to be verified.
    /// \param runtime Machine runtime configuration to use during verification.
    /// \param one_based Use 1-based indices when reporting errors.
    static void verify_uarch_step_log(const access_log &log, const machine_runtime_config &runtime = {},
        bool one_based = false);

    /// \brief Checks the validity of a state transition.
    /// \param root_hash_before State hash before step.
    /// \param log Step state access log.
    /// \param root_hash_after State hash after step.
    /// \param runtime Machine runtime configuration to use during verification.
    /// \param one_based Use 1-based indices when reporting errors.
    static void verify_uarch_step_state_transition(const hash_type &root_hash_before, const access_log &log,
        const hash_type &root_hash_after, const machine_runtime_config &runtime = {}, bool one_based = false);

    /// \brief Checks the internal consistency of an access log produced by log_uarch_reset
    /// \param log State access log to be verified.
    /// \param runtime Machine runtime configuration to use during verification.
    /// \param one_based Use 1-based indices when reporting errors.
    static void verify_uarch_reset_log(const access_log &log, const machine_runtime_config &runtime = {},
        bool one_based = false);

    /// \brief Checks the validity of a state transition. caused by log_uarch_reset
    /// \param root_hash_before State hash before uarch reset
    /// \param log Step state access log.
    /// \param root_hash_after State hash after uarch reset.
    /// \param runtime Machine runtime configuration to use during verification.
    /// \param one_based Use 1-based indices when reporting errors.
    static void verify_uarch_reset_state_transition(const hash_type &root_hash_before, const access_log &log,
        const hash_type &root_hash_after, const machine_runtime_config &runtime = {}, bool one_based = false);

    static machine_config get_default_config(void);

    /// \brief Returns machine state for direct access.
    machine_state &get_state(void) {
        return m_s;
    }

    /// \brief Returns machine state for direct read-only access.
    const machine_state &get_state(void) const {
        return m_s;
    }

    /// \brief Returns a list of descriptions for all PMA entries registered in the machine, sorted by start
    machine_memory_range_descrs get_memory_ranges(void) const {
        return m_mrds;
    }

    /// \brief Destructor.
    ~machine();

    /// \brief Fill file descriptors to be polled by select() for all VirtIO devices.
    /// \param fds Pointer to sets of read, write and except file descriptors to be updated.
    /// \param timeout_us Maximum amount of time to wait in microseconds, this may be updated (always to lower values).
    void prepare_virtio_devices_select(select_fd_sets *fds, uint64_t *timeout_us);

    /// \brief Poll file descriptors that were marked as ready by select() for all VirtIO devices.
    /// \param select_ret Return value from the most recent select() call.
    /// \param fds Pointer to sets of read, write and except file descriptors to be checked.
    /// \returns True if an interrupt was requested, false otherwise.
    /// \details This function process pending events and trigger interrupt requests (if any).
    bool poll_selected_virtio_devices(int select_ret, select_fd_sets *fds, i_device_state_access *da);

    /// \brief Poll file descriptors through select() for all VirtIO devices.
    /// \details Basically call prepare_virtio_devices_select(), select() and poll_selected_virtio_devices().
    /// \param timeout_us Maximum amount of time to wait in microseconds, this may be updated (always to lower values).
    /// \returns True if an interrupt was requested, false otherwise.
    bool poll_virtio_devices(uint64_t *timeout_us, i_device_state_access *da);

    /// \brief Checks if the machine has VirtIO devices.
    /// \returns True if at least one VirtIO device is present.
    bool has_virtio_devices() const;

    /// \brief Checks if the machine has VirtIO console device.
    /// \returns True if at least one VirtIO console is present.
    bool has_virtio_console() const;

    /// \brief Checks if the machine has HTIF console device.
    /// \returns True if HTIF console is present.
    bool has_htif_console() const;

    /// \brief Update the Merkle tree so it matches the contents of the machine state.
    /// \returns true if succeeded, false otherwise.
    bool update_merkle_tree(void) const;

    /// \brief Update the Merkle tree after a page has been modified in the machine state.
    /// \param address Any address inside modified page.
    /// \returns true if succeeded, false otherwise.
    bool update_merkle_tree_page(uint64_t address);

    /// \brief Obtains the proof for a node in the Merkle tree.
    /// \param address Address of target node. Must be aligned to a 2<sup>log2_size</sup> boundary.
    /// \param log2_size log<sub>2</sub> of size subintended by target node.
    /// Must be between 3 (for a word) and 64 (for the entire address space), inclusive.
    /// \param proof Receives the proof.
    /// \details If the node is
    /// smaller than a page size, then it must lie entirely inside the same PMA range.
    machine_merkle_tree::proof_type get_proof(uint64_t address, int log2_size) const;

    /// \brief Obtains the proof for a node in the Merkle tree without making any modifications to the tree.
    /// \param address Address of target node. Must be aligned to a 2<sup>log2_size</sup> boundary.
    /// \param log2_size log<sub>2</sub> of size subintended by target node.
    /// Must be between 3 (for a word) and 64 (for the entire address space), inclusive.
    /// \param proof Receives the proof.
    /// \details If the node is smaller than a page size, then it must lie entirely inside the same PMA range.
    /// This overload is used to optimize proof generation when the caller knows that the tree is already up to date.
    machine_merkle_tree::proof_type get_proof(uint64_t address, int log2_size, skip_merkle_tree_update_t) const;

    /// \brief Obtains the root hash of the Merkle tree.
    /// \param hash Receives the hash.
    void get_root_hash(hash_type &hash) const;

    /// \brief Verifies integrity of Merkle tree.
    /// \returns True if tree is self-consistent, false otherwise.
    bool verify_merkle_tree(void) const;

    /// \brief Read the value of any CSR
    /// \param csr CSR to read
    /// \returns The value of the CSR
    uint64_t read_csr(csr csr) const;

    /// \brief Write the value of any CSR
    /// \param csr CSR to write
    /// \param value Value to write
    void write_csr(csr csr, uint64_t value);

    /// \brief Gets the address of any CSR
    /// \param csr The CSR to obtain address
    /// \returns The address of CSR
    static uint64_t get_csr_address(csr csr);

    /// \brief Read the value of a word in the machine state.
    /// \param address Word address (aligned to 64-bit boundary).
    /// \returns The value of word at address.
    /// \warning The current implementation of this function is very slow!
    uint64_t read_word(uint64_t address) const;

    /// \brief Reads a chunk of data from the machine memory.
    /// \param address Physical address to start reading.
    /// \param data Receives chunk of memory.
    /// \param length Size of chunk.
    /// \details The entire chunk, from \p address to \p address + \p length must
    /// be inside the same PMA region.
    void read_memory(uint64_t address, unsigned char *data, uint64_t length) const;

    /// \brief Writes a chunk of data to the machine memory.
    /// \param address Physical address to start writing.
    /// \param data Source for chunk of data.
    /// \param length Size of chunk.
    /// \details The entire chunk, from \p address to \p address + \p length must
    /// be inside the same PMA region. Moreover, this PMA must be a memory PMA,
    /// and not a device PMA.
    void write_memory(uint64_t address, const unsigned char *data, size_t length);

    /// \brief Reads a chunk of data from the machine virtual memory.
    /// \param vaddr_start Virtual address to start reading.
    /// \param data Receives chunk of memory.
    /// \param length Size of chunk.
    void read_virtual_memory(uint64_t vaddr_start, unsigned char *data, uint64_t length);

    /// \brief Writes a chunk of data to the machine virtual memory.
    /// \param vaddr_start Virtual address to start writing.
    /// \param data Source for chunk of data.
    /// \param length Size of chunk.
    void write_virtual_memory(uint64_t vaddr_start, const unsigned char *data, size_t length);

    /// \brief Reads the value of a general-purpose register.
    /// \param index Register index. Between 0 and X_REG_COUNT-1, inclusive.
    /// \returns The value of the register.
    uint64_t read_x(int index) const;

    /// \brief Writes the value of a general-purpose register.
    /// \param index Register index. Between 1 and X_REG_COUNT-1, inclusive.
    /// \param value New register value.
    void write_x(int index, uint64_t value);

    /// \brief Gets the address of a general-purpose register.
    /// \param index Register index. Between 0 and X_REG_COUNT-1, inclusive.
    /// \returns Address of the specified register
    static uint64_t get_x_address(int index);

    /// \brief Gets the address of a general-purpose microarchitecture register.
    /// \param index Register index. Between 0 and UARCH_X_REG_COUNT-1, inclusive.
    /// \returns Address of the specified register
    static uint64_t get_uarch_x_address(int index);

    /// \brief Reads the value of a floating-point register.
    /// \param index Register index. Between 0 and F_REG_COUNT-1, inclusive.
    /// \returns The value of the register.
    uint64_t read_f(int index) const;

    /// \brief Writes the value of a floating-point register.
    /// \param index Register index. Between 1 and F_REG_COUNT-1, inclusive.
    /// \param value New register value.
    void write_f(int index, uint64_t value);

    /// \brief Gets the address of a floating-point register.
    /// \param index Register index. Between 0 and F_REG_COUNT-1, inclusive.
    /// \returns Address of the specified register
    static uint64_t get_f_address(int index);

    /// \brief Reads the value of the pc register.
    /// \returns The value of the register.
    uint64_t read_pc(void) const;

    /// \brief Reads the value of the pc register.
    /// \param value New register value.
    void write_pc(uint64_t value);

    /// \brief Reads the value of the fcsr register.
    /// \returns The value of the register.
    uint64_t read_fcsr(void) const;

    /// \brief Writes the value of the fcsr register.
    /// \param value New register value.
    void write_fcsr(uint64_t value);

    /// \brief Reads the value of the mvendorid register.
    /// \returns The value of the register.
    uint64_t read_mvendorid(void) const;

    /// \brief Reads the value of the mvendorid register.
    /// \param value New register value.
    void write_mvendorid(uint64_t value);

    /// \brief Reads the value of the marchid register.
    /// \returns The value of the register.
    uint64_t read_marchid(void) const;

    /// \brief Reads the value of the marchid register.
    /// \param value New register value.
    void write_marchid(uint64_t value);

    /// \brief Reads the value of the mimpid register.
    /// \returns The value of the register.
    uint64_t read_mimpid(void) const;

    /// \brief Reads the value of the mimpid register.
    /// \param value New register value.
    void write_mimpid(uint64_t value);

    /// \brief Reads the value of the mcycle register.
    /// \returns The value of the register.
    uint64_t read_mcycle(void) const;

    /// \brief Writes the value of the mcycle register.
    /// \param value New register value.
    void write_mcycle(uint64_t value);

    /// \brief Reads the value of the icycleinstret register.
    /// \returns The value of the register.
    uint64_t read_icycleinstret(void) const;

    /// \brief Writes the value of the icycleinstret register.
    /// \param value New register value.
    void write_icycleinstret(uint64_t value);

    /// \brief Reads the value of the mstatus register.
    /// \returns The value of the register.
    uint64_t read_mstatus(void) const;

    /// \brief Writes the value of the mstatus register.
    /// \param value New register value.
    void write_mstatus(uint64_t value);

    /// \brief Reads the value of the menvcfg register.
    /// \returns The value of the register.
    uint64_t read_menvcfg(void) const;

    /// \brief Writes the value of the menvcfg register.
    /// \param value New register value.
    void write_menvcfg(uint64_t value);

    /// \brief Reads the value of the mtvec register.
    /// \returns The value of the register.
    uint64_t read_mtvec(void) const;

    /// \brief Writes the value of the mtvec register.
    /// \param value New register value.
    void write_mtvec(uint64_t value);

    /// \brief Reads the value of the mscratch register.
    /// \returns The value of the register.
    uint64_t read_mscratch(void) const;

    /// \brief Writes the value of the mscratch register.
    /// \param value New register value.
    void write_mscratch(uint64_t value);

    /// \brief Reads the value of the mepc register.
    /// \returns The value of the register.
    uint64_t read_mepc(void) const;

    /// \brief Writes the value of the mepc register.
    /// \param value New register value.
    void write_mepc(uint64_t value);

    /// \brief Reads the value of the mcause register.
    /// \returns The value of the register.
    uint64_t read_mcause(void) const;

    /// \brief Writes the value of the mcause register.
    /// \param value New register value.
    void write_mcause(uint64_t value);

    /// \brief Reads the value of the mtval register.
    /// \returns The value of the register.
    uint64_t read_mtval(void) const;

    /// \brief Writes the value of the mtval register.
    /// \param value New register value.
    void write_mtval(uint64_t value);

    /// \brief Reads the value of the misa register.
    /// \returns The value of the register.
    uint64_t read_misa(void) const;

    /// \brief Writes the value of the misa register.
    /// \param value New register value.
    void write_misa(uint64_t value);

    /// \brief Reads the value of the mie register.
    /// \returns The value of the register.
    uint64_t read_mie(void) const;

    /// \brief Reads the value of the mie register.
    /// \param value New register value.
    void write_mie(uint64_t value);

    /// \brief Reads the value of the mip register.
    /// \returns The value of the register.
    uint64_t read_mip(void) const;

    /// \brief Reads the value of the mip register.
    /// \param value New register value.
    void write_mip(uint64_t value);

    /// \brief Reads the value of the medeleg register.
    /// \returns The value of the register.
    uint64_t read_medeleg(void) const;

    /// \brief Writes the value of the medeleg register.
    /// \param value New register value.
    void write_medeleg(uint64_t value);

    /// \brief Reads the value of the mideleg register.
    /// \returns The value of the register.
    uint64_t read_mideleg(void) const;

    /// \brief Writes the value of the mideleg register.
    /// \param value New register value.
    void write_mideleg(uint64_t value);

    /// \brief Reads the value of the mcounteren register.
    /// \returns The value of the register.
    uint64_t read_mcounteren(void) const;

    /// \brief Writes the value of the mcounteren register.
    /// \param value New register value.
    void write_mcounteren(uint64_t value);

    /// \brief Reads the value of the senvcfg register.
    /// \returns The value of the register.
    uint64_t read_senvcfg(void) const;

    /// \brief Writes the value of the senvcfg register.
    /// \param value New register value.
    void write_senvcfg(uint64_t value);

    /// \brief Reads the value of the stvec register.
    /// \returns The value of the register.
    uint64_t read_stvec(void) const;

    /// \brief Writes the value of the stvec register.
    /// \param value New register value.
    void write_stvec(uint64_t value);

    /// \brief Reads the value of the sscratch register.
    /// \returns The value of the register.
    uint64_t read_sscratch(void) const;

    /// \brief Writes the value of the sscratch register.
    /// \param value New register value.
    void write_sscratch(uint64_t value);

    /// \brief Reads the value of the sepc register.
    /// \returns The value of the register.
    uint64_t read_sepc(void) const;

    /// \brief Writes the value of the sepc register.
    /// \param value New register value.
    void write_sepc(uint64_t value);

    /// \brief Reads the value of the scause register.
    /// \returns The value of the register.
    uint64_t read_scause(void) const;

    /// \brief Writes the value of the scause register.
    /// \param value New register value.
    void write_scause(uint64_t value);

    /// \brief Reads the value of the stval register.
    /// \returns The value of the register.
    uint64_t read_stval(void) const;

    /// \brief Writes the value of the stval register.
    /// \param value New register value.
    void write_stval(uint64_t value);

    /// \brief Reads the value of the satp register.
    /// \returns The value of the register.
    uint64_t read_satp(void) const;

    /// \brief Writes the value of the satp register.
    /// \param value New register value.
    void write_satp(uint64_t value);

    /// \brief Reads the value of the scounteren register.
    /// \returns The value of the register.
    uint64_t read_scounteren(void) const;

    /// \brief Writes the value of the scounteren register.
    /// \param value New register value.
    void write_scounteren(uint64_t value);

    /// \brief Reads the value of the ilrsc register.
    /// \returns The value of the register.
    uint64_t read_ilrsc(void) const;

    /// \brief Writes the value of the ilrsc register.
    /// \param value New register value.
    void write_ilrsc(uint64_t value);

    /// \brief Reads the value of the iflags register.
    /// \returns The value of the register.
    uint64_t read_iflags(void) const;

    /// \brief Returns packed iflags from its component fields.
    /// \returns The value of the register.
    uint64_t packed_iflags(int PRV, int Y, int H);

    /// \brief Reads the value of the iflags register.
    /// \param value New register value.
    void write_iflags(uint64_t value);

    /// \brief Reads the value of the iunrep register.
    /// \returns The value of the register.
    uint64_t read_iunrep(void) const;

    /// \brief Writes the value of the iunrep register.
    /// \param value New register value.
    void write_iunrep(uint64_t value);

    /// \brief Reads the value of HTIF's tohost register.
    /// \returns The value of the register.
    uint64_t read_htif_tohost(void) const;

    /// \brief Reads the value of the device field of HTIF's tohost register.
    /// \returns The value of the field.
    uint64_t read_htif_tohost_dev(void) const;

    /// \brief Reads the value of the command field of HTIF's tohost register.
    /// \returns The value of the field.
    uint64_t read_htif_tohost_cmd(void) const;

    /// \brief Reads the value of the data field of HTIF's tohost register.
    /// \returns The value of the field.
    uint64_t read_htif_tohost_data(void) const;

    /// \brief Writes the value of HTIF's tohost register.
    /// \param value New register value.
    void write_htif_tohost(uint64_t value);

    /// \brief Reads the value of HTIF's fromhost register.
    /// \returns The value of the register.
    uint64_t read_htif_fromhost(void) const;

    /// \brief Writes the value of HTIF's fromhost register.
    /// \param value New register value.
    void write_htif_fromhost(uint64_t value);

    /// \brief Writes the value of the data field in HTIF's fromhost register.
    /// \param value New value for the field.
    void write_htif_fromhost_data(uint64_t value);

    /// \brief Reads the value of HTIF's halt register.
    /// \returns The value of the register.
    uint64_t read_htif_ihalt(void) const;

    /// \brief Writes the value of HTIF's halt register.
    /// \param value New register value.
    void write_htif_ihalt(uint64_t value);

    /// \brief Reads the value of HTIF's console register.
    /// \returns The value of the register.
    uint64_t read_htif_iconsole(void) const;

    /// \brief Writes the value of HTIF's console register.
    /// \param value New register value.
    void write_htif_iconsole(uint64_t value);

    /// \brief Reads the value of HTIF's yield register.
    /// \returns The value of the register.
    uint64_t read_htif_iyield(void) const;

    /// \brief Writes the value of HTIF's yield register.
    /// \param value New register value.
    void write_htif_iyield(uint64_t value);

    /// \brief Reads the value of CLINT's mtimecmp register.
    /// \returns The value of the register.
    uint64_t read_clint_mtimecmp(void) const;

    /// \brief Writes the value of CLINT's mtimecmp register.
    /// \param value New register value.
    void write_clint_mtimecmp(uint64_t value);

    /// \brief Reads the value of PLIC's girqpend register.
    /// \returns The value of the register.
    uint64_t read_plic_girqpend(void) const;

    /// \brief Writes the value of PLIC's girqpend register.
    /// \param val New register value.
    void write_plic_girqpend(uint64_t val);

    /// \brief Reads the value of PLIC's girqsrvd register.
    /// \returns The value of the register.
    uint64_t read_plic_girqsrvd(void) const;

    /// \brief Writes the value of PLIC's girqsrvd register.
    /// \param val New register value.
    void write_plic_girqsrvd(uint64_t val);

    /// \brief Checks the value of the iflags_X flag.
    /// \returns The flag value.
    bool read_iflags_X(void) const;

    /// \brief Resets the value of the iflags_X flag.
    void reset_iflags_X(void);

    /// \brief Sets the iflags_X flag.
    void set_iflags_X(void);

    /// \brief Checks the value of the iflags_Y flag.
    /// \returns The flag value.
    bool read_iflags_Y(void) const;

    /// \brief Resets the value of the iflags_Y flag.
    void reset_iflags_Y(void);

    /// \brief Sets the iflags_Y flag.
    void set_iflags_Y(void);

    /// \brief Checks the value of the iflags_H flag.
    /// \returns The flag value.
    bool read_iflags_H(void) const;

    /// \brief Checks the value of the iflags_PRV field.
    /// \returns The field value.
    uint8_t read_iflags_PRV(void) const;

    /// \brief Sets the iflags_H flag.
    void set_iflags_H(void);

    /// \brief Get read-only access to container with all PMA entries.
    /// \returns The container.
    const boost::container::static_vector<pma_entry, PMA_MAX> &get_pmas(void) const;

    /// \brief Obtain PMA entry from the machine state that covers a given physical memory region
    /// \brief Microarchitecture PMAs are not considered.
    /// \param s Pointer to machine state.
    /// \param paddr Start of physical memory region.
    /// \param length Length of physical memory region.
    /// \returns Corresponding entry if found, or a sentinel entry
    /// for an empty range.
    pma_entry &find_pma_entry(uint64_t paddr, size_t length);

    const pma_entry &find_pma_entry(uint64_t paddr, size_t length) const;

    /// \brief Obtain PMA entry covering a physical memory word
    /// \tparam T Type of word.
    /// \param s Pointer to machine state.
    /// \param paddr Target physical address.
    /// \returns Corresponding entry if found, or a sentinel entry
    /// for an empty range.
    template <typename T>
    const pma_entry &find_pma_entry(uint64_t paddr) const {
        return find_pma_entry(paddr, sizeof(T));
    }

    /// \brief Go over the write TLB and mark as dirty all pages currently there.
    void mark_write_tlb_dirty_pages(void) const;

    /// \brief Verify if dirty page maps are consistent.
    /// \returns true if they are, false if there is an error.
    bool verify_dirty_page_maps(void) const;

    /// \brief Copies the current state into a configuration for serialization
    /// \returns The configuration
    machine_config get_serialization_config(void) const;

    /// \brief Returns copy of initialization config.
    const machine_config &get_initial_config(void) const {
        return m_c;
    }

    /// \brief Returns the machine runtime config.
    const machine_runtime_config &get_runtime_config(void) const {
        return m_r;
    }

    /// \brief Replaces a memory range.
    /// \param range Configuration of the new memory range.
    /// \details The machine must contain an existing memory range
    /// matching the start and length specified in range.
    void replace_memory_range(const memory_range_config &range);

    /// \brief Reads the value of a microarchitecture register.
    /// \param index Register index. Between 0 and UARCH_X_REG_COUNT-1, inclusive.
    /// \returns The value of the register.
    uint64_t read_uarch_x(int index) const;

    /// \brief Writes the value of a of a microarchitecture register.
    /// \param index Register index. Between 0 and UARCH_X_REG_COUNT-1, inclusive.
    /// \param value New register value.
    void write_uarch_x(int index, uint64_t value);

    /// \brief Reads the value of the microarchitecture pc register.
    /// \returns The current microarchitecture pc value.
    uint64_t read_uarch_pc(void) const;

    /// \brief Writes the value ofthe microarchitecture pc register.
    /// \param value New register value.
    void write_uarch_pc(uint64_t value);

    /// \brief Reads the value of the microarchitecture halt flag.
    /// \returns The current microarchitecture halt value.
    bool read_uarch_halt_flag(void) const;

    /// \brief Sets the value ofthe microarchitecture halt flag.
    void set_uarch_halt_flag();

    /// \brief Reads the value of the microarchitecture cycle counter register.
    /// \returns The current microarchitecture cycle.
    uint64_t read_uarch_cycle(void) const;

    /// \brief Writes the value ofthe microarchitecture cycle counter register.
    /// \param value New register value.
    void write_uarch_cycle(uint64_t value);
};

} // namespace cartesi

#endif
