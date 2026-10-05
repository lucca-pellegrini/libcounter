# libcounter build tasks. Run `just` to list them.
#
# ESP-IDF recipes forward their arguments to idf.py, so
#   just flash monitor        ==  idf.py flash monitor
#   just build flash monitor  ==  idf.py build flash monitor
#   just flash -p /dev/ttyACM0
# The ESP-IDF environment (esp-idf/export.sh) is sourced automatically when
# the calling shell has not done it already; see scripts/idf.sh.

set positional-arguments

idf := "scripts/idf.sh"
case_dir := "build/case"

[private]
default:
    @just --list

# ---- ESP-IDF ---------------------------------------------------------------

# Run any idf.py command: `just idf <args...>`
idf *args:
    {{ idf }} "$@"

# Build the project
build *args:
    {{ idf }} build "$@"

# Build the app only
app *args:
    {{ idf }} app "$@"

# Flash everything (builds first if needed)
flash *args:
    {{ idf }} flash "$@"

# Flash the app only
app-flash *args:
    {{ idf }} app-flash "$@"

# Flash the bootloader only
bootloader-flash *args:
    {{ idf }} bootloader-flash "$@"

# Flash the partition table only
partition-table-flash *args:
    {{ idf }} partition-table-flash "$@"

# Serial monitor
monitor *args:
    {{ idf }} monitor "$@"

# Configuration menu
menuconfig *args:
    {{ idf }} menuconfig "$@"

# Select the target chip, e.g. `just set-target esp32s3`
set-target *args:
    {{ idf }} set-target "$@"

# Re-run CMake
reconfigure *args:
    {{ idf }} reconfigure "$@"

# Build only the bootloader
bootloader *args:
    {{ idf }} bootloader "$@"

# Build only the partition table
partition-table *args:
    {{ idf }} partition-table "$@"

# Size summary
size *args:
    {{ idf }} size "$@"

# Size per component
size-components *args:
    {{ idf }} size-components "$@"

# Size per source file
size-files *args:
    {{ idf }} size-files "$@"

# Erase the whole flash
erase-flash *args:
    {{ idf }} erase-flash "$@"

# Save a minimal sdkconfig.defaults-style config
save-defconfig *args:
    {{ idf }} save-defconfig "$@"

# Show eFuse summary
efuse-summary *args:
    {{ idf }} efuse-summary "$@"

# Start OpenOCD
openocd *args:
    {{ idf }} openocd "$@"

# Start GDB
gdb *args:
    {{ idf }} gdb "$@"

# Delete build products (keeps CMake cache)
clean *args:
    {{ idf }} clean "$@"

# Delete the whole build directory (also removes build/case!)
fullclean *args:
    {{ idf }} fullclean "$@"

# ---- Case (OpenSCAD) -------------------------------------------------------

# Export the case: build/case/{base,lid}.stl and both parts on one plate in case.3mf
case: case-base case-lid case-3mf

# Export the case body to STL
case-base:
    @mkdir -p {{ case_dir }}
    openscad -q -o {{ case_dir }}/base.stl -D 'part="base"' case/case.scad

# Export the sliding lid to STL
case-lid:
    @mkdir -p {{ case_dir }}
    openscad -q -o {{ case_dir }}/lid.stl -D 'part="lid"' case/case.scad

# Export both parts, laid out side by side, to a single 3MF
case-3mf:
    @mkdir -p {{ case_dir }}
    openscad -q -o {{ case_dir }}/case.3mf -D 'part="plate"' case/case.scad

# Remove exported case files
case-clean:
    rm -rf {{ case_dir }}
