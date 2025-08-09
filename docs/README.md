# AI-OS Project Documentation

This document provides an overview of the AI-OS project, detailing the initial setup, the core components, and the current development status.

## Project Overview

The AI-OS project aims to build a basic operating system. This initial phase focuses on setting up the development environment, creating the fundamental boot and kernel components, and enabling basic output functionality.

## Resources Used

The primary resource for this project is the detailed definition of files for "Étape 1: Le Socle - Démarrage et Noyau de Base" (Step 1: The Foundation - Boot and Basic Kernel). This document, originally provided as `pasted_content.txt`, outlines the necessary files and their content.

- `pasted_content.txt`: Original documentation detailing the project structure, file contents, and build instructions.

## Project Structure

The project follows the structure outlined in the `pasted_content.txt`:

```
projet-ai-os/
├── kernel/
│   └── kernel.c
├── boot/
│   └── boot.s
├── linker.ld
├── Makefile
└── .gitignore
```

## File Details and Modifications

### 1. `boot/boot.s`

This assembly file serves as the entry point for the operating system. It sets up the Multiboot header and the stack, then calls the `kmain` function in `kernel.c`.

**No modifications were made to this file.**

### 2. `kernel/kernel.c`

This C file contains the core of our kernel. Initially, it was designed to display a message directly to VGA memory. However, to enable output capture in the sandbox environment, modifications were made to redirect output to the serial port.

**Modifications:**
- Added `serial_init()`, `is_transmit_empty()`, `write_serial()`, `inb()`, and `outb()` functions for serial port communication.
- Modified `kmain()` to initialize the serial port and print the welcome message "Bienvenue dans AI-OS !" to the serial port instead of VGA.
- Renamed `print_char` to `print_char_vga` and `print_string` to `print_string_vga` to distinguish them from serial output functions.

### 3. `linker.ld`

This linker script instructs the `ld` tool on how to combine the compiled object files (`boot.o` and `kernel.o`) into the final OS executable. It defines the entry point and memory organization.

**No modifications were made to this file.**

### 4. `Makefile`

This Makefile automates the compilation, linking, and testing processes. It defines rules for building the OS image and running it with QEMU.

**Modifications:**
- The `run` target was modified to include `-nographic` and `-serial file:output.log` flags for QEMU. This allows the OS to run without a graphical interface and redirects all serial output to `output.log`, making it possible to capture the kernel's output in the sandbox environment.

### 5. `.gitignore`

This file specifies which files and directories Git should ignore, preventing generated files from being committed to the repository.

**No modifications were made to this file.**

## Current Development Status

As of now, the AI-OS project has successfully completed the initial setup and core component creation. The environment is configured, and the bootloader and kernel are compiled and linked. The OS can be run in QEMU, and its output is now redirected to a log file, confirming its basic functionality.

**Next Steps:**

1. **Further Kernel Development:** Implement more advanced features in the kernel, such as keyboard input, interrupt handling, and memory management.
2. **Driver Development:** Add drivers for various hardware components to enable more complex interactions.
3. **User Interaction:** Develop a basic shell or command-line interface for user interaction.

## How to Continue the Project

To continue working on this project, follow these steps:

1. **Clone the Repository:**
   ```bash
   git clone https://github.com/kamgueblondin/ai-os.git
   cd ai-os
   ```

2. **Install Prerequisites (if not already installed):**
   ```bash
   sudo apt-get update
   sudo apt-get install -y build-essential nasm qemu-system-x86 git
   ```

3. **Build the OS:**
   ```bash
   make
   ```

4. **Run the OS in QEMU and view output:**
   ```bash
   make run
   cat output.log
   ```

This will run the AI-OS in QEMU, and the output will be displayed in your terminal from the `output.log` file. You can then proceed to modify `kernel.c` or other files to add new functionalities.

