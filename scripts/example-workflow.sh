#!/bin/bash

# Example workflow for using the version update script
# This demonstrates a typical release process
#
# Usage Examples:
#   ./scripts/example-workflow.sh patch                    # Increment patch version (1.0.0 -> 1.0.1)
#   ./scripts/example-workflow.sh minor                    # Increment minor version (1.0.0 -> 1.1.0)
#   ./scripts/example-workflow.sh major                    # Increment major version (1.0.0 -> 2.0.0)
#   ./scripts/example-workflow.sh custom 1.0.0-alpha.1     # Use custom version
#   ./scripts/example-workflow.sh --help                   # Show help message
#
# Workflow Steps:
#   1. Check git status
#   2. Show current version
#   3. Dry run to preview changes
#   4. Ask for confirmation
#   5. Perform version update
#   6. Show final status
#
# Note: This is a demonstration script for learning purposes

set -e

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

# Example workflow function
example_workflow() {
    local version_type=$1
    local new_version=$2
    
    print_info "Starting example workflow for $version_type release"
    echo
    
    # Step 1: Check current status
    print_info "Step 1: Checking current git status"
    git status --short
    echo
    
    # Step 2: Show current version
    print_info "Step 2: Current version"
    ./scripts/update-version.sh --help | head -1 > /dev/null 2>&1 || {
        print_warning "Current version: $(grep '^PKG_VERSION' Makefile | sed 's/.*:= *//')"
    }
    echo
    
    # Step 3: Dry run to see what would change
    print_info "Step 3: Dry run to preview changes"
    ./scripts/update-version.sh "$new_version" --dry-run --force
    echo
    
    # Step 4: Ask for confirmation
    print_warning "Step 4: Confirmation required"
    read -p "Do you want to proceed with the version update? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        print_info "Version update cancelled"
        return 0
    fi
    
    # Step 5: Perform the actual update
    print_info "Step 5: Performing version update"
    ./scripts/update-version.sh "$new_version" --force
    echo
    
    # Step 6: Show final status
    print_info "Step 6: Final git status"
    git status --short
    echo
    
    print_success "Workflow completed successfully!"
}

# Show usage
show_usage() {
    cat << EOF
Example Workflow Script for gpio-fan-rpm

This script demonstrates how to use the version update script in a typical workflow.

Usage: $0 <version_type> [version]

VERSION TYPES:
    patch     Increment patch version (e.g., 1.0.0 -> 1.0.1)
    minor     Increment minor version (e.g., 1.0.0 -> 1.1.0)
    major     Increment major version (e.g., 1.0.0 -> 2.0.0)
    custom    Use custom version (requires version parameter)

EXAMPLES:
    $0 patch                    # Increment patch version
    $0 minor                    # Increment minor version
    $0 major                    # Increment major version
    $0 custom 1.0.0-alpha.1     # Use custom version

NOTES:
    - This is a demonstration script
    - Use --dry-run first to preview changes
    - Always test your changes before releasing
    - Consider creating a GitHub release after tagging

EOF
}

# Main script
main() {
    if [ $# -eq 0 ]; then
        show_usage
        exit 1
    fi
    
    local version_type=$1
    local custom_version=$2
    
    case $version_type in
        patch|minor|major)
            if [ -n "$custom_version" ]; then
                print_warning "Ignoring custom version '$custom_version' for $version_type type"
            fi
            # For demo purposes, we'll use a fixed current version
            local current_version="1.0.0"
            local new_version=""
            
            case $version_type in
                patch)
                    new_version="1.0.1"
                    ;;
                minor)
                    new_version="1.1.0"
                    ;;
                major)
                    new_version="2.0.0"
                    ;;
            esac
            
            example_workflow "$version_type" "$new_version"
            ;;
        custom)
            if [ -z "$custom_version" ]; then
                print_error "Custom version type requires a version parameter"
                show_usage
                exit 1
            fi
            example_workflow "custom" "$custom_version"
            ;;
        -h|--help)
            show_usage
            exit 0
            ;;
        *)
            print_error "Unknown version type: $version_type"
            show_usage
            exit 1
            ;;
    esac
}

# Run main function
main "$@" 