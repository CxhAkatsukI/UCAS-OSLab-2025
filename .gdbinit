# ================================================================
#  GDB initialization script for Project 3
# ================================================================

# --- Configuration ---

# Stop GDB from asking for confirmation (y/n) for commands.
# This makes the script fully automatic.
set confirm off

# Tell GDB where to find the source files (.c files).
# This helps it resolve breakpoints correctly. Adjust the path if needed.
# directory test/test_project3/

# --- Symbol Loading ---

echo "Loading symbol files for child processes...\n"

# Load symbol files for the executables that will be loaded by the OS.
# These paths should be relative to where you launch GDB.
# Note: 'build/wait_for_lock' from your log was omitted as it resulted
# in a "No such file or directory" error.
# add-symbol-file build/ready_to_exit
# add-symbol-file build/wait_locks

# --- Breakpoints ---

echo "Setting breakpoints...\n"

# Set breakpoints in the code of the child processes.
# These will be "pending" until the OS loads these programs into memory, which is expected.
b do_scheduler
b exception_handler_entry

# --- Finalization ---

# Display a confirmation message and list the set breakpoints
# so you know the script ran successfully.
echo "\n--- .gdbinit setup complete ---\n"
info breakpoints
echo "--------------------------------\n"
