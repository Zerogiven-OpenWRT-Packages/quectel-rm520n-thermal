#!/bin/bash

# Version Update Script for gpio-fan-rpm
# This script updates the version in Makefile, adds changelog entries, and creates a git tag
#
# Usage Examples:
#   ./scripts/update-version.sh 1.0.1                    # Basic version update
#   ./scripts/update-version.sh 2.0.0 --message "Major release"  # Custom commit message
#   ./scripts/update-version.sh 1.1.0 --dry-run          # Preview changes without applying
#   ./scripts/update-version.sh 1.0.2 --force            # Force update with uncommitted changes
#   ./scripts/update-version.sh 1.0.3 --tag-only         # Only create git tag
#   ./scripts/update-version.sh 1.0.0-alpha.1            # Pre-release version
#   ./scripts/update-version.sh 1.0.0-rc.1               # Release candidate
#
# Version Format: Must follow semantic versioning (e.g., 1.0.0, 2.0.0-alpha.1, 1.0.0+build.1)

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to show usage
show_usage() {
    cat << EOF
Usage: $0 [OPTIONS] <new_version>

Update version for gpio-fan-rpm project.

OPTIONS:
    -h, --help          Show this help message
    -d, --dry-run       Show what would be done without making changes
    -m, --message       Custom commit message (default: "Release version <version>")
    -t, --tag-only      Only create git tag, don't update files
    -f, --force         Force update even if working directory is not clean

ARGUMENTS:
    new_version         New version in semver format (e.g., 1.0.1, 2.0.0-alpha.1)

EXAMPLES:
    $0 1.0.1                    # Update to version 1.0.1
    $0 2.0.0 --message "Major release"  # Update with custom message
    $0 1.1.0 --dry-run          # Show what would be changed
    $0 1.0.2 --tag-only         # Only create git tag

EOF
}

# Function to validate version format
validate_version() {
    local version=$1
    if [[ ! $version =~ ^[0-9]+\.[0-9]+\.[0-9]+(-[a-zA-Z0-9.-]+)?(\+[a-zA-Z0-9.-]+)?$ ]]; then
        print_error "Invalid version format: $version"
        print_error "Version must follow semantic versioning (e.g., 1.0.0, 2.0.0-alpha.1)"
        exit 1
    fi
}

# Function to get current version from Makefile
get_current_version() {
    local version_line=$(grep "^PKG_VERSION" Makefile)
    echo "$version_line" | sed 's/.*:= *//'
}

# Function to update version in Makefile
update_makefile_version() {
    local new_version=$1
    local dry_run=$2
    
    print_info "Updating version in Makefile from $(get_current_version) to $new_version"
    
    if [ "$dry_run" = "true" ]; then
        echo "Would update Makefile: PKG_VERSION := $new_version"
    else
        # Update PKG_VERSION line
        sed -i.bak "s/^PKG_VERSION.*:=.*/PKG_VERSION := $new_version/" Makefile
        rm -f Makefile.bak
        print_success "Updated Makefile version to $new_version"
    fi
}

# Function to update CHANGELOG.md
update_changelog() {
    local new_version=$1
    local dry_run=$2
    
    print_info "Updating CHANGELOG.md"
    
    # Get current date
    local current_date=$(date +%Y-%m-%d)
    
    # Create changelog entry
    local changelog_entry="## [$new_version] - $current_date

### Added
- Version $new_version release

### Changed
- Updated version to $new_version

### Fixed
- None

### Removed
- None

"
    
    if [ "$dry_run" = "true" ]; then
        echo "Would add to CHANGELOG.md:"
        echo "$changelog_entry"
    else
        # Create a temporary file with the new content
        {
            # Add the header (first 4 lines)
            head -8 CHANGELOG.md
            
            # Add the new version entry
            echo "$changelog_entry"
            
            # Add the rest of the content (skip the first 4 lines)
            tail -n +9 CHANGELOG.md
        } > CHANGELOG.md.tmp && mv CHANGELOG.md.tmp CHANGELOG.md
        
        print_success "Updated CHANGELOG.md with version $new_version"
    fi
}

# Function to check if git repository is clean
check_git_status() {
    if ! git rev-parse --git-dir > /dev/null 2>&1; then
        print_error "Not in a git repository"
        exit 1
    fi
    
    if [ "$FORCE" != "true" ] && [ -n "$(git status --porcelain)" ]; then
        print_error "Working directory is not clean. Please commit or stash changes first."
        print_error "Use --force to override this check."
        git status --short
        exit 1
    fi
}

