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

#include "machine.h"
#include "htif.h"
#include "i-virtual-state-access.h"
#include "strict-aliasing.h"

#include <signal.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace cartesi {

#define HTIF_TOHOST_REL_ADDR (static_cast<uint64_t>(htif::csr::tohost))
#define HTIF_FROMHOST_REL_ADDR (static_cast<uint64_t>(htif::csr::fromhost))

void htif::reset_fromhost_pending(void) {
    m_fromhost_pending = false;
}

bool htif::is_interactive(void) const {
    return m_interactive;
}

bool htif::fromhost_pending(void) const {
    return m_fromhost_pending;
}

const machine &htif::get_machine(void) const {
    return m_machine;
}

uint64_t htif::get_csr_rel_addr(csr reg) {
    return static_cast<uint64_t>(reg);
}

static int new_ttyfd(const char *path) {
    int fd;
    do {
        fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    } while (fd == -1 && errno == EINTR);
    return fd;
}

static int get_ttyfd(void) {
    char *path;
    if ((path = ttyname(STDERR_FILENO)) != nullptr)
        return new_ttyfd(path);
    else if ((path = ttyname(STDOUT_FILENO)) != nullptr)
        return new_ttyfd(path);
    else if ((path = ttyname(STDIN_FILENO)) != nullptr)
        return new_ttyfd(path);
    else if ((path = ctermid(nullptr)) != nullptr)
        return new_ttyfd(path);
    else
        errno = ENOTTY; /* No terminal */
    return -1;
}

void htif::init_console(void) {
    if ((m_ttyfd = get_ttyfd()) >= 0) {
        struct termios tty;
        memset(&tty, 0, sizeof(tty));
        tcgetattr(m_ttyfd, &tty);
        m_oldtty = tty;
        // Set terminal to "raw" mode
        tty.c_lflag &= ~(
            ECHO   | // Echo off
            ICANON | // Canonical mode off
            ECHONL | // Do not echo NL (redundant with ECHO and ICANON)
            ISIG   | // Signal chars off
            IEXTEN   // Extended input processing off
        );
        tty.c_iflag &= ~(
            IGNBRK | // Generate \377 \0 \0 on BREAK
            BRKINT | //
            PARMRK | //
            ICRNL  | // No CR-to-NL
            ISTRIP | // Do not strip off 8th bit
            INLCR  | // No NL-to-CR
            IGNCR  | // Do not ignore CR
            IXON     // Disable XON/XOFF flow control on output
        );
        tty.c_oflag |=
            OPOST;   // Enable output processing
        // Enable parity generation on output and checking for input
        tty.c_cflag &= ~(CSIZE|PARENB);
        tty.c_cflag |= CS8;
        // Read returns with 1 char and no delay
        tty.c_cc[VMIN] = 1;
        tty.c_cc[VTIME] = 0;
        tcsetattr(m_ttyfd, TCSANOW, &tty);
        //??D Should we check to see if changes stuck?
    }
}

void htif::poll_console(void) {
    //??D We do not need to register any access to state here because
    //    the console is always disabled during verifiable execution

    // Check for input from console, if requested by HTIF
    // Obviously, somethind different must be done in blockchain
    if (!m_fromhost_pending) {
        // If we don't have any characters left in buffer, try to obtain more
        if (m_buf_pos >= m_buf_len) {
            fd_set rfds;
            int fd_max;
            struct timeval tv;
            FD_ZERO(&rfds);
            FD_SET(STDIN_FILENO, &rfds);
            fd_max = 0;
            tv.tv_sec = 0;
            tv.tv_usec = 0;
            if (select(fd_max+1, &rfds, nullptr, nullptr, &tv) > 0 &&
                FD_ISSET(0, &rfds)) {
                m_buf_pos = 0;
                m_buf_len = read(STDIN_FILENO, m_buf, sizeof(m_buf));
                // If stdin is closed, pass EOF to client
                if (m_buf_len <= 0) {
                    m_buf_len = 1;
                    m_buf[0] = 4; // CTRL+D
                }
            }
        }
        // If we have data to return
        if (m_buf_pos < m_buf_len) {
            m_machine.write_htif_fromhost(
                ((uint64_t)1 << 56) | ((uint64_t)0 << 48) | m_buf[m_buf_pos++]);
            m_fromhost_pending = true;
        }
    }
}

void htif::end_console(void) {
    if (m_ttyfd >= 0) {
        tcsetattr(m_ttyfd, TCSANOW, &m_oldtty);
        close(m_ttyfd);
    }
}

// The constructor for the associated machine is typically done
// yet when the constructor for the HTIF device is invoked.
htif::htif(machine &m, bool i):
    m_machine{m},
    m_interactive{i},
    m_buf{}, m_buf_pos{}, m_buf_len{},
    m_fromhost_pending{false},
    m_divisor_counter{},
    m_ttyfd{-1} {
    memset(&m_oldtty, 0, sizeof(m_oldtty));
    if (m_interactive) {
        init_console();
    }
}

htif::~htif() {
    if (m_interactive) {
        end_console();
    }
}

void htif::interact(void) {
    // Only interact every
    if (m_interactive && ++m_divisor_counter >= HTIF_INTERACT_DIVISOR) {
        m_divisor_counter = 0;
        poll_console();
    }
}

/// \brief HTIF device read callback. See ::pma_read.
static bool htif_read(const pma_entry &pma, i_virtual_state_access *a, uint64_t offset, uint64_t *pval, int size_log2) {
    (void) pma;

    // Our HTIF only supports aligned 64-bit reads
    if (size_log2 != 3 || offset & 7) return false;

    switch (offset) {
        case HTIF_TOHOST_REL_ADDR:
            *pval = a->read_htif_tohost();
            return true;
        case HTIF_FROMHOST_REL_ADDR:
            *pval = a->read_htif_fromhost();
            return true;
        default:
            // other reads are exceptions
            return false;
    }
}

/// \brief HTIF device peek callback. See ::pma_peek.
static bool htif_peek(const pma_entry &pma, uint64_t page_offset,
    const unsigned char **page_data, unsigned char *scratch) {
    const htif *h = reinterpret_cast<htif *>(pma.get_device().get_context());
    const machine &m = h->get_machine();
    // Check for alignment and range
    if (page_offset % PMA_PAGE_SIZE != 0 || page_offset >= pma.get_length()) {
        *page_data = nullptr;
        return false;
    }
    // Page 0 is the only non-pristine page
    if (page_offset != 0) {
        *page_data = nullptr;
        return true;
    }
    // Clear entire page.
    memset(scratch, 0, PMA_PAGE_SIZE);
    // Copy tohost and fromhost to their places within page.
    aliased_aligned_write<uint64_t>(scratch +
        htif::get_csr_rel_addr(htif::csr::tohost), m.read_htif_tohost());
    aliased_aligned_write<uint64_t>(scratch +
        htif::get_csr_rel_addr(htif::csr::fromhost), m.read_htif_fromhost());
    *page_data = scratch;
    return true;
}

static bool htif_write_getchar(i_virtual_state_access *a, htif *h, uint64_t payload) {
    //??D Not sure exactly what role this command plays
    (void) h; (void) payload;
    a->write_htif_tohost(0); // Acknowledge command
    // a->write_fromhost(((uint64_t)1 << 56) | ((uint64_t)1 << 48));
    return true;
}

static bool htif_write_putchar(i_virtual_state_access *a, htif *h, uint64_t payload) {
    (void) h;
    a->write_htif_tohost(0); // Acknowledge command
    uint8_t ch = payload & 0xff;
    // Obviously, somethind different must be done in blockchain
    if (write(STDOUT_FILENO, &ch, 1) < 1) { ; }
    a->write_htif_fromhost(((uint64_t)1 << 56) | ((uint64_t)1 << 48));
    return true;
}

static bool htif_write_halt(i_virtual_state_access *a, htif *h, uint64_t payload) {
    (void) h; (void) payload;
    a->set_iflags_H();
    // Leave tohost value alone so the payload can be read afterwards
    return true;
}

static bool htif_write_tohost(i_virtual_state_access *a, htif *h, uint64_t tohost) {
    // Decode tohost
    uint32_t device = tohost >> 56;
    uint32_t cmd = (tohost >> 48) & 0xff;
    uint64_t payload = (tohost & (~1ULL >> 16));
    // Log write to tohost
    a->write_htif_tohost(tohost);
    // Handle commands
    if (device == 0 && cmd == 0 && (payload & 1)) {
        return htif_write_halt(a, h, payload);
    } else if (device == 1 && cmd == 1) {
        return htif_write_putchar(a, h, payload);
    } else if (device == 1 && cmd == 0) {
        return htif_write_getchar(a, h, payload);
    }
    //??D Unknown HTIF commands are sillently ignored
    return true;
}

static bool htif_write_fromhost(i_virtual_state_access *a, htif *h, uint64_t val) {
    a->write_htif_fromhost(val);
    if (h->is_interactive()) {
        h->reset_fromhost_pending();
        h->poll_console();
    }
    return true;
}

/// \brief HTIF device write callback. See ::pma_write.
static bool htif_write(const pma_entry &pma, i_virtual_state_access *a, uint64_t offset, uint64_t val, int size_log2) {
    htif *h = reinterpret_cast<htif *>(pma.get_device().get_context());

    // Our HTIF only supports aligned 64-bit writes
    if (size_log2 != 3 || offset & 7) return false;

    switch (offset) {
        case HTIF_TOHOST_REL_ADDR:
            return htif_write_tohost(a, h, val);
        case HTIF_FROMHOST_REL_ADDR:
            return htif_write_fromhost(a, h, val);
        default:
            // other writes are exceptions
            return false;
    }
}

static const pma_driver htif_driver {
    "HTIF",
    htif_read,
    htif_write
};

void htif::register_mmio(uint64_t start, uint64_t length) {
    m_machine.register_mmio(start, length, htif_peek, this, &htif_driver, PMA_ISTART_DID::HTIF);
}

} // namespace cartesi