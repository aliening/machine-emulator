// Copyright 2019 Cartesi Pte. Ltd.
//
// This file is part of the machine-emulator. The machine-emulator is free
// software: you can redistribute it and/or modify it under the terms of the GNU
// Lesser General Public License as published by the Free Software Foundation,
// either version 3 of the License, or (at your option) any later version.
//
// The machine-emulator is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
// for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with the machine-emulator. If not, see http://www.gnu.org/licenses/.
//

#ifndef MACHINE_CONFIG_H
#define MACHINE_CONFIG_H

#include <cstdint>
#include <string>
#include <boost/container/static_vector.hpp>

#include "riscv-constants.h"

namespace cartesi {

/// \brief Processor state configuration
struct processor_config final {
    uint64_t x[X_REG_COUNT]{};          ///< Value of general-purpose registers
    uint64_t pc{PC_INIT};                  ///< Value of pc
    uint64_t mvendorid{MVENDORID_INIT};    ///< Value of mvendorid CSR
    uint64_t marchid{MARCHID_INIT};        ///< Value of marchid CSR
    uint64_t mimpid{MIMPID_INIT};          ///< Value of mimpid CSR
    uint64_t mcycle{MCYCLE_INIT};          ///< Value of mcycle CSR
    uint64_t minstret{MINSTRET_INIT};      ///< Value of minstret CSR
    uint64_t mstatus{MSTATUS_INIT};        ///< Value of mstatus CSR
    uint64_t mtvec{MTVEC_INIT};            ///< Value of mtvec CSR
    uint64_t mscratch{MSCRATCH_INIT};      ///< Value of mscratch CSR
    uint64_t mepc{MEPC_INIT};              ///< Value of mepc CSR
    uint64_t mcause{MCAUSE_INIT};          ///< Value of mcause CSR
    uint64_t mtval{MTVAL_INIT};            ///< Value of mtval CSR
    uint64_t misa{MISA_INIT};              ///< Value of misa CSR
    uint64_t mie{MIE_INIT};                ///< Value of mie CSR
    uint64_t mip{MIP_INIT};                ///< Value of mip CSR
    uint64_t medeleg{MEDELEG_INIT};        ///< Value of medeleg CSR
    uint64_t mideleg{MIDELEG_INIT};        ///< Value of mideleg CSR
    uint64_t mcounteren{MCOUNTEREN_INIT};  ///< Value of mcounteren CSR
    uint64_t stvec{STVEC_INIT};            ///< Value of stvec CSR
    uint64_t sscratch{SSCRATCH_INIT};      ///< Value of sscratch CSR
    uint64_t sepc{SEPC_INIT};              ///< Value of sepc CSR
    uint64_t scause{SCAUSE_INIT};          ///< Value of scause CSR
    uint64_t stval{STVAL_INIT};            ///< Value of stval CSR
    uint64_t satp{SATP_INIT};              ///< Value of satp CSR
    uint64_t scounteren{SCOUNTEREN_INIT};  ///< Value of scounteren CSR
    uint64_t ilrsc{ILRSC_INIT};            ///< Value of ilrsc CSR
    uint64_t iflags{IFLAGS_INIT};          ///< Value of iflags CSR
};

/// \brief RAM state configuration
struct ram_config final {
    uint64_t length{0}; ///< RAM length
    std::string image_filename{}; ///< RAM image file name
};

/// \brief ROM state configuration
struct rom_config final {
    std::string bootargs{}; ///< Bootargs to pass to kernel
    std::string image_filename{}; ///< ROM image file
};

/// \brief Flash drive state configuration
struct flash_drive_config final {
    uint64_t start{0};            ///< Flash drive start position
    uint64_t length{0};           ///< Flash drive length
    bool shared{false};           ///< Target changes to drive affect image file?
    std::string image_filename{}; ///< Flash drive image file name
};

/// \brief CLINT device state configuration
struct clint_config final {
    uint64_t mtimecmp{MTIMECMP_INIT}; ///< Value of mtimecmp CSR
};

/// \brief HTIF device state configuration
struct htif_config final {
    uint64_t fromhost{FROMHOST_INIT}; ///< Value of fromhost CSR
    uint64_t tohost{TOHOST_INIT};     ///< Value of tohost CSR
    bool console_getchar{false};      ///< Make console getchar available?
    bool yield_progress{false};       ///< Make yield progress available?
    bool yield_rollup{false};         ///< Make yield rollup available?
};

/// \brief DHD device state configuration
struct dhd_config final {
    uint64_t tstart{0};           ///< Start of target physical memory range for output data
    uint64_t tlength{0};          ///< Length of target physical memory range for output data
    std::string image_filename{}; ///< Data image file name
    uint64_t dlength{0};          ///< Output data length CSR
    uint64_t hlength{0};          ///< Input hash length CSR
    uint64_t h[DHD_H_REG_COUNT]{}; ///< Input hash words
};

/// \brief Flash constants
enum FLASH_DRIVE_constants {
    FLASH_DRIVE_MAX = 8 ///< Maximum number of flash drives
};

/// \brief List of flash drives
using flash_drive_configs = boost::container::static_vector<flash_drive_config, FLASH_DRIVE_MAX>;

/// \brief Machine state configuration
struct machine_config final {

    processor_config processor{};      ///< Processor state
    ram_config ram{};                  ///< RAM state
    rom_config rom{};                  ///< ROM state
    flash_drive_configs flash_drive{}; ///< Flash drives state
    clint_config clint{};              ///< CLINT device state
    htif_config htif{};                ///< HTIF device state
    dhd_config dhd{};                  ///< DHD state

    /// \brief Get the name where config will be stored in a directory
    static std::string get_config_filename(const std::string &dir);

    /// \brief Get the name where memory range will be stored in a directory
    static std::string get_image_filename(const std::string &dir,
        uint64_t start, uint64_t length);

    /// \brief Loads a machine config from a directory
    /// \param dir Directory from whence "config" will be loaded
    /// \returns The config loaded
    static machine_config load(const std::string &dir);

    /// \brief Stores the machine config to a directory
    /// \param dir Directory where "config" will be stored
    void store(const std::string &dir) const;
};

} // namespace cartesi

#endif
