# slick_object_pool - Documentation Guide

## Overview

This document provides a comprehensive guide to the documentation in `slick/object_pool.h`.

## Documentation Style

The code uses **Doxygen-style** documentation comments for automatic API documentation generation.

### Doxygen Tags Used

| Tag | Purpose | Example |
|-----|---------|---------|
| `@file` | File description | File-level overview |
| `@brief` | Short description | One-line summary |
| `@details` | Detailed description | In-depth explanation |
| `@tparam` | Template parameter | Template type requirements |
| `@param` | Function parameter | Parameter description |
| `@return` | Return value | What the function returns |
| `@throws` | Exception specification | When exceptions are thrown |
| `@note` | Important note | Additional information |
| `@warning` | Warning | Critical warnings |
| `@pre` | Precondition | Requirements before calling |
| `@post` | Postcondition | State after calling |
| `@par` | Paragraph heading | Subsection headers |
| `@code/@endcode` | Code example | Inline code examples |
| `@section` | Documentation section | Major sections |
| `///` | Inline comment | Member variable docs |

## Documentation Coverage

### File-Level Documentation

Includes:
- Brief description of the library
- Detailed feature list
- Memory layout diagram
- Type requirements
- Thread safety guarantees
- Usage examples
- Author and version information

### Class Documentation

**ObjectPool Template Class**

Comprehensive documentation covering:
- Purpose and design
- Template type requirements
- Thread safety model
- Performance characteristics
- Example usage patterns

### Public API Documentation

#### Constructors

1. **ObjectPool(uint32_t size)**
   - Full parameter documentation
   - Exception specifications
   - Preconditions and postconditions
   - Usage examples

#### Destructor

- Cleanup behavior documentation
- Important warnings

#### Public Methods

1. **uint32_t size() const**
   - Pool size query
   - Power-of-2 note

2. **T* allocate()**
   - Comprehensive documentation
   - Behavior under different conditions
   - Performance characteristics
   - Thread safety guarantees
   - Usage examples
   - Important warnings

3. **void free(T* obj)**
   - Dual behavior (pool vs heap)
   - Automatic detection mechanism
   - Performance notes
   - Critical warnings

4. **void reset()**
   - Purpose and use cases
   - Thread safety warnings
   - Testing vs production use

### Private Implementation Documentation

#### Internal Methods

1. **uint64_t reserve(uint32_t n)**
   - Lock-free reservation mechanism
   - Ring buffer wrapping
   - Exception specification

2. **operator[](uint64_t index)**
   - Array access semantics
   - Both const and non-const versions

3. **void publish(uint64_t index, uint32_t n)**
   - Synchronization mechanism
   - Memory ordering notes

4. **std::pair<T*, uint32_t> consume()**
   - Consumption logic
   - Lock-free guarantees
   - Return value meaning

#### Member Variables

All member variables are documented with inline comments (`///`):

- **Configuration:** `size_`, `mask_`
- **Storage:** `buffer_`, `lower_bound_`, `upper_bound_`, `free_objects_`
- **Control:** `control_`, `reserved_`, `consumed_`

#### Internal Structures

1. **struct slot**
   - Purpose and usage
   - Member documentation

2. **struct reserved_info**
   - Producer coordination
   - Field meanings

## Generating API Documentation

### Using Doxygen

1. **Install Doxygen:**
   ```bash
   # Ubuntu/Debian
   sudo apt-get install doxygen graphviz

   # macOS
   brew install doxygen graphviz

   # Windows
   # Download from https://www.doxygen.nl/download.html
   ```

2. **Create Doxyfile:**
   ```bash
   cd d:\repo\slick_object_pool
   doxygen -g
   ```

3. **Configure Doxyfile:**
   ```
   PROJECT_NAME           = "slick_object_pool"
   PROJECT_BRIEF          = "Lock-free object pool for C++20"
   PROJECT_VERSION        = "0.1.0"
   OUTPUT_DIRECTORY       = docs
   INPUT                  = include/slick
   RECURSIVE              = YES
   EXTRACT_ALL            = YES
   EXTRACT_PRIVATE        = NO
   EXTRACT_STATIC         = YES
   GENERATE_HTML          = YES
   GENERATE_LATEX         = NO
   SOURCE_BROWSER         = YES
   INLINE_SOURCES         = YES
   HAVE_DOT               = YES
   CALL_GRAPH             = YES
   CALLER_GRAPH           = YES
   ```

4. **Generate Documentation:**
   ```bash
   doxygen Doxyfile
   ```

5. **View Documentation:**
   ```bash
   # Open in browser
   open docs/html/index.html  # macOS
   xdg-open docs/html/index.html  # Linux
   start docs/html/index.html  # Windows
   ```

## Documentation Best Practices

### What's Documented

✅ **All public APIs** - Complete parameter and return value docs
✅ **Examples** - Working code examples for common use cases
✅ **Thread safety** - Explicit guarantees and warnings
✅ **Performance** - Expected performance characteristics
✅ **Exceptions** - What exceptions are thrown and when
✅ **Preconditions** - Requirements before calling
✅ **Postconditions** - Expected state after calling
✅ **Warnings** - Critical usage warnings
✅ **Platform notes** - Platform-specific behavior

### Documentation Quality

| Aspect | Coverage |
|--------|----------|
| Public API | 100% |
| Private methods | 100% |
| Member variables | 100% |
| Code examples | Extensive |
| Thread safety | Explicit |
| Performance notes | Detailed |
| Platform specifics | Complete |

## Reading the Documentation

### For Users (Public API)

Focus on:
- Constructor documentation
- `allocate()` and `free()` methods
- Thread safety guarantees
- Code examples
- Warnings and notes

### For Contributors (Implementation)

Additionally review:
- Private method documentation
- Internal structure documentation
- Memory layout details
- Platform-specific implementations

### For Library Maintainers

Full documentation including:
- Implementation algorithms
- Memory ordering semantics
- Platform-specific details
- Performance characteristics

## Documentation Examples

### Well-Documented Function

```cpp
/**
 * @brief Allocate an object from the pool
 *
 * @details
 * Attempts to allocate an object from the pool using lock-free operations.
 * If the pool is exhausted, allocates a new object from the heap instead.
 *
 * This method is thread-safe and lock-free for multiple concurrent callers.
 *
 * @return Pointer to an object (never returns nullptr)
 *
 * @par Behavior
 * - Pool has available objects: Returns pooled object (fast path)
 * - Pool is exhausted: Allocates from heap (slower fallback)
 *
 * @par Performance
 * - Average: O(1) with low latency (~10-30ns typical)
 * - Worst case: O(number of concurrent allocators) due to CAS retry
 *
 * @par Thread Safety
 * Lock-free, safe for concurrent calls from multiple threads
 *
 * @par Example
 * @code
 * slick::ObjectPool<MyStruct> pool(1024);
 * MyStruct* obj = pool.allocate();
 * obj->field = value;
 * pool.free(obj);
 * @endcode
 *
 * @note Must be paired with free() to avoid memory leaks
 * @note Returned object is NOT automatically initialized
 */
T* allocate();
```

### Key Elements

1. **Brief** - One line summary
2. **Details** - In-depth explanation
3. **Return** - What it returns
4. **Behavior** - Different scenarios
5. **Performance** - Expected performance
6. **Thread Safety** - Concurrency guarantees
7. **Example** - Working code
8. **Notes** - Important information

## Updating Documentation

### When to Update

- Adding new public methods
- Changing method signatures
- Modifying behavior
- Adding new features
- Fixing bugs that change semantics
- Performance improvements

### Documentation Checklist

When adding/modifying code:

- [ ] Add/update `@brief` description
- [ ] Add/update `@details` if complex
- [ ] Document all parameters with `@param`
- [ ] Document return value with `@return`
- [ ] List exceptions with `@throws`
- [ ] Add thread safety notes
- [ ] Include performance characteristics
- [ ] Add code example if helpful
- [ ] Add warnings if needed
- [ ] Update file-level docs if major change

## Related Documentation

- **README.md** - User-facing documentation and examples
- **CHANGES.md** - Migration guide and changelog
- **Comments in code** - Implementation notes (not in public API docs)

---

**Documentation Version:** 1.0
**Last Updated:** 2025
**Maintained By:** SlickQuant
