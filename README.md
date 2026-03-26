# OSlab: 操作系统研讨课

This repository is for the course "Operating Systems Seminar" (操作系统研讨课).

**Student:** Chuxiao Han  
**Student Number:** 2023K8009908002

---

## Course Information

*   **Course Name:** 操作系统研讨课 (Operating Systems Seminar)
*   **Course Code:** B0911011H
*   **Term:** Fall 2025-2026
*   **Course Convener:** 陈明宇 (cmy@ict.ac.cn)
*   **Lecturer (Fall):** 蒋德钧 (jiangdejun@ict.ac.cn)
*   **Teaching Assistants:**
    *   卢天越 (lutianyue@ict.ac.cn)
    *   吴墨林 (wumolin22@mails.ucas.ac.cn)
    *   艾华春 (aihuachun22@mails.ucas.ac.cn)
    *   朱夏楠 (zhuxianan22@mails.ucas.ac.cn)
    *   孙广润 (sunguangrun22@mails.ucas.ac.cn)
    *   许晋升 (xujinsheng22@mails.ucas.ac.cn)
*   **Classroom:** 学园二204 (机房)

---

## Course Objectives

The primary goals of this course are to:
*   Apply theoretical knowledge from the Operating Systems theory course in a practical setting.
*   Develop system programming capabilities, including hardware-software co-design.
*   Establish a comprehensive, full-stack perspective of computer systems.

---

## Course Content

The main focus of this course is to build an operating system kernel from the ground up. The projects will cover the following key areas:

1.  **Bootloader:** The initial program that loads the kernel.
2.  **Kernel with Multitasking:** Implementing basic multitasking support.
3.  **Process Management and Communication:** Managing processes and enabling inter-process communication, with support for dual cores.
4.  **Virtual Memory Management:** Implementing a virtual memory system.
5.  **Device Drivers:** Writing drivers for hardware devices like the NIC.
6.  **File System:** Building a file system for disk management and file/directory operations.

---

## Development Environment

### Software
*   A VirtualBox virtual machine will be provided.
*   **Operating System:** Ubuntu 24.04
*   Pre-installed RISC-V cross-compiling toolchain.

### Hardware
*   **Core Board:** RISC-V based PYNQ board.
*   **CPU:** Nutshell dual-core (25MHz)
*   **Memory:** 512MB RAM
*   **Storage:** 8/16/32GB SD card
*   **Networking:** 1Gb NIC port

---

## Grading Policy

### Project Grading
*   Each project is worth 100 points.
*   **Design Review:** Mandatory for each project. Failure to participate will cap the project score at a maximum of 60 points.
*   **Code Submission and On-site Check:** This includes a Q&A session where you may be asked to modify your code.

### Core Categories for Each Project
*   **S-Core (Simple Core):** Basic functionalities. (0-70 points)
*   **A-Core (Advanced Core):** More complete functionalities. (71-85 points)
*   **C-Core (Complex Core):** Advanced features. (86-100 points)

### Final Grade Calculation
*   The final grade is a weighted sum of all project scores:
    `Final Grade = Σ (Project_i * Weight_i)`
*   **Project Weights:**
    *   P1: 10%
    *   P2: 20%
    *   P3: 20%
    *   P4: 20%
    *   P5: 10%
    *   P6: 20%
*   **Important:** Failure to complete the S-Core for any project will result in a maximum final course grade of 65.

### Submission Deadlines
*   **S-Core:** No point deduction for late submissions.
*   **A-Core:** A one-week delay is allowed, but the maximum score for a delayed submission is 80.
*   **C-Core:** No delays are permitted.

### Academic Integrity
*   **Plagiarism, including the use of LLMs without understanding, is strictly prohibited.** Any instance of academic dishonesty will result in zero points for the assignment.
*   You must be able to explain every line of code you submit.

---

## Suggestions for Success

*   **Design Before Coding:** Think through the problem and create a design before writing code.
*   **Start Early:** Avoid leaving work until the last minute.
*   **Utilize Design Reviews:** Prepare a preliminary design to discuss with the TAs.
*   **Master Your Tools:** Become proficient with tools like GDB and QEMU for debugging.
*   **Collaborate:** Discuss ideas and problems with your peers.
*   **Use Version Control:** Make incremental commits to your Git repository.
*   **Consult XV6:** The XV6 operating system source code can be a helpful reference.

---

## How to Use This Repo

*   **Go to the Respective Branch**: The code for each project is on a respective branch. You can go to that branch to find the dedicated `README.md` and `development_note.md` for that project.
*   **The Main Branch**: Code for project 6 is currently merged into the main branch. But I suggest you to go to project 6's branch for its code.
