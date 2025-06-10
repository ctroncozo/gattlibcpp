# 🔗 Gattlib C++

<div align="center">

[![License: LGPL v3](https://img.shields.io/badge/License-LGPL%20v3-blue.svg)](https://www.gnu.org/licenses/lgpl-3.0)
[![Platform](https://img.shields.io/badge/platform-linux-lightgrey)](https://github.com/labapart/gattlib)
[![C++](https://img.shields.io/badge/C++-17-blue.svg)](https://en.cppreference.com/w/cpp/17)

**A modern C++ wrapper for BlueZ's GATT library**
</div>

## 📖 Overview (work in progress)

This project is fork of the original C implementation [labapart/gattlib](https://github.com/labapart/gattlib). Implements modern C++ wrapper to Bluetooth Low Energy (BLE) library on Linux. Built on top of BlueZ and Dbus, it provides a safe, efficient, and idiomatic C++ interface for BLE operations.

## ✨ Features

### 🔄 Modern C++ Design
- **RAII-compliant** resource management
- **Smart pointer** usage throughout
- **Exception-safe** error handling
- **PIMPL idiom** for ABI stability

### 🛠️ Core Functionality
- **✅ BLE Scanner**
  - Asynchronous device discovery
  - MAC address filtering
  - Signal-based abort mechanism
- **🔄 Connection Manager** (In Progress)
  - GATT operations
  - Service discovery
  - Characteristic read/write

### 🧪 Quality Assurance
- **Comprehensive test suite**
  - Unit tests with Google Test
  - Mock BLE adapter support
  - CI/CD integration ready
- **Memory safety** improvements
  - Fixed memory leaks in C core
  - RAII-based cleanup

## 🚀 Getting Started

### Prerequisites
- Linux system with BlueZ
- CMake 3.16 or higher
- C++17 compiler
- Development packages:
  ```bash
  sudo apt install libglib2.0-dev libbluetooth-dev
  ```

### Build Instructions

1. **Clone the Repository**
   ```bash
   git clone https://github.com/your-username/gattlib.git
   cd gattlib
   ```

2. **Create Build Directory**
   ```bash
   mkdir build && cd build
   ```

3. **Configure and Build**
   ```bash
   cmake ..
   make -j$(nproc)
   ```

4. **Run Tests** (Optional)
   ```bash
   ctest --output-on-failure
   ```

## 📚 Documentation

### Example Usage
```cpp
#include <gattlib_scanner.hpp>

int main() {
    try {
        // Create scanner instance
        blecpp::GattlibScanner scanner;
        
        // Scan for devices (5 second timeout)
        scanner.scan(5);
        
        // Or scan for specific device
        scanner.scan(5, "00:11:22:33:44:55");
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
```

## 🤝 Contributing

Contributions are welcome! Please feel free to submit a Pull Request. For major changes, please open an issue first to discuss what you would like to change.

## 📄 License

This project is licensed under the LGPL-3.0 License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- Original [gattlib](https://github.com/labapart/gattlib) developers
- BlueZ project team
- All contributors to this modern C++ fork

## 📁 Project Structure

```
gattlib/
├── cpp/             # Modern C++ wrapper for BLE APIs
│   ├── include/
│   ├── src/
│   └── tests/
├── bluez/           # BlueZ-related backend logic
├── dbus/            # D-Bus integration for BlueZ v5
├── examples/        # Example programs
├── gattlib-py/      # Python bindings
└── CMakeLists.txt   # Build configuration
```

## ⚖️ Why This Fork?

The goal of this fork is to provide:
- A modern, idiomatic C++ API for BLE development
- Cleaner, safer abstractions over the C-based `gattlib`
- Easier integration in modern CMake-based C++ projects
- Fix memory leaks and improve maintainability of the original C core

I hope this helps developers who want the power of `gattlib` in a more modern form.

## 📫 Contact
Cristian Troncoso [Linkedin](www.linkedin.com/in/cristian-troncoso-05563b139)


---

> “Built on the shoulders of giants.” — Thanks to the original `gattlib` authors.
