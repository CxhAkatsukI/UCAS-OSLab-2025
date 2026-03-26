#include <aesthetic.h>
#include <os/kernel.h>

#define IBM_LOGO 1
#define WINDOWS_95

/**
 * @brief Print the batch processing system logo when OS starts.
 */
void print_logo(void) {

#if(IBM_LOGO==1)

    bios_putstr("=====================================================\n\r");
    bios_putstr("        GM-NAA I/O BATCH PROCESSING MONITOR\n\r");
    bios_putstr("=====================================================\n\r");
    bios_putstr("\n\r");
    bios_putstr("*****************************************************\n\r");
    bios_putstr("*****************************************************\n\r");
    bios_putstr("*****************************************************\n\r");
    bios_putstr("███████████  ████████████      ████████      ████████\n\r");
    bios_putstr("███████████  ███████████████   █████████    █████████\n\r");
    bios_putstr("   █████        ████   █████     ████████  ████████\n\r");
    bios_putstr("   █████        ███████████      ████  ███ ███ ████\n\r");
    bios_putstr("   █████        ███████████      ████  ███████ ████\n\r");
    bios_putstr("   █████        ████   █████     ████   █████  ████\n\r");
    bios_putstr("███████████  ███████████████   ██████    ███   ██████\n\r");
    bios_putstr("███████████  ████████████      ██████     █    ██████\n\r");
    bios_putstr("*****************************************************\n\r");
    bios_putstr("*  This system is for the use of authorized users   *\n\r");
    bios_putstr("*  only. Usage of  this system may be monitored     *\n\r");
    bios_putstr("*  and recorded                                     *\n\r");
    bios_putstr("*****************************************************\n\r");
    bios_putstr("\n\r"); // Add an extra newline for spacing

#endif

}
