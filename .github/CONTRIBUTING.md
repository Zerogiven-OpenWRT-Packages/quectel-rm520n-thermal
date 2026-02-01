# Contributing to Quectel RM520N Thermal Management Tools

Thank you for your interest in contributing to this project! This document provides guidelines and information for contributors.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Setup](#development-setup)
- [Making Changes](#making-changes)
- [Coding Standards](#coding-standards)
- [Commit Guidelines](#commit-guidelines)
- [Pull Request Process](#pull-request-process)
- [Testing](#testing)
- [Reporting Issues](#reporting-issues)

## Code of Conduct

Please be respectful and constructive in all interactions. We welcome contributors of all experience levels.

## Getting Started

1. Fork the repository
2. Clone your fork locally
3. Set up the development environment (see below)
4. Create a new branch for your changes
5. Make your changes and test them
6. Submit a pull request

## Development Setup

### Prerequisites

**For userspace tool development:**
```bash
# Ubuntu/Debian
sudo apt-get install build-essential libuci-dev libsysfs-dev libubox-dev

# Alpine Linux
apk add build-base uci-dev sysfs-utils-dev libubox-dev
```

**For kernel module development:**
- Linux kernel headers matching your target system
- OpenWRT SDK (for cross-compilation)

## Making Changes

### Repository Structure

```
quectel-rm520n-thermal/
├── src/                    # Source code
│   ├── include/            # Header files
│   ├── kmod/               # Kernel modules
│   ├── main.c              # Entry point
│   ├── daemon.c            # Daemon mode
│   ├── cli.c               # CLI mode
│   ├── config.c            # Configuration management
│   ├── serial.c            # Serial communication
│   ├── temperature.c       # Temperature parsing
│   ├── system.c            # System utilities
│   ├── uci_config.c        # UCI integration
│   ├── ui.c                # Help and version display
│   └── logging.c           # Logging wrapper
├── files/                  # OpenWRT package files
│   ├── quectel_rm520n_thermal         # UCI config
│   ├── quectel_rm520n_thermal.init    # Init script
│   └── quectel_rm520n_thermal.lua     # Prometheus collector
├── Makefile                # OpenWRT package Makefile
└── README.md               # Documentation
```

### Key Components

- **Kernel Modules** (`src/kmod/`): Three modules for sysfs, thermal sensors, and hwmon
- **Userspace Tool** (`src/`): Combined daemon and CLI binary
- **Configuration** (`files/`): UCI config, init script, Prometheus collector

## Coding Standards

### C Standards

| Component | Standard | Compiler Flag |
|-----------|----------|---------------|
| Userspace code | GNU17 | `-std=gnu17` |
| Kernel modules | GNU11 | (kernel default) |

### Userspace Code Style

- Use 4-space indentation
- Keep lines under 100 characters
- Use descriptive variable and function names
- Use `<stdbool.h>` for `bool`, `true`, `false`
- Prefer designated initializers: `{ .field = value }`

### Kernel Module Code Style

Follow the [Linux Kernel Coding Style](https://www.kernel.org/doc/html/latest/process/coding-style.html):

- **Indentation**: Tabs (8-character width), not spaces
- **Brace placement**: K&R style
  - Functions: opening brace on new line
  - Control structures: opening brace on same line
- **Line length**: 80 characters preferred, 100 max
- **Naming**: Lowercase with underscores, short local variables
- **No typedefs** for structs (use `struct name` directly)
- **Comments**: Explain "what", not "how"
- **Error handling**: Use `goto` for cleanup paths

### Security Guidelines

- Always validate user input
- Use `SAFE_STRNCPY` macro for safe string copying
- Check return values of system calls
- Avoid `system()` and `popen()` - use library APIs instead
- Validate serial port paths (must start with `/dev/`, no path traversal)

### Memory Safety

- Check buffer sizes before copying
- Use `snprintf()` instead of `sprintf()`
- Always null-terminate strings
- Free allocated memory appropriately

### Example: Userspace Code Style

```c
/**
 * function_name - Brief description
 * @param1: Description of first parameter
 * @param2: Description of second parameter
 *
 * Return: Description of return value
 */
int function_name(const char *param1, int param2)
{
    int result;
    char buffer[256];

    /* Validate input */
    if (!param1) {
        return -1;
    }

    /* Process data */
    if (snprintf(buffer, sizeof(buffer), "%s", param1) >= sizeof(buffer)) {
        logging_warning("Buffer truncated");
        return -1;
    }

    return 0;
}
```

### Example: Kernel Module Code Style

```c
/**
 * quectel_temp_show - Read temperature from sysfs
 * @kobj: Kernel object pointer
 * @attr: Kernel object attribute
 * @buf: Output buffer
 *
 * Return: Number of bytes written to buffer
 */
static ssize_t quectel_temp_show(struct kobject *kobj,
				 struct kobj_attribute *attr, char *buf)
{
	int temp;

	if (!buf)
		return -EINVAL;

	mutex_lock(&temp_lock);
	temp = modem_temp;
	mutex_unlock(&temp_lock);

	return sysfs_emit(buf, "%d\n", temp);
}
```

## Commit Guidelines

We use [Conventional Commits](https://www.conventionalcommits.org/) for commit messages:

### Format

```
<type>(<scope>): <description>

[optional body]

[optional footer]
```

### Types

- `feat`: New feature
- `fix`: Bug fix
- `refactor`: Code refactoring (no functional change)
- `docs`: Documentation changes
- `style`: Code style changes (formatting, whitespace)
- `test`: Adding or updating tests
- `chore`: Build process, dependencies, tooling
- `security`: Security-related changes

### Scopes

- `daemon`: Daemon mode functionality
- `cli`: CLI mode functionality
- `kmod`: Kernel modules
- `config`: Configuration handling
- `serial`: Serial communication
- `system`: System utilities
- `uci`: UCI integration

### Examples

```
feat(cli): add --watch mode for continuous monitoring

fix(serial): handle fcntl() return values properly

security(config): add serial port path validation

refactor(common): consolidate SAFE_STRNCPY macro
```

## Pull Request Process

1. **Create a descriptive PR title** following the conventional commit format
2. **Fill out the PR description** explaining:
   - What changes were made
   - Why the changes were needed
   - How to test the changes
3. **Ensure all tests pass** (if applicable)
4. **Update documentation** if your changes affect usage
5. **Request review** from maintainers

### PR Checklist

- [ ] Code follows the project's coding standards
- [ ] Commit messages follow conventional commit format
- [ ] Documentation updated (if applicable)
- [ ] No security vulnerabilities introduced
- [ ] Tested on actual hardware (if possible)

## Testing

### Local Testing

```bash
# Build and run help
cd src && make && ./quectel_rm520n_temp --help

# Test with debug output
./quectel_rm520n_temp --debug read
```

### Testing on OpenWRT Device

```bash
# Transfer binary to device
scp quectel_rm520n_temp root@router:/usr/bin/

# Test on device
ssh root@router "quectel_rm520n_temp --version"
ssh root@router "quectel_rm520n_temp status"
ssh root@router "quectel_rm520n_temp --celsius"
```

### Test Checklist

- [ ] `--version` displays correct version
- [ ] `--help` shows usage information
- [ ] `read` command returns temperature
- [ ] `--celsius` returns temperature in degrees
- [ ] `--json` returns valid JSON
- [ ] `status` shows daemon status
- [ ] `config` updates kernel module thresholds
- [ ] Daemon starts and runs without errors

## Reporting Issues

When reporting issues, please include:

1. **Description**: Clear description of the problem
2. **Steps to reproduce**: How to trigger the issue
3. **Expected behavior**: What should happen
4. **Actual behavior**: What actually happens
5. **Environment**:
   - OpenWRT version
   - Kernel version
   - Modem model
   - Package version (`quectel_rm520n_temp --version`)
6. **Logs**: Relevant log output (`logread | grep quectel`)

### Bug Report Template

```markdown
**Description**
A clear description of the bug.

**Steps to Reproduce**
1. Step one
2. Step two
3. Step three

**Expected Behavior**
What you expected to happen.

**Actual Behavior**
What actually happened.

**Environment**
- OpenWRT version:
- Kernel version:
- Modem model:
- Package version:

**Logs**
```
Paste relevant logs here
```
```

## Questions?

If you have questions about contributing, feel free to:
- Open an issue for discussion
- Check existing issues and pull requests for context

Thank you for contributing!