# Function to create git commit and tag
create_git_release() {
    local new_version=$1
    local commit_message=$2
    local dry_run=$3
    
    print_info "Creating git commit and tag for version $new_version"
    
    if [ "$dry_run" = "true" ]; then
        echo "Would run:"
        echo "  git add Makefile CHANGELOG.md"
        echo "  git commit -m \"$commit_message\""
        echo "  git tag -a v$new_version -m \"Release version $new_version\""
        echo "  git push origin main"
        echo "  git push origin v$new_version"
    else
        # Add modified files
        git add Makefile CHANGELOG.md
        
        # Create commit
        git commit -m "$commit_message"
        print_success "Created commit: $commit_message"
        
        # Create annotated tag
        git tag -a "v$new_version" -m "Release version $new_version"
        print_success "Created tag: v$new_version"
        
        # Push changes
        print_info "Pushing changes to remote repository..."
        git push origin main
        git push origin "v$new_version"
        print_success "Pushed changes and tag to remote repository"
    fi
}

# Function to show summary
show_summary() {
    local new_version=$1
    local dry_run=$2
    
    print_success "Version update completed!"
    echo
    echo "Summary:"
    echo "  - New version: $new_version"
    echo "  - Previous version: $(get_current_version)"
    
    if [ "$dry_run" = "true" ]; then
        echo "  - Mode: Dry run (no changes made)"
    else
        echo "  - Files updated: Makefile, CHANGELOG.md"
        echo "  - Git: Commit and tag created"
    fi
    
    echo
    echo "Next steps:"
    echo "  1. Review the changes"
    echo "  2. Test the build with the new version"
    echo "  3. Create a GitHub release if needed"
    echo
}

# Main script
main() {
    # Parse command line arguments
    DRY_RUN=false
    TAG_ONLY=false
    FORCE=false
    COMMIT_MESSAGE=""
    NEW_VERSION=""
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                show_usage
                exit 0
                ;;
            -d|--dry-run)
                DRY_RUN=true
                shift
                ;;
            -t|--tag-only)
                TAG_ONLY=true
                shift
                ;;
            -f|--force)
                FORCE=true
                shift
                ;;
            -m|--message)
                COMMIT_MESSAGE="$2"
                shift 2
                ;;
            -*)
                print_error "Unknown option: $1"
                show_usage
                exit 1
                ;;
            *)
                if [ -z "$NEW_VERSION" ]; then
                    NEW_VERSION="$1"
                else
                    print_error "Multiple versions specified: $NEW_VERSION and $1"
                    exit 1
                fi
                shift
                ;;
        esac
    done
    
    # Check if version was provided
    if [ -z "$NEW_VERSION" ]; then
        print_error "No version specified"
        show_usage
        exit 1
    fi
    
    # Validate version format
    validate_version "$NEW_VERSION"
    
    # Set default commit message if not provided
    if [ -z "$COMMIT_MESSAGE" ]; then
        COMMIT_MESSAGE="Release version $NEW_VERSION"
    fi
    
    # Get current version
    CURRENT_VERSION=$(get_current_version)
    
    # Check if version is actually changing
    if [ "$CURRENT_VERSION" = "$NEW_VERSION" ]; then
        print_warning "Version is already $NEW_VERSION"
        if [ "$FORCE" != "true" ]; then
            print_error "Use --force to update anyway"
            exit 1
        fi
    fi
    
    print_info "Starting version update process"
    print_info "Current version: $CURRENT_VERSION"
    print_info "New version: $NEW_VERSION"
    echo
    
    # Check git status (unless tag-only mode)
    if [ "$TAG_ONLY" != "true" ]; then
        check_git_status
    fi
    
    # Update files (unless tag-only mode)
    if [ "$TAG_ONLY" != "true" ]; then
        update_makefile_version "$NEW_VERSION" "$DRY_RUN"
        update_changelog "$NEW_VERSION" "$DRY_RUN"
    fi
    
    # Create git commit and tag
    create_git_release "$NEW_VERSION" "$COMMIT_MESSAGE" "$DRY_RUN"
    
    # Show summary
    show_summary "$NEW_VERSION" "$DRY_RUN"
}

# Run main function with all arguments
main "$@" 